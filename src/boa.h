/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996-99 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1997-99 Jon Nelson <jnelson@boa.org>
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

/* $Id: boa.h,v 1.43 2000/04/10 19:45:10 jon Exp $*/

#ifndef _BOA_H
#define _BOA_H

#include "config.h"
/* config.h should be 1st include because this sets lots of #defines that
 are used in other header files
 */

#include "compat.h"             /* oh what fun is porting */
#include "defines.h"
#include "globals.h"

#include <errno.h>
#include <stdlib.h>             /* malloc, free, etc. */
#include <stdio.h>              /* stdin, stdout, stderr */
#include <string.h>             /* strdup */
#include <ctype.h>
#include <time.h>               /* localtime, time */
#include <pwd.h>

#include <sys/types.h>          /* socket, bind, accept */
#include <sys/socket.h>         /* socket, bind, accept, setsockopt, */
#include <sys/stat.h>           /* open */

/* alias */

void add_alias(char *fakename, char *realname, int script);
int translate_uri(request * req);
int init_script_alias(request * req, alias * current, int uri_len);
void dump_alias(void);

/* config */

void read_config_files(void);

/* get */

int init_get(request * req);
int process_get(request * req);
int get_dir(request * req, struct stat *statbuf);

/* hash */

int get_mime_hash_value(char *extension);
char *get_mime_type(char *filename);
char *get_home_dir(char *name);
void dump_mime(void);
void dump_passwd(void);

/* log */

void open_logs(void);
void close_access_log(void);
void log_access(request * req);
void log_error_doc(request * req);
void boa_perror(request * req, char *message);
void log_error_time(void);
void log_error_mesg(char *file, int line, char *mesg);

/* queue */

void block_request(request * req);
void ready_request(request * req);
void dequeue(request ** head, request * req);
void enqueue(request ** head, request * req);

/* read */

int read_header(request * req);
int read_body(request * req);
int write_body(request * req);

/* request */

request *new_request(void);
void get_request(int);
void process_requests(void);
int process_header_end(request * req);
int process_header_line(request * req);
int process_logline(request * req);
void process_option_line(request * req);
void add_accept_header(request * req, char *mime_type);
void free_requests(void);

/* response */

void print_ka_phrase(request * req);
void print_content_type(request * req);
void print_content_length(request * req);
void print_last_modified(request * req);
void print_http_headers(request * req);

void send_r_request_ok(request * req); /* 200 */
void send_redirect_perm(request * req, char *url); /* 301 */
void send_redirect_temp(request * req, char *url, char *more_hdr); /* 302 */
void send_r_not_modified(request * req); /* 304 */
void send_r_bad_request(request * req); /* 400 */
void send_r_unauthorized(request * req, char *name); /* 401 */
void send_r_forbidden(request * req); /* 403 */
void send_r_not_found(request * req); /* 404 */
void send_r_error(request * req); /* 500 */
void send_r_not_implemented(request * req); /* 501 */
void send_r_bad_version(request * req); /* 505 */

/* cgi */

void create_common_env(void);
void create_env(request * req);
#define env_gen(x,y) env_gen_extra(x,y,0)
char *env_gen_extra(const char *key, const char *value, int extra);
void add_cgi_env(request * req, char *key, char *value);
void complete_env(request * req);
void create_argv(request * req, char **aargv);
int init_cgi(request * req);

/* signals */

void init_signals(void);
void sighup_run(void);
void sigchld_run(void);
void lame_duck_mode_run(int);

/* util.c */
void clean_pathname(char *pathname);
char *get_commonlog_time(void);
void rfc822_time_buf(char *buf, time_t s);
char *simple_itoa(int i);
char *escape_string(char *inp, char *buf);
int month2int(char *month);
int modified_since(time_t * mtime, char *if_modified_since);
char *to_upper(char *str);
int unescape_uri(char *uri);

/* buffer */
int req_write(request * req, char *msg);
int req_write_escape_http(request * req, char *msg);
int req_write_escape_html(request * req, char *msg);
int req_flush(request * req);
char *escape_uri(char *uri);

/* timestamp.c */
void timestamp(void);

/* mmap_cache.c */
struct mmap_entry *find_mmap(int data_fd, struct stat *s);
void release_mmap(struct mmap_entry *e);

/* sublog.c */
int open_gen_fd(char *spec);
int process_cgi_header(request * req);

/* pipe.c */
int read_from_pipe(request * req);
int write_from_pipe(request * req);

/* ip.c */
int bind_server(int server_s, char *server_ip);
char *ascii_sockaddr(struct SOCKADDR *s, char *dest, int len);
int net_port(struct SOCKADDR *s);

#endif
