/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996,97 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1996 Charles F. Randall <crandall@goldsys.com>
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

/* $Id: cgi.c,v 1.66 2000/10/03 01:49:20 jon Exp $ */

#include "boa.h"

int verbose_cgi_logs = 0;

static char **common_cgi_env;

/*
 * Name: create_common_env
 *
 * Description: Set up the environment variables that are common to
 * all CGI scripts
 */

void create_common_env()
{
    int index = 0;

    common_cgi_env =
        (char **) malloc(sizeof (char *) * COMMON_CGI_COUNT + 4);
    /* The +4 is to handle the extra 3 environment variables + the NULL in
       complete_env
     */

    /* NOTE NOTE NOTE:
       If you (the reader) someday modify this chunk of code to
       handle more "common" CGI environment variables, then bump the
       value in defines.h UP

       Also, in the case of document_root and server_admin, two variables
       that may or may not be defined depending on the way the server
       is configured, we check for null values and use an empty
       string to denote a NULL value to the environment, as per the
       specification. The quote for which follows:

       "In all cases, a missing environment variable is
       equivalent to a zero-length (NULL) value, and vice versa."
     */
    common_cgi_env[index++] = env_gen("PATH", DEFAULT_PATH);
    common_cgi_env[index++] = env_gen("SERVER_SOFTWARE", SERVER_VERSION);
    common_cgi_env[index++] = env_gen("SERVER_NAME", server_name);
    common_cgi_env[index++] = env_gen("GATEWAY_INTERFACE", CGI_VERSION);
    common_cgi_env[index++] =
        env_gen("SERVER_PORT", simple_itoa(server_port));

    /* NCSA and APACHE added -- not in CGI spec */
    common_cgi_env[index++] = env_gen("DOCUMENT_ROOT", document_root);

    /* NCSA added */
    common_cgi_env[index++] = env_gen("SERVER_ROOT", server_root);

    /* APACHE added */
    common_cgi_env[index++] = env_gen("SERVER_ADMIN", server_admin);
    common_cgi_env[index] = NULL;
}

/*
 * Name: create_env
 *
 * Description: Allocates memory for environment before execing a CGI
 * script.  I like spelling creat with an extra e, don't you?
 */

void create_env(request * req)
{
    int i;

    req->cgi_env = (char **) malloc(sizeof (char *) * CGI_ENV_MAX + 4);

    for (i = 0; common_cgi_env[i]; i++)
        req->cgi_env[i] = common_cgi_env[i];

    req->cgi_env_index = i;

    {
        char *w;

        switch (req->method) {
        case M_POST:
            w = "POST";
            break;
        case M_HEAD:
            w = "HEAD";
            break;
        case M_GET:
            w = "GET";
            break;
        default:
            w = "UNKNOWN";
            break;
        }
        req->cgi_env[req->cgi_env_index++] = env_gen("REQUEST_METHOD", w);
    }

    req->cgi_env[req->cgi_env_index++] =
        env_gen("SERVER_PROTOCOL", req->http_version);

    if (req->path_info) {
        req->cgi_env[req->cgi_env_index++] =
            env_gen("PATH_INFO", req->path_info);
        /* path_translated depends upon path_info */
        req->cgi_env[req->cgi_env_index++] =
            env_gen("PATH_TRANSLATED", req->path_translated);
    }
    req->cgi_env[req->cgi_env_index++] =
        env_gen("SCRIPT_NAME", req->script_name);

    if (req->query_string) {
        req->cgi_env[req->cgi_env_index++] =
            env_gen("QUERY_STRING", req->query_string);
    }
    req->cgi_env[req->cgi_env_index++] =
        env_gen("REMOTE_ADDR", req->remote_ip_addr);

    req->cgi_env[req->cgi_env_index++] =
        env_gen("REMOTE_PORT", simple_itoa(req->remote_port));
}

/*
 * Name: env_gen_extra
 *       (and via a not-so-tricky #define, env_gen)
 * This routine calls malloc: please free the memory when you are done
 */
char *env_gen_extra(const char *key, const char *value, int extra)
{
    char *result;
    int key_len, value_len;

    if (value == NULL)          /* ServerAdmin may not be defined, eg */
        value = "";
    key_len = strlen(key);
    value_len = strlen(value);
    /* leave room for '=' sign and null terminator */
    result = malloc(extra + key_len + value_len + 2);
    if (result) {
        memcpy(result + extra, key, key_len);
        *(result + extra + key_len) = '=';
        memcpy(result + extra + key_len + 1, value, value_len);
        *(result + extra + key_len + value_len + 1) = '\0';
    }
    return result;
}

/*
 * Name: add_cgi_env
 *
 * Description: adds a variable to CGI's environment
 * Used for HTTP_ headers
 */

void add_cgi_env(request * req, char *key, char *value)
{
    char *p;

    if (req->cgi_env_index < CGI_ENV_MAX) {
        p = env_gen_extra(key, value, 5);
        memcpy(p, "HTTP_", 5);
        req->cgi_env[req->cgi_env_index++] = p;
    } else {
        log_error_time();
        fprintf(stderr, "Unable to generate additional CGI Environment"
                "variable -- not enough space!\n");
    }
}

/*
 * Name: complete_env
 *
 * Description: adds the known client header env variables
 * and terminates the environment array
 */

void complete_env(request * req)
{
    if (req->method == M_POST) {
        if (req->content_type)
            req->cgi_env[req->cgi_env_index++] =
                env_gen("CONTENT_TYPE", req->content_type);
        else
            req->cgi_env[req->cgi_env_index++] =
                env_gen("CONTENT_TYPE", default_type);

        if (req->content_length) {
            req->cgi_env[req->cgi_env_index++] =
                env_gen("CONTENT_LENGTH", req->content_length);
        }
    }
#ifdef ACCEPT_ON
    if (req->accept[0]) {
        req->cgi_env[req->cgi_env_index++] =
            env_gen("HTTP_ACCEPT", req->accept);
    }
#endif
    if (req->header_referer) {
        req->cgi_env[req->cgi_env_index++] =
            env_gen("HTTP_REFERER", req->header_referer);
    }
    req->cgi_env[req->cgi_env_index] = NULL; /* terminate */
}

/*
 * Name: make_args_cgi
 *
 * Build argv list for a CGI script according to spec
 *
 */

void create_argv(request * req, char **aargv)
{
    char *p, *q, *r;
    int aargc;

    q = req->query_string;
    aargv[0] = req->pathname;

    if (q && !strchr(q, '=')) {
        /* fprintf(stderr,"Parsing string %s\n",q); */
        q = strdup(q);
        for (aargc = 1; q && (aargc < CGI_ARGC_MAX);) {
            r = q;
            if ((p = strchr(q, '+'))) {
                *p = '\0';
                q = p + 1;
            } else {
                q = NULL;
            }
            if (unescape_uri(r)) {
                /* printf("parameter %d: %s\n",aargc,r); */
                aargv[aargc++] = r;
            }
        }
        aargv[aargc] = NULL;
    } else {
        aargv[1] = NULL;
    }
}

/*
 * Name: init_cgi
 *
 * Description: Called for GET/POST requests that refer to ScriptAlias
 * directories or application/x-httpd-cgi files.  Ties stdout to socket,
 * stdin to data if POST, and execs CGI.
 * stderr remains tied to our log file; is this good?
 *
 * Returns:
 * 0 - error or NPH, either way the socket is closed
 * 1 - success
 */

int init_cgi(request * req)
{
    int child_pid;
    int pipes[2];

    SQUASH_KA(req);

    if (req->is_cgi)
        complete_env(req);
#ifdef FASCIST_LOGGING
    {
        int i;
        for (i = 0; i < req->cgi_env_index; ++i)
            fprintf(stderr, "%s - environment variable for cgi: \"%s\"\n",
                    __FILE__, req->cgi_env[i]);
    }
#endif

    if (req->is_cgi == CGI)
        pipe(pipes);

    if ((child_pid = fork()) == -1) { /* fork unsuccessful */
        log_error_time();
        perror("fork");

        if (req->is_cgi == CGI) {
            close(pipes[0]);
            close(pipes[1]);
        }
        send_r_error(req);
        /* FIXME: There is aproblem here. send_r_error would work
           for NPH and CGI, but not for GUNZIP.  Fix that. */
        /* i'd like to send_r_error, but.... */
        return 0;
    }
    /* if here, fork was successful */

    if (!child_pid) {           /* 0 == child */
        if (req->is_cgi == CGI) {
            close(pipes[0]);
            /* tie cgi's STDOUT to it's write end of pipe */
            if (dup2(pipes[1], STDOUT_FILENO) == -1) {
                log_error_time();
                perror("dup2 - pipes");
                close(pipes[1]);
                exit(1);
            }
            if (fcntl(pipes[1], F_SETFL, 0) == -1) {
                log_error_time();
                perror("cgi-fcntl");
                close(pipes[1]);
                exit(1);
            }
        } else {
            /* tie stdout to socket */
            if (dup2(req->fd, STDOUT_FILENO) == -1) {
                log_error_time();
                perror("dup2 - fd");
                exit(1);
            }
            /* Switch socket flags back to blocking */
            if (fcntl(req->fd, F_SETFL, 0) == -1) {
                log_error_time();
                perror("cgi-fcntl");
                exit(1);
            }
        }
        /* tie post_data_fd to POST stdin */
        if (req->method == M_POST) { /* tie stdin to file */
            lseek(req->post_data_fd, SEEK_SET, 0);
            dup2(req->post_data_fd, STDIN_FILENO);
            close(req->post_data_fd);
        }
        /* Close access log, so CGI program can't scribble
         * where it shouldn't
         */
        close_access_log();

        /*
         * tie STDERR to cgi_log_fd
         * cgi_log_fd will automatically close, close-on-exec rocks!
         * if we don't tied STDERR (current log_error) to cgi_log_fd,
         *  then we ought to close it.
         */
        if (!cgi_log_fd)
            close(STDERR_FILENO);
        else
            dup2(cgi_log_fd, STDERR_FILENO);

        if (req->is_cgi) {
            char *aargv[CGI_ARGC_MAX + 1];
            create_argv(req, aargv);
            execve(req->pathname, aargv, req->cgi_env);
        } else {
            if (req->pathname[strlen(req->pathname) - 1] == '/')
                execl(dirmaker, dirmaker, req->pathname, req->request_uri,
                      NULL);
#ifdef GUNZIP
            else
                execl(GUNZIP, GUNZIP, "--stdout", "--decompress",
                      req->pathname, NULL);
#endif
        }
        /* execve failed */
        log_error_mesg(__FILE__, __LINE__, req->pathname);
        exit(1);
    }

    /* if here, fork was successful */
    if (verbose_cgi_logs) {
        log_error_time();
        fprintf(stderr, "Forked child \"%s\" pid %d\n",
                req->pathname, child_pid);
    }

    if (req->method == M_POST)
        close(req->post_data_fd); /* child closed it too */

    /* NPH, GUNZIP, etc... all go straight to the fd */
    if (req->is_cgi != CGI)
        return 0;

    close(pipes[1]);
    req->data_fd = pipes[0];

    req->status = PIPE_READ;
    if (req->is_cgi == CGI) {
        req->cgi_status = CGI_PARSE; /* got to parse cgi header */
        /* for cgi_header... I get half the buffer! */
        req->header_line = req->header_end =
            (req->buffer + BUFFER_SIZE / 2);
    } else {
        req->cgi_status = CGI_BUFFER;
        /* I get all the buffer! */
        req->header_line = req->header_end = req->buffer;
    }

    /* reset req->filepos for logging (it's used in pipe.c) */
    /* still don't know why req->filesize might be reset though */
    req->filepos = 0;

    /* set the read end of the socket to non-blocking */
    if (fcntl(pipes[0], F_SETFL, O_NONBLOCK) == -1) {
        log_error_time();
        perror("cgi-fcntl");
        return 0;
    }

    return 1;
}
