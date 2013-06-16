/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@cebaf.gov>
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

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "defines.h"
#include "compat.h"
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>

struct request {	/* pending requests */
    int fd;					/* client's socket fd */
    int status;				        /* see #defines.h */
    int simple;					/* simple request? */
    int keepalive;                              /* 0-disabled 2-close 3-continue */
    int kacount;                                /* kacount */

    int data_fd;				/* fd of data */
    unsigned long filesize;			/* filesize */
    char * data_mem;				/* mmapped char array */
    int time_last;				/* time of last succ. op. */
    int method;					/* M_GET, M_POST, etc. */
    int response_len;				/* Length of response[] */

    char request_uri[MAX_HEADER_LENGTH + 1];	/* uri */
    char logline[MAX_LOG_LENGTH + 1];		/* line to log file */
    char response[RESPONSEBUF_SIZE + 1];	/* data to send */
    char accept[MAX_HEADER_LENGTH + 1];	        /* Accept: fields */

    char header_line[MAX_HEADER_LENGTH + 1];
    int headerpos;

    char http_version[8];                       /* HTTP/?.? of req */
    int header_offset;				/* offset into headers */
    long filepos;				/* position in file */
    int response_status;			/* R_NOT_FOUND etc. */

    char * ret_content_type;			/* content type to return */
    char * if_modified_since;			/* If-Modified-Since */
    char * user_agent;                          /* User-Agent */
    char * referer;                             /* Referer */
    char * remote_ip_addr;			/* after inet_ntoa */
    int remote_port;				/* could be used for ident */

    time_t last_modified;			/* Last-modified: */

    /* CGI needed vars */

    int is_cgi;				        /* true if CGI/NPH */
    char ** cgi_env;				/* CGI environment */
    int cgi_env_index;                          /* index into array */
    /* char ** cgi_argv; */			/* CGI argv */

    int post_data_fd;                           /* fd for post data tmpfile */

    char * path_info;				/* env variable */
    char * path_translated;			/* env variable */
    char * script_name;				/* env variable */
    char * query_string;			/* env variable */
    char * content_type;			/* env variable */
    char * content_length;			/* env variable */

    int cgi_argc;				/* CGI argc */
    
    struct request * next;			/* next */
    struct request * prev;			/* previous */
    char * post_file_name;			/* only used processing POST */
};

typedef struct request request;

struct alias {
    char * fakename;            /* URI path to file */
    char * realname;            /* Actual path to file */
    int type;                   /* ALIAS, SCRIPTALIAS, REDIRECT */
    int fake_len;		/* strlen of fakename */
    int real_len;		/* strlen of realname */
    struct alias * next;
};

typedef struct alias alias;

extern char * optarg;			/* For getopt */
extern FILE * yyin;			/* yacc input */

extern request * request_ready;		/* first in ready list */
extern request * request_block;		/* first in blocked list */
extern request * request_free;		/* first in free list */

extern fd_set block_read_fdset;		/* fds blocked on read */
extern fd_set block_write_fdset;	/* fds blocked on write */

extern int true;			/* true = 1: for setsockopt */

#define SQUASH_KA(req) ((req)->keepalive &= (~1))

/* global server variables */

extern char * access_log_name;
extern char * aux_log_name;
extern char * error_log_name;

extern int server_port;
extern UID_T server_uid;
extern GID_T server_gid;
extern char * server_admin;
extern char * server_root;
extern char * server_name;

extern char * document_root;
extern char * user_dir;
extern char * directory_index;
extern char * default_type;
extern char * cachedir;

extern int ka_timeout;
extern int ka_max;

extern int sighup_flag;
extern int sigchld_flag;
extern int lame_duck_mode;

#endif

