/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996 Charles F. Randall <crandall@goldsys.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1996-99 Jon Nelson <jnelson@boa.org>
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

/* $Id: boa.c,v 1.82 2001/09/19 01:21:19 jnelson Exp $*/

#include "boa.h"
#include <sys/resource.h>       /* setrlimit */
#include <grp.h>
#include <arpa/inet.h>          /* inet_ntoa */

inline void fdset_update(void);
void fixup_server_root(void);

int server_s;                   /* boa socket */
int backlog = SO_MAXCONN;
int max_connections = 0;

struct timeval req_timeout;     /* timeval for select */

extern char *optarg;            /* getopt */

fd_set block_read_fdset;
fd_set block_write_fdset;

int sighup_flag = 0;            /* 1 => signal has happened, needs attention */
int sigchld_flag = 0;           /* 1 => signal has happened, needs attention */
short lame_duck_mode = 0;
time_t current_time;

int sock_opt = 1;
int do_fork = 1;

int main(int argc, char **argv)
{
    int c;                      /* command line arg */

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

    /* set umask to u+rw, u-x, go-rwx */
    umask(~0600);

    /* For when we have done more work with chroot....
       if (chdir(chroot_path) == -1) {
       fprintf(stderr, "Could not chdir to ChrootPath.\n");
       exit(1);
       }

       if (chroot_path) {
       puts(chroot_path);
       if (chroot(chroot_path) == -1) {
       perror("chroot:");
       exit(1);
       }
       }
     */

    read_config_files();
    open_logs();
    create_common_env();

    if ((server_s = socket(SERVER_AF, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        log_error_mesg(__FILE__, __LINE__, "unable to create socket");
        exit(errno);
    }

    /* server socket is nonblocking */
    if (fcntl(server_s, F_SETFL, NOBLOCK) == -1) {
        log_error_mesg(__FILE__, __LINE__,
                       "fcntl: unable to set server socket to nonblocking");
        exit(errno);
    }

    /* close server socket on exec so cgi's can't write to it */
    if (fcntl(server_s, F_SETFD, 1) == -1) {
        log_error_mesg(__FILE__, __LINE__,
                       "can't set close-on-exec on server socket!");
        exit(errno);
    }

    /* reuse socket addr */
    if ((setsockopt(server_s, SOL_SOCKET, SO_REUSEADDR, (void *) &sock_opt,
                    sizeof (sock_opt))) == -1) {
        log_error_mesg(__FILE__, __LINE__, "setsockopt");
        exit(errno);
    }

    /* internet family-specific code encapsulated in bind_server()  */
    if (bind_server(server_s, server_ip) == -1) {
        log_error_mesg(__FILE__, __LINE__, "unable to bind");
        exit(errno);
    }

    /* listen: large number just in case your kernel is nicely tweaked */
    if (listen(server_s, backlog) == -1) {
        log_error_mesg(__FILE__, __LINE__, "unable to listen");
        exit(errno);
    }

    init_signals();

    /* background ourself */

    if (do_fork)
        if (fork())
            exit(0);

    /* make STDIN and STDOUT point to /dev/null */
    {
        int devnull = open("/dev/null", 0);

        if (devnull == -1) {
            log_error_mesg(__FILE__, __LINE__, "can't open /dev/null");
            exit(errno);
        }

        if (dup2(devnull, STDIN_FILENO) == -1) {
            log_error_mesg(__FILE__, __LINE__,
                           "can't dup2 /dev/null to STDIN_FILENO");
            exit(errno);
        }

        if (dup2(devnull, STDOUT_FILENO) == -1) {
            log_error_mesg(__FILE__, __LINE__,
                           "can't dup2 /dev/null to STDIN_FILENO");
            exit(errno);
        }
        close(devnull);
    }

    /* write a PID file if we have one */
    /* do we unlink here and not close (system will unlink when we exit)
       or do we try to unlink the file at close time in signals.c,
       which means we might have to unlink a root-owned file
       Also, "Removing stale pid file..." and so on
       Do we check for existing boa.pid file?
       If so, do we see if that process exists?  I mean, sheesh!
       if (pidfile) {
       FILE *bob = fopen("w", pidfile);
       if (bob == NULL) {
       log_error_mesg(__FILE__, __LINE__, "pidfile - fopen");
       exit(errno);
       }
       fprintf(bob, "%d", getpid());
       }
     */

    /* give away our privs if we can */
    /* but first, update timestamp, because log_error_time uses it */
    time(&current_time);

    if (getuid() == 0) {
        struct passwd *passwdbuf;
        passwdbuf = getpwuid(server_uid);
        if (passwdbuf == NULL) {
            log_error_mesg(__FILE__, __LINE__, "getpwuid");
            exit(errno);
        }
        if (initgroups(passwdbuf->pw_name, passwdbuf->pw_gid) == -1) {
            log_error_mesg(__FILE__, __LINE__, "initgroups");
            exit(errno);
        }
        if (setgid(server_gid) == -1) {
            log_error_mesg(__FILE__, __LINE__, "setgid");
            exit(errno);
        }
        if (setuid(server_uid) == -1) {
            log_error_mesg(__FILE__, __LINE__, "setuid");
            exit(errno);
        }
    } else {
        if (server_gid || server_uid) {
            log_error_time();
            fprintf(stderr, "Warning: "
                    "Not running as root: no attempt to change"
                    " to uid %d gid %d\n", server_uid, server_gid);
        }
        server_gid = getgid();
        server_uid = getuid();
    }

    {
        struct rlimit rl;

        getrlimit(RLIMIT_NOFILE, &rl);
        max_connections = rl.rlim_cur;
    }

    /* main loop */
    timestamp();
    build_needs_escape();

    FD_ZERO(&block_read_fdset);
    FD_ZERO(&block_write_fdset);

    status.requests = 0;
    status.errors = 0;

    /* set server_s and req_timeout */
    FD_SET(server_s, &block_read_fdset); /* server always set */
    req_timeout.tv_sec = (ka_timeout ? ka_timeout : REQUEST_TIMEOUT);
    req_timeout.tv_usec = 0l;   /* reset timeout */

    while (1) {
        if (sighup_flag)
            sighup_run();
        if (sigchld_flag)
            sigchld_run();

        if (lame_duck_mode) {
            if (lame_duck_mode == 1)
                lame_duck_mode_run(server_s);
            if (!request_ready && !request_block) {
                log_error_time();
                fprintf(stderr, "exiting Boa normally\n");
                chdir(tempdir);
                exit(0);
            }
        }

        if (!request_ready) {
            if (select(OPEN_MAX, &block_read_fdset,
                       &block_write_fdset, NULL,
                       (request_block ? &req_timeout : NULL)) == -1) {
                /* what is the appropriate thing to do here on EBADF */
                if (errno == EINTR)
                    continue;   /* while(1) */
                else if (errno != EBADF) {
                    log_error_mesg(__FILE__, __LINE__, "select");
                    exit(errno);
                }

            }
            time(&current_time);
            if (FD_ISSET(server_s, &block_read_fdset))
                get_request(server_s);

            if (request_block)
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
 *
 * Here, we need to do some things:
 *  - keepalive timeouts simply close
 *    (this is special:: a keepalive timeout is a timeout where
       keepalive is active but nothing has been read yet)
 *  - regular timeouts close + error
 *  - stuff in buffer and fd ready?  write it out
 *  - fd ready for other actions?  do them
 */

inline void fdset_update(void)
{
    request *current = request_block, *next;

    while (current) {
        time_t time_since = current_time - current->time_last;
        next = current->next;

        /* hmm, what if we are in "the middle" of a request and not
         * just waiting for a new one... perhaps check to see if anything
         * has been read via header position, etc... */
        if (current->kacount < ka_max && /* we *are* in a keepalive */
            (time_since >= ka_timeout) && /* ka timeout */
            !current->logline)  /* haven't read anything yet */
            current->status = DEAD; /* connection keepalive timed out */
        else if (time_since > REQUEST_TIMEOUT) {
            log_error_doc(current);
            fputs("connection timed out\n", stderr);
            current->status = DEAD;
        }
        if (current->buffer_end) {
            if (FD_ISSET(current->fd, &block_write_fdset))
                ready_request(current);
            else
                FD_SET(current->fd, &block_write_fdset);
        } else {
            switch (current->status) {
            case WRITE:
            case PIPE_WRITE:
                if (FD_ISSET(current->fd, &block_write_fdset))
                    ready_request(current);
                else
                    FD_SET(current->fd, &block_write_fdset);
                break;
            case BODY_WRITE:
                if (FD_ISSET(current->post_data_fd, &block_write_fdset))
                    ready_request(current);
                else
                    FD_SET(current->post_data_fd, &block_write_fdset);
                break;
            case PIPE_READ:
                if (FD_ISSET(current->data_fd, &block_read_fdset))
                    ready_request(current);
                else
                    FD_SET(current->data_fd, &block_read_fdset);
                break;
            case DONE:
                if (FD_ISSET(current->fd, &block_write_fdset))
                    ready_request(current);
                else
                    FD_SET(current->fd, &block_write_fdset);
                break;
            case DEAD:
                ready_request(current);
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

    if (!lame_duck_mode && total_connections < (max_connections - 10))
        FD_SET(server_s, &block_read_fdset); /* server always set */
    req_timeout.tv_sec = (ka_timeout ? ka_timeout : REQUEST_TIMEOUT);
    req_timeout.tv_usec = 0l;   /* reset timeout */
}

/*
 * Name: fixup_server_root
 *
 * Description: Makes sure the server root is valid.
 *
 */

void fixup_server_root()
{
    char *dirbuf;
    int dirbuf_size;

    if (!server_root) {
#ifdef SERVER_ROOT
        server_root = strdup(SERVER_ROOT);
#else
        fputs("boa: don't know where server root is.  Please #define "
              "SERVER_ROOT in boa.h\n"
              "and recompile, or use the -c command line option to "
              "specify it.\n", stderr);
        exit(1);
#endif
    }

    if (chdir(server_root) == -1) {
        fprintf(stderr, "Could not chdir to \"%s\": aborting\n",
                server_root);
        exit(1);
    }

    if (server_root[0] == '/')
        return;

    /* if here, server_root (as specified on the command line) is
     * a relative path name. CGI programs require SERVER_ROOT
     * to be absolute.
     */

    dirbuf_size = MAX_PATH_LENGTH * 2 + 1;
    if ((dirbuf = (char *) malloc(dirbuf_size)) == NULL) {
        fprintf(stderr,
                "boa: Cannot malloc %d bytes of memory. Aborting.\n",
                dirbuf_size);
        exit(1);
    }
#ifndef HAVE_GETCWD
    perror("boa: getcwd() not defined. Aborting.");
    exit(1);
#endif
    if (getcwd(dirbuf, dirbuf_size) == NULL) {
        if (errno == ERANGE)
            perror
                ("boa: getcwd() failed - unable to get working directory. "
                 "Aborting.");
        else if (errno == EACCES)
            perror("boa: getcwd() failed - No read access in current "
                   "directory. Aborting.");
        else
            perror("boa: getcwd() failed - unknown error. Aborting.");
        exit(1);
    }
    fprintf(stderr,
            "Warning, the server_root directory specified"
            " on the command line - "
            "\"%s\" - is relative.  CGI programs expect the environment "
            "variable SERVER_ROOT to be an absolute path.  "
            "Setting SERVER_ROOT to \"%s\" to conform.\n", server_root,
            dirbuf);
    free(server_root);
    server_root = dirbuf;
}
