/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996, 97 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1997 Jon Nelson <nels0988@tc.umn.edu>
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
 * Name: read_header
 * Description: Reads data from a request socket.  Manages the current
 * status via a state machine.  Changes status from READ_HEADER to
 * READ_BODY or WRITE as necessary.
 * 
 * Return values:
 *  -1: request blocked, move to blocked queue
 *   0: request done, close it down
 *   1: more to do, leave on ready list
 */

int read_header(request * req)
{
	int bytes, buf_bytes_left;
	char *check, *buffer;

	if (req->pipeline_start){
		buffer = req->client_stream;
		bytes = req->client_stream_pos = req->pipeline_start;
		req->pipeline_start = 0;
	} else { /* first from free_request */
		buffer = req->client_stream + req->client_stream_pos;
		buf_bytes_left = CLIENT_STREAM_SIZE - req->client_stream_pos;
		if (buf_bytes_left < 0) {
			log_error_time();
			fputs("buffer overrun - read.c, read_header - closing\n", stderr);
			return 0;
		}
		bytes = read(req->fd, buffer, buf_bytes_left);
	
		if (bytes == -1) {
			if (errno == EINTR)
				return 1;
			if (errno == EAGAIN || errno == EWOULDBLOCK)	/* request blocked */
				return -1;
			else if (errno == EBADF || errno == EPIPE) {
				SQUASH_KA(req);		/* force close fd */
				return 0;
			} else {
				boa_perror(req, "header read");
				return 0;
			}
		} else if (bytes == 0)
			return 0;
		req->client_stream_pos += bytes;
	}

	check = buffer;

	while (check < (buffer + bytes)) {
		switch (req->status) {
		case READ_HEADER:
			if (*check == '\r') {
				req->status = ONE_CR;
				req->header_end = check;
			} else if (*check == '\n') {
				req->status = ONE_LF;
				req->header_end = check;
			}
			break;

		case ONE_CR:
			if (*check == '\n')
				req->status = ONE_LF;
			else
				req->status = READ_HEADER;
			break;

		case ONE_LF:			/* if here, we've found the end (for sure) of a header */
			if (*check == '\r')	/* could be end o headers */
				req->status = TWO_CR;
			else if (*check == '\n')
				req->status = BODY_READ;
			else
				req->status = READ_HEADER;
			break;

		case TWO_CR:
			if (*check == '\n')
				req->status = BODY_READ;
			else
				req->status = READ_HEADER;
			break;

		default:
			break;
		}

		++check;

		if (req->status == ONE_LF) {
			*req->header_end = '\0';
			/* terminate string that begins at req->header_line */
			/* (or at req->data_mem, if we've never been here before */

			/* the following logic still needs work, esp. after req->simple */
			if (req->logline)
				process_option_line(req);
			else {
				if (process_logline(req) == 0)
					return 0;
				if (req->simple)
					return process_header_end(req);
			}
			req->header_line = check; /* start of unprocessed data */
		} else if (req->status == BODY_READ) {
			int retval = process_header_end(req);
			/* process_header_end inits non-POST cgi's */
			
			req->pipeline_start = (check - req->client_stream);

			if (retval && req->method == M_POST) {

				/* rest of non-header data is contained in
				   the area following check

				   check now points to data 
				 */

				if (req->content_length)
					req->filesize = atoi(req->content_length);
				else {
					log_error_time();
					fprintf(stderr, "Unknown Content-Length POST\n");
				}
				
				/* buffer + bytes is 1 past the end of the data */
				req->filepos = (buffer + bytes) - check;
				req->header_line = check;
				req->header_end = (buffer + bytes);				

				if (req->filepos > req->filesize)
					req->cgi_status = CLOSE;	/* close after write */
				
				req->filepos = 0;	/* haven't actually anything it yet */
				req->status = BODY_WRITE;	/* so write it */
				/* have to write first, or read will be confused
				 * because of the special case where the filesize is 
				 * less than we have already read.  
				 */
			}
			return retval;		/* 0 - close it done, 1 - keep on ready */
		}
	}

	/* only reached if request is split across more than one packet */
	return 1;
}

/*
 * Name: read_body
 * Description: Reads body from a request socket for POST CGI
 * 
 * Return values:
 * 
 *  -1: request blocked, move to blocked queue
 *   0: request done, close it down
 *   1: more to do, leave on ready list
 *
 *   At the moment, we are lenient with a POST request that comes
 *   in without a content length.  We read the network socket
 *   until we get an EWOUDLBOCK or EAGAIN, then assume that's the end.
 *   This works for short requests (less than a packet), but fails
 *   miserably for multi-packet requests.  Of course, it's their fault
 *   for not including the content-length so we can know when to quit.
 *   (Modified from Larry Doolittle's comment on the 0.92 POST patch)
 *   Note that filesize, if present, denotes the converted Content-Length
 */

int read_body(request * req)
{
	int bytes_read, bytes_to_read;

	bytes_to_read = BUFFER_SIZE - (req->header_end - req->header_line);

	if (req->filesize) {
		int bytes_left = req->filesize - req->filepos;
		if (bytes_left < bytes_to_read)
			bytes_to_read = bytes_left;
	}
	if (bytes_to_read <= 0) {
		req->status = BODY_WRITE;	/* go write it */
		return 1;
	}
	bytes_read = read(req->fd, 
			req->header_end,
			bytes_to_read);

	if (bytes_read == -1) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			if (req->filesize)
				return -1;
			else {
				req->status = BODY_WRITE;
				return 1;
			}
		} else {
			boa_perror(req, "read body");
			return 0;
		}
	} else if (bytes_read == 0) {
		req->status = BODY_WRITE;
		req->cgi_status = CLOSE;	/* init cgi when write finished */
	}
	req->header_end += bytes_read;

	if (bytes_read == bytes_to_read)
		req->status = BODY_WRITE;

	return 1;
}

/*
 * Name: write_body
 * Description: Writes a chunk of data to a file
 * 
 * Return values:
 *  -1: request blocked, move to blocked queue
 *   0: EOF or error, close it down
 *   1: successful write, recycle in ready queue
 */

int write_body(request * req)
{
	int bytes_written, bytes_to_write = req->header_end - req->header_line;

	if (bytes_to_write == 0) {	/* nothing left in buffer to write */
		req->header_line = req->header_end = req->buffer;
		if (req->cgi_status == CLOSE || req->filepos >= req->filesize)
			return init_cgi(req);	/* init_cgi should change status approp. */
		/* if here, we can safely assume that there is more to read */
		req->status = BODY_READ;
		return 1;
	}
	bytes_written = write(req->post_data_fd,
			req->header_line,
			bytes_to_write);

	if (bytes_written == -1)
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return -1;			/* request blocked at the pipe level, but keep going */
		else {
			boa_perror(req, "write body");	/* OK to disable if your logs get too big */
			return 0;
		}
		
	req->filepos += bytes_written;
	req->header_line += bytes_written;

	return 1;					/* more to do */
}
