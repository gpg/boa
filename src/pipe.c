/*
 *  Boa, an http server
 *  Based on code Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1997, 1998 Jon Nelson <nels0988@tc.umn.edu>
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
 * Name: read_from_pipe
 * Description: Reads data from a pipe
 * 
 * Return values:
 *  -1: request blocked, move to blocked queue
 *   0: EOF or error, close it down
 *   1: successful read, recycle in ready queue
 */

int read_from_pipe(request * req)
{
	int bytes_read, bytes_to_read = 
			BUFFER_SIZE - (req->header_end - req->buffer);
	
	if (bytes_to_read == 0) {	/* buffer full */
		req->status = PIPE_WRITE;
		if (req->cgi_status == READ_HEADER)
			return process_cgi_header(req);
		return 1;
	}
	
	bytes_read = read(req->data_fd, 
		req->header_end,
		bytes_to_read);
	
	if (bytes_read == -1) {
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return -1;			/* request blocked at the pipe level, but keep going */
		else {
			boa_perror(req, "pipe read");
			return 0;
		}
	} else if (bytes_read == 0) {	/* eof, write rest of buffer */
		req->status = PIPE_WRITE;
		if (req->cgi_status == READ_HEADER) {	/* hasn't processed header yet */
			req->cgi_status = CLOSE;
			return process_cgi_header(req);		/* cgi_status will change */
		}
		req->cgi_status = CLOSE;
		return 1;
	}
	req->header_end += bytes_read;
	return 1;
}

/*
 * Name: write_from_pipe
 * Description: Writes data previously read from a pipe
 * 
 * Return values:
 *  -1: request blocked, move to blocked queue
 *   0: EOF or error, close it down
 *   1: successful write, recycle in ready queue
 */

int write_from_pipe(request * req)
{
	int bytes_written, bytes_to_write = req->header_end - req->header_line;

	if (bytes_to_write == 0) {
		if (req->cgi_status == CLOSE)
			return 0;

		req->status = PIPE_READ;

		
				req->header_end = req->header_line = req->buffer;
		return 1;
	}
	bytes_written = write(req->fd,
			req->header_line,
			bytes_to_write);

	if (bytes_written == -1)
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return -1;			/* request blocked at the pipe level, but keep going */
		else {
			log_error_time();
			perror("pipe write");	/* OK to disable if your logs get too big */
			return 0;
		}
		
	req->header_line += bytes_written;
	req->filepos += bytes_written;

	return 1;
}
