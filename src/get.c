/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1996,97 Jon Nelson <nels0988@tc.umn.edu>
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

#include "boa.h"

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
	char buf[MAX_PATH_LENGTH];
	struct stat statbuf;
	
	data_fd = open(req->pathname, O_RDONLY);

	if (data_fd == -1) {		/* cannot open */
#ifdef GUNZIP
		sprintf(buf, "%s.gz", req->pathname);
		data_fd = open(buf, O_RDONLY);
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
		req->pathname = strdup(buf);
		return init_cgi(req);	/* 1 - OK, 2 - die */
#endif
	}
	fstat(data_fd, &statbuf);

	if (S_ISDIR(statbuf.st_mode)) {		/* directory */
		close(data_fd);			/* close dir */

		if (req->pathname[strlen(req->pathname) - 1] != '/') {
			char buffer[3 * MAX_PATH_LENGTH + 128];

			if (server_port != 80)
				sprintf(buffer, "http://%s:%d%s/", server_name, server_port,
						req->request_uri);
			else
				sprintf(buffer, "http://%s%s/", server_name, req->request_uri);
			send_redirect_perm(req, buffer);
			return 0;
		}
		data_fd = get_dir(req, &statbuf);	/* updates statbuf */

		if (data_fd == -1)		/* couldn't do it */
			return 0;			/* errors reported by get_dir */
		else if (data_fd == 0)
			return 1;
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
	/* MAP_OPTIONS: see compat.h */
	req->data_mem = mmap(0, statbuf.st_size, PROT_READ, MAP_OPTIONS,
						 data_fd, 0);
	close(data_fd);				/* close data file */

	if ((long) req->data_mem == -1) {
		boa_perror(req, "mmap");
		return 0;
	}
	send_r_request_ok(req);		/* All's well */

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

	bytes_written = write(req->fd, req->data_mem + req->filepos,
						  bytes_to_write);

	if (bytes_written == -1)
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return -1;			/* request blocked at the pipe level, but keep going */
		else {
			if (errno != EPIPE) {
				log_error_doc(req);	/* Can generate lots of log entries, */
				perror("write");	/* OK to disable if your logs get too big */
			}
			return 0;
		}
	req->filepos += bytes_written;
	if (req->filepos == req->filesize)	/* EOF */
		return 0;
	else
		return 1;				/* more to do */
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

	sprintf(pathname_with_index, "%s%s", req->pathname, directory_index);

	data_fd = open(pathname_with_index, O_RDONLY);

	if (data_fd != -1) {		/* user's index file */
		strcpy(req->request_uri, directory_index);	/* for mimetype */
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
	if (data_fd != -1) {		/* user's index file */
		close(data_fd);

		req->response_status = R_REQUEST_OK;
		if (req->method == M_HEAD) {
			req_write(req, "HTTP/1.0 200 OK-GUNZIP\r\n");
			print_http_headers(req);
			print_last_modified(req);
			req_write(req, "Content-Type: ");
			req_write(req, get_mime_type(directory_index));
			req_write(req, "\r\n\r\n");
			req_flush(req);
			return 0;
		}
		if (req->pathname)
			free(req->pathname);
		req->pathname = strdup(pathname_with_index);
		if (init_cgi(req) == 0)
			return -1;
		return 0;				/* in this case, 0 means success */
	} else
#endif

	if (dirmaker != NULL) {
		req->response_status = R_REQUEST_OK;

		/* the indexer should take care of all headers */
		if (!req->simple || req->method == M_HEAD) {
			req_write(req, "HTTP/1.0 200 OK\r\n");
			print_http_headers(req);
			print_last_modified(req);
			req_write(req, "Content-Type: ");
			req_write(req, get_mime_type(directory_index));
			req_write(req, "\r\n\r\n");
			req_flush(req);
		}
		if (req->method == M_HEAD)
			return 0;

		return (init_cgi(req) == 0 ? -1 : 0);
		/* in this case, 0 means success */
	}
	send_r_forbidden(req);
	return -1;					/* nothing worked */
}
