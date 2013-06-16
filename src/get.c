/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996,99 Larry Doolittle <ldoolitt@jlab.org>
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

/* $Id: get.c,v 1.70 2001/10/20 02:57:05 jnelson Exp $*/

#include "boa.h"
#include <sys/mman.h>

/* local prototypes */
int get_cachedir_file(request * req, struct stat *statbuf);
int index_directory(request * req, char *dest_filename);

/*
 * Name: init_get
 * Description: Initializes a non-script GET or HEAD request.
 *
 * Return values:
 *   0: finished or error, request will be freed
 *   1: successfully initialized, added to ready queue
 */

int init_get(request * req)
{
    int data_fd;
    struct stat statbuf;

    data_fd = open(req->pathname, O_RDONLY);

    if (data_fd == -1) {        /* cannot open */
        /* it's either a gunzipped file or a directory */

#ifdef GUNZIP
        char gzip_pathname[MAX_PATH_LENGTH];
        int len;

        len = strlen(req->pathname);

        memcpy(gzip_pathname, req->pathname, len);
        memcpy(gzip_pathname + len, ".gz", 3);
        gzip_pathname[len + 3] = '\0';
        data_fd = open(gzip_pathname, O_RDONLY);
        if (data_fd == -1) {
#endif
            int errno_save = errno;
            log_error_doc(req);
            errno = errno_save;
            perror("document open");
            errno = errno_save;

            if (errno == ENOENT)
                send_r_not_found(req);
            else if (errno == EACCES)
                send_r_forbidden(req);
            else
                send_r_bad_request(req);
            return 0;
#ifdef GUNZIP
        }
        close(data_fd);

        req->response_status = R_REQUEST_OK;
        if (!req->simple) {
            req_write(req, "HTTP/1.0 200 OK-GUNZIP\r\n");
            print_http_headers(req);
            print_content_type(req);
            print_last_modified(req);
            req_write(req, "\r\n");
            req_flush(req);
        }
        if (req->method == M_HEAD)
            return 0;
        if (req->pathname)
            free(req->pathname);
        req->pathname = strdup(gzip_pathname);
        return init_cgi(req);
#endif
    }
    fstat(data_fd, &statbuf);

    if (S_ISDIR(statbuf.st_mode)) { /* directory */
        close(data_fd);         /* close dir */

        if (req->pathname[strlen(req->pathname) - 1] != '/') {
            char buffer[3 * MAX_PATH_LENGTH + 128];

            if (server_port != 80)
                sprintf(buffer, "http://%s:%d%s/", server_name,
                        server_port, req->request_uri);
            else
                sprintf(buffer, "http://%s%s/", server_name,
                        req->request_uri);
            send_redirect_perm(req, buffer);
            return 0;
        }
        data_fd = get_dir(req, &statbuf); /* updates statbuf */

        if (data_fd == -1)      /* couldn't do it */
            return 0;           /* errors reported by get_dir */
        else if (data_fd <= 1)
            /* data_fd == 0 -> close it down, 1 -> continue */
            return data_fd;
        /* else, data_fd contains the fd of the file... */
    }
    if (req->if_modified_since &&
        !modified_since(&(statbuf.st_mtime), req->if_modified_since)) {
        send_r_not_modified(req);
        close(data_fd);
        return 0;
    }
    req->filesize = statbuf.st_size;
    req->last_modified = statbuf.st_mtime;

    if (req->method == M_HEAD) {
        send_r_request_ok(req);
        close(data_fd);
        return 0;
    }

    if (req->filesize > MAX_FILE_MMAP) {
        send_r_request_ok(req); /* All's well */
        req->status = PIPE_READ;
        req->cgi_status = CGI_BUFFER;
        req->data_fd = data_fd;
        req_flush(req);         /* this should *always* complete due to
                                   the size of the I/O buffers */
        req->header_line = req->header_end = req->buffer;
        return 1;
    }

    req->mmap_entry_var = find_mmap(data_fd, &statbuf);
    if (req->mmap_entry_var == NULL) {
        send_r_error(req);
        close(data_fd);
        return 0;
    }
    req->data_mem = req->mmap_entry_var->mmap;
    close(data_fd);             /* close data file */

    if ((long) req->data_mem == -1) {
        boa_perror(req, "mmap");
        return 0;
    }

    send_r_request_ok(req);     /* All's well */
    {                           /* combine first part of file with headers */
        int bob;

        bob = BUFFER_SIZE - req->buffer_end;
        /* bob is now how much the buffer can hold
         * after the headers
         */

        if (bob > 0) {
            if (bob > req->filesize)
                bob = req->filesize;

            memcpy(req->buffer + req->buffer_end, req->data_mem, bob);
            req->buffer_end += bob;
            req->filepos += bob;
            if (req->filesize == req->filepos) {
                req_flush(req);
                req->status = DONE;
                return 1;       /* get it flushed next time around if need be */
            }
        }
    }

    /* We lose statbuf here, so make sure response has been sent */
    return 1;
}

/*
 * Name: process_get
 * Description: Writes a chunk of data to the socket.
 *
 * Return values:
 *  -1: request blocked, move to blocked queue
 *   0: EOF or error, close it down
 *   1: successful write, recycle in ready queue
 */

int process_get(request * req)
{
    int bytes_written, bytes_to_write;

    bytes_to_write = req->filesize - req->filepos;
    if (bytes_to_write > SOCKETBUF_SIZE)
        bytes_to_write = SOCKETBUF_SIZE;

    bytes_written = write(req->fd, req->data_mem + req->filepos,
                          bytes_to_write);

    if (bytes_written < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return -1;
        /* request blocked at the pipe level, but keep going */
        else {
            if (errno != EPIPE) {
                log_error_doc(req);
                /* Can generate lots of log entries, */
                perror("write");
                /* OK to disable if your logs get too big */
            }
            req->status = DEAD;
            return 0;
        }
    }
    req->filepos += bytes_written;

    if (req->filepos == req->filesize) { /* EOF */
        return 0;
    } else
        return 1;               /* more to do */
}

/*
 * Name: get_dir
 * Description: Called from process_get if the request is a directory.
 * statbuf must describe directory on input, since we may need its
 *   device, inode, and mtime.
 * statbuf is updated, since we may need to check mtimes of a cache.
 * returns:
 *  -1 error
 *  0  cgi (either gunzip or auto-generated)
 *  >0  file descriptor of file
 */

int get_dir(request * req, struct stat *statbuf)
{

    char pathname_with_index[MAX_PATH_LENGTH];
    int data_fd;

    if (directory_index) {      /* look for index.html first?? */
        strcpy(pathname_with_index, req->pathname);
        strcat(pathname_with_index, directory_index);
        /*
           sprintf(pathname_with_index, "%s%s", req->pathname, directory_index);
         */

        data_fd = open(pathname_with_index, O_RDONLY);

        if (data_fd != -1) {    /* user's index file */
            strcpy(req->request_uri, directory_index); /* for mimetype */
            fstat(data_fd, statbuf);
            return data_fd;
        }
        if (errno == EACCES) {
            send_r_forbidden(req);
            return -1;
        }
#ifdef GUNZIP
        strcat(pathname_with_index, ".gz");
        data_fd = open(pathname_with_index, O_RDONLY);
        if (data_fd != -1) {    /* user's index file */
            close(data_fd);

            req->response_status = R_REQUEST_OK;
            SQUASH_KA(req);
            if (!req->simple) {
                req_write(req, "HTTP/1.0 200 OK-GUNZIP\r\n");
                print_http_headers(req);
                print_last_modified(req);
                req_write(req, "Content-Type: ");
                req_write(req, get_mime_type(directory_index));
                req_write(req, "\r\n\r\n");
                req_flush(req);
            }
            if (req->method == M_HEAD)
                return 0;
            if (req->pathname)
                free(req->pathname);
            req->pathname = strdup(pathname_with_index);
            return init_cgi(req);
        }
#endif
    }

    /* only here if index.html, index.html.gz don't exist */
    if (dirmaker != NULL) {     /* don't look for index.html... maybe automake? */
        req->response_status = R_REQUEST_OK;
        SQUASH_KA(req);

        /* the indexer should take care of all headers */
        if (!req->simple) {
            req_write(req, "HTTP/1.0 200 OK\r\n");
            print_http_headers(req);
            print_last_modified(req);
            req_write(req, "Content-Type: text/html\r\n\r\n");
            req_flush(req);
        }
        if (req->method == M_HEAD)
            return 0;

        return init_cgi(req);
        /* in this case, 0 means success */
    } else if (cachedir) {
        return get_cachedir_file(req, statbuf);
    } else {                    /* neither index.html nor autogenerate are allowed */
        send_r_forbidden(req);
        return -1;              /* nothing worked */
    }
}

int get_cachedir_file(request * req, struct stat *statbuf)
{

    char pathname_with_index[MAX_PATH_LENGTH];
    int data_fd;
    time_t real_dir_mtime;

    real_dir_mtime = statbuf->st_mtime;
    sprintf(pathname_with_index, "%s/dir.%d.%ld",
            cachedir, (int) statbuf->st_dev, statbuf->st_ino);
    data_fd = open(pathname_with_index, O_RDONLY);

    if (data_fd != -1) {        /* index cache */

        fstat(data_fd, statbuf);
        if (statbuf->st_mtime > real_dir_mtime) {
            statbuf->st_mtime = real_dir_mtime; /* lie */
            strcpy(req->request_uri, directory_index); /* for mimetype */
            return data_fd;
        }
        close(data_fd);
        unlink(pathname_with_index); /* cache is stale, delete it */
    }
    if (index_directory(req, pathname_with_index) == -1)
        return -1;

    data_fd = open(pathname_with_index, O_RDONLY); /* Last chance */
    if (data_fd != -1) {
        strcpy(req->request_uri, directory_index); /* for mimetype */
        fstat(data_fd, statbuf);
        statbuf->st_mtime = real_dir_mtime; /* lie */
        return data_fd;
    }

    boa_perror(req, "re-opening dircache");
    return -1;                  /* Nothing worked. */

}

/*
 * Name: index_directory
 * Description: Called from get_cachedir_file if a directory html
 * has to be generated on the fly
 * returns -1 for problem, else 0
 * This version is the fastest, ugliest, and most accurate yet.
 * It solves the "stale size or type" problem by not ever giving
 * the size or type.  This also speeds it up since no per-file
 * stat() is required.
 */

int index_directory(request * req, char *dest_filename)
{
    DIR *request_dir;
    FILE *fdstream;
    struct dirent *dirbuf;
    int bytes = 0;
    char *escname = NULL;

    if (chdir(req->pathname) == -1) {
        if (errno == EACCES || errno == EPERM) {
            send_r_forbidden(req);
        } else {
            log_error_doc(req);
            perror("chdir");
            send_r_bad_request(req);
        }
        return -1;
    }

    request_dir = opendir(".");
    if (request_dir == NULL) {
        int errno_save = errno;
        send_r_error(req);
        log_error_time();
        fprintf(stderr, "directory \"%s\": ", req->pathname);
        errno = errno_save;
        perror("opendir");
        return -1;
    }

    fdstream = fopen(dest_filename, "w");
    if (fdstream == NULL) {
        boa_perror(req, "dircache fopen");
        closedir(request_dir);
        return -1;
    }

    bytes += fprintf(fdstream,
                     "<HTML><HEAD>\n<TITLE>Index of %s</TITLE>\n</HEAD>\n\n",
                     req->request_uri);
    bytes += fprintf(fdstream, "<BODY>\n\n<H2>Index of %s</H2>\n\n<PRE>\n",
                     req->request_uri);

    while ((dirbuf = readdir(request_dir))) {
        if (!strcmp(dirbuf->d_name, "."))
            continue;

        if (!strcmp(dirbuf->d_name, "..")) {
            bytes += fprintf(fdstream,
                             " [DIR] <A HREF=\"../\">Parent Directory</A>\n");
            continue;
        }

        if ((escname = escape_string(dirbuf->d_name, NULL)) != NULL) {
            bytes += fprintf(fdstream, " <A HREF=\"%s\">%s</A>\n",
                             escname, dirbuf->d_name);
            free(escname);
            escname = NULL;
        }
    }
    closedir(request_dir);
    bytes += fprintf(fdstream, "</PRE>\n\n</BODY>\n</HTML>\n");

    fclose(fdstream);

    chdir(server_root);

    req->filesize = bytes;      /* for logging transfer size */
    return 0;                   /* success */
}
