/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996 Charles F. Randall <crandall@dmacc.cc.ia.us>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@cebaf.gov>
 *  Some changes Copyright (C) 1996 Jon Nelson <nels0988@tc.umn.edu>
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
#include <pwd.h>

int server_s;				/* boa socket */
struct sockaddr_in server_sockaddr;	/* boa socket address */

struct timeval req_timeout;		/* timeval for select */

extern char * optarg;                   /* getopt */

fd_set block_read_fdset;
fd_set block_write_fdset;

int sighup_flag=0;		/* 1 => signal has happened, needs attention */
int sigchld_flag=0;		/* 1 => signal has happened, needs attention */
int lame_duck_mode=0;

int true = 1;

int main(int argc, char **argv) 
{
    int c;                     /* command line arg */
    struct rlimit rl;
    char *dirbuf;
    size_t dirbuf_size;

    while((c = getopt(argc, argv, "c:")) != -1) {
        switch(c) {
            case 'c':
                server_root = strdup(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-c serverroot]\n",argv[0]);
                exit(1);
        }
    }

    if(!server_root) {
        #ifdef SERVER_ROOT
            server_root = strdup(SERVER_ROOT);
        #else
	    fprintf(stderr, 
"Boa: don't know where server root is.  Please #define SERVER_ROOT in boa.h\n"
"and recompile, or use the -c command line option to specify it.\n");
	    exit(1);
        #endif
    }

    if(chdir(server_root) == -1) {
        fprintf(stderr, "Could not chdir to %s: aborting\n", server_root);
        exit(1);
    }

    if ( server_root[0] != '/' ) {
      /* server_root (as specified on the command line) is
       * a relative path name. CGI programs require SERVER_ROOT
       * to be absolute.
       */
        dirbuf_size = MAX_PATH_LENGTH*2+1;
	if ( (dirbuf = (char *) malloc(dirbuf_size)) == NULL ) {
	  fprintf(stderr,
		  "boa: Cannot malloc %d bytes of memory. Aborting.\n",
		  dirbuf_size);
	  exit(1);
	}
	while ( getcwd(dirbuf,dirbuf_size) == NULL ) {
	  if ( errno == ERANGE ) {
	    dirbuf_size += (size_t) MAX_PATH_LENGTH;
	    if ( (dirbuf = (char *) realloc(dirbuf,dirbuf_size)) == NULL ) {
	      fprintf(stderr,
		      "boa: Cannot realloc %d bytes of memory. Aborting.\n",
		      dirbuf_size);
	      exit(1);
	    }
	  }
	  else if ( errno == EACCES ) {
	    fprintf(stderr,"boa: getcwd() failed. "
		    "No read access in current directory. Aborting.\n");
	    exit(1);
	  }
	  else {
	    perror("getcwd");
	    exit(1);
	  }

				 
	}
	fprintf(stderr,
		"boa: Warning, the server_root directory specified"
		" on the command line,\n"
		"%s, is relative. CGI programs expect the environment\n"
		"variable SERVER_ROOT to be an absolute path.\n"
		"Setting SERVER_ROOT to %s to conform.\n",server_root,dirbuf);
	free(server_root);
	server_root = dirbuf;
    }

/* HP-UX doesn't support this */
#ifdef RLIM_INFINITY
    rl.rlim_cur = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &rl);
#endif

    read_config_files();
    open_logs();
    create_common_env();

    log_error_time();
    fprintf(stderr, "boa: starting server\n");

    if((server_s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) 
	die(NO_CREATE_SOCKET);

    /* server socket is nonblocking */
    if(fcntl(server_s, F_SETFL, NOBLOCK) == -1)
	die(NO_FCNTL);

    if((setsockopt(server_s, SOL_SOCKET, SO_REUSEADDR, (void *)&true, 
      sizeof(true))) == -1)
	die(NO_SETSOCKOPT);

    /* internet socket */
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(server_port);
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(server_s, (struct sockaddr *)&server_sockaddr, 
      sizeof(server_sockaddr)) == -1) 
	die(NO_BIND);

    /* listen: large number just in case your kernel is nicely tweaked */
    if(listen(server_s, 100) == -1)
	die(NO_LISTEN);

    init_signals();

    /* give away our privs if we can */

    if(getuid() == 0) {
	struct passwd *passwdbuf;
	passwdbuf = getpwuid(server_uid);
	if(passwdbuf == NULL)
	    die(NO_SETUID);
	if(initgroups(passwdbuf->pw_name,passwdbuf->pw_gid) == -1)
	    die(NO_SETGID);
	if(setgid(server_gid) == -1)
	    die(NO_SETGID);
	if(setuid(server_uid) == -1)
	    die(NO_SETUID);
    } else {
	if (server_gid || server_uid) {
	    log_error_time();
	    fprintf(stderr, "Warning: "
	    "Not running as root: no attempt to change to uid %d gid %d\n",
	    server_uid, server_gid);
	}
	server_gid=getgid();
	server_uid=getuid();
    }

    create_tmp_dir();      /* takes care of exiting and errors all by itself */

    /* background ourself */

/*
    if(fork())
	exit(0);
*/

    /* main loop */

    fdset_update();	/* set server_s and req_timeout */

    while(1) {

	if (sighup_flag) sighup_run();
	if (sigchld_flag) sigchld_run();
	if (lame_duck_mode == 1) lame_duck_mode_run();

	if (!request_ready && !request_block && lame_duck_mode) {
	    log_error_time();
            fprintf(stderr, "completing shutdown\n");
            exit(15);
	}

	if(!request_ready) {
	    if(select(OPEN_MAX, &block_read_fdset, &block_write_fdset, NULL, 
	      (request_block ? &req_timeout : NULL)) == -1)
		continue;

	    if(FD_ISSET(server_s, &block_read_fdset))
		get_request();
	    fdset_update();  /* move selected req's from request_block to request_ready */
	}

	process_requests(); /* any blocked req's move from request_ready to request_block */
    }
}

/*
 * Name: fdset_update
 * 
 * Description: iterate through the blocked requests, checking whether
 * that file descriptor has been set by select.  Update the fd_set to
 * reflect current status.
 */

void fdset_update()
{
    request * current, * next;

    current = request_block;

    while(current) {
	next = current->next;

	if (lame_duck_mode)
	    SQUASH_KA(current);

	if(FD_ISSET(current->fd, &block_write_fdset) ||
	   FD_ISSET(current->fd, &block_read_fdset)) {
	    ready_request(current);
	}
   	else if(current->kacount && ((time(NULL) - current->time_last) > ka_timeout)) {
            SQUASH_KA(current);
            free_request(&request_block, current);
        }
	else if((time(NULL) - current->time_last) > REQUEST_TIMEOUT) {
	    log_error_doc(current);
	    fputs("connection timed out\n",stderr);
 	    SQUASH_KA(current);
	    free_request(&request_block, current);
	}
	else {
            if(current->status == WRITE) {
		FD_SET(current->fd, &block_write_fdset);
            }
            else {
		FD_SET(current->fd, &block_read_fdset);
	    }
	}
	  
	current = next;
    }

    if (lame_duck_mode)
	FD_CLR(server_s, &block_read_fdset);
    else
        FD_SET(server_s, &block_read_fdset);	/* server always set */
    req_timeout.tv_sec = (ka_timeout ? ka_timeout : REQUEST_TIMEOUT);
    req_timeout.tv_usec = 0l;                   /* reset timeout */
}

/*
 * Name: die
 * Description: die with fatal error
 */

void die(int exit_code)
{
    int errno_save=errno;
    log_error_time();

    switch(exit_code) {
      case SERVER_ERROR:
	fprintf(stderr, "fatal error: exiting\n");	
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
        fprintf(stderr, "port %d: ", server_port);
        errno=errno_save;
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
	perror("logfile fopen");    /* ??? */
	break;
      case NO_CREATE_TMP:
        fprintf(stderr, "unable to create cachedir \"%s\": \n", cachedir);
        log_error_time();
        errno=errno_save;
        perror("mkdir");
	break;
      case WRONG_TMP_STAT:
	fprintf(stderr, "cachedir \"%s\" unusable\n", cachedir);
        log_error_time();
	errno=errno_save;
	perror("chmod");
	break;
      default:
	break;
    }

    fclose(stderr);
    exit(exit_code);
}

