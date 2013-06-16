/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996 Charles F. Randall <crandall@goldsys.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1996,97,98 Jon Nelson <nels0988@tc.umn.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* boa: boa.c */

#include "boa.h"
#include <grp.h>

int server_s;					/* boa socket */
int backlog = SO_MAXCONN;
struct sockaddr_in server_sockaddr;		/* boa socket address */

struct timeval req_timeout;		/* timeval for select */

extern char *optarg;			/* getopt */

fd_set block_read_fdset;
fd_set block_write_fdset;

int sighup_flag = 0;			/* 1 => signal has happened, needs attention */
int sigchld_flag = 0;			/* 1 => signal has happened, needs attention */
int lame_duck_mode = 0;

int sock_opt = 1;
int do_fork = 1;

int main(int argc, char **argv)
{
	int c;						/* command line arg */
	struct rlimit rl;

	while ((c = getopt(argc, argv, "c:d")) != -1) {
		switch (c) {
		case 'c':
			server_root = strdup(optarg);
			break;
		case 'd':
			do_fork = 0;
			break;
		default:
			fprintf(stderr, "Usage: %s [-c serverroot] [-d]\n", argv[0]);
			exit(1);
		}
	}

	fixup_server_root();

	read_config_files();
	open_logs();
	create_common_env();

	if ((server_s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		die(NO_CREATE_SOCKET);

	/* server socket is nonblocking */
	if (fcntl(server_s, F_SETFL, NOBLOCK) == -1)
		die(NO_FCNTL);

	if ((setsockopt(server_s, SOL_SOCKET, SO_REUSEADDR, (void *) &sock_opt,
					sizeof (sock_opt))) == -1)
		die(NO_SETSOCKOPT);

	/* internet socket */
	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_sockaddr.sin_port = htons(server_port);

	if (bind(server_s, (struct sockaddr *) &server_sockaddr,
			 sizeof (server_sockaddr)) == -1)
		die(NO_BIND);

	/* listen: large number just in case your kernel is nicely tweaked */
	if (listen(server_s, backlog) == -1)
		die(NO_LISTEN);

	init_signals();

	/* background ourself */

	if (do_fork)
		if (fork())
			exit(0);

	/* close server socket on exec 
	 * so cgi's can't write to it */

	if (fcntl(server_s, F_SETFD, 1) == -1) {
		perror("can't set close-on-exec on server socket!");
		exit(0);
	}
	/* close STDIN on exec so cgi's can't read from it */
	if (fcntl(STDIN_FILENO, F_SETFD, 1) == -1) {
		perror("can't set close-on-exec on STDIN!");
		exit(0);
	}
	/* give away our privs if we can */

	if (getuid() == 0) {
		struct passwd *passwdbuf;
		passwdbuf = getpwuid(server_uid);
		if (passwdbuf == NULL)
			die(GETPWUID);
		if (initgroups(passwdbuf->pw_name, passwdbuf->pw_gid) == -1)
			die(INITGROUPS);
		if (setgid(server_gid) == -1)
			die(NO_SETGID);
		if (setuid(server_uid) == -1)
			die(NO_SETUID);
	} else {
		if (server_gid || server_uid) {
			log_error_time();
			fprintf(stderr, "Warning: "
					"Not running as root: no attempt to change"
					" to uid %d gid %d\n",
					server_uid, server_gid);
		}
		server_gid = getgid();
		server_uid = getuid();
	}

	/* main loop */

	timestamp();

	FD_ZERO(&block_read_fdset);
	FD_ZERO(&block_write_fdset);

	status.requests = 0;
	status.errors = 0;

	fdset_update();
	/* set server_s and req_timeout */

	while (1) {
		if (sighup_flag)
			sighup_run();
		if (sigchld_flag)
			sigchld_run();

		if (lame_duck_mode && !request_ready && !request_block)
			die(SHUTDOWN);

		if (!request_ready) {
			if (select(OPEN_MAX, &block_read_fdset,
					   &block_write_fdset, NULL,
					   (request_block ? &req_timeout : NULL)) == -1)
				if (errno == EINTR || errno == EBADF)
					continue;	/* while(1) */
				else
					die(SELECT);

			if (FD_ISSET(server_s, &block_read_fdset))
				get_request();

			fdset_update();
			/* move selected req's from request_block to request_ready */
		}
		process_requests();
		/* any blocked req's move from request_ready to request_block */
	}
}

/*
 * Name: fdset_update
 * 
 * Description: iterate through the blocked requests, checking whether
 * that file descriptor has been set by select.  Update the fd_set to
 * reflect current status.
 */

void fdset_update(void)
{
	request *current, *next;
	time_t current_time;

	current = request_block;

	current_time = time(NULL);

	while (current) {
		time_t time_since;
		next = current->next;

		time_since = current_time - current->time_last;

		/* hmm, what if we are in "the middle" of a request and not
		 * just waiting for a new one... perhaps check to see if anything
		 * has been read via header position, etc... */
		if (current->kacount && (time_since >= ka_timeout) && !current->logline) {
			SQUASH_KA(current);
			current->status = CLOSE;
			free_request(&request_block, current);
		} else if (time_since > REQUEST_TIMEOUT) {
			log_error_doc(current);
			fputs("connection timed out\n", stderr);
			SQUASH_KA(current);
			current->status = CLOSE;
			free_request(&request_block, current);
		} else if (current->buffer_end) {
			if (FD_ISSET(current->fd, &block_write_fdset))
				ready_request(current);
		} else {
			switch (current->status) {
			case PIPE_WRITE:
			case WRITE:
				if (FD_ISSET(current->fd, &block_write_fdset))
					ready_request(current);
				else
					FD_SET(current->fd, &block_write_fdset);
				break;
			case PIPE_READ:
				if (FD_ISSET(current->data_fd, &block_read_fdset))
					ready_request(current);
				else
					FD_SET(current->data_fd, &block_read_fdset);
				break;
			case BODY_WRITE:
				if (FD_ISSET(current->post_data_fd, &block_write_fdset))
					ready_request(current);
				else
					FD_SET(current->post_data_fd, &block_write_fdset);
				break;
			default:
				if (FD_ISSET(current->fd, &block_read_fdset))
					ready_request(current);
				else
					FD_SET(current->fd, &block_read_fdset);
				break;
			}
		}
		current = next;
	}

	if (!lame_duck_mode)
		FD_SET(server_s, &block_read_fdset);	/* server always set */
	req_timeout.tv_sec = (ka_timeout ? ka_timeout : REQUEST_TIMEOUT);
	req_timeout.tv_usec = 0l;	/* reset timeout */
}

/*
 * Name: die
 * Description: die with fatal error
 */

void die(int exit_code)
{
	log_error_time();

	switch (exit_code) {
	case SERVER_ERROR:
		fputs("fatal error: exiting\n", stdout);
		break;
	case OUT_OF_MEMORY:
		perror("malloc");
		break;
	case NO_CREATE_SOCKET:
		perror("socket create");
		break;
	case NO_FCNTL:
		perror("fcntl");
		break;
	case NO_SETSOCKOPT:
		perror("setsockopt");
		break;
	case NO_BIND:
		perror("bind");
		break;
	case NO_LISTEN:
		perror("listen");
		break;
	case NO_SETGID:
		perror("setgid/initgroups");
		break;
	case NO_SETUID:
		perror("setuid");
		break;
	case NO_OPEN_LOG:
		perror("logfile fopen");	/* ??? */
		break;
	case SELECT:
		perror("select");
		break;
	case GETPWUID:
		perror("getpwuid");
		break;
	case INITGROUPS:
		perror("initgroups");
		break;
	case SHUTDOWN:
		fputs("completing shutdown\n", stderr);
		break;
	default:
		break;
	}

	fclose(stderr);
	exit(exit_code);
}
