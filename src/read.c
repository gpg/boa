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
    char * check, * tmpfilep;
    char buffer[MAX_HEADER_LENGTH + 1];

    bytes = read(req->fd, buffer, MAX_HEADER_LENGTH);
    buf_bytes_left = bytes;

    if(bytes == -1) {
	if (errno == EINTR ) return 1;
	if (errno == EAGAIN || errno == EWOULDBLOCK) /* request blocked */
	    return -1;
	else if (errno == EBADF) {
   	    SQUASH_KA(req);   /* force close fd */
	    return 0;
	} else {
	    boa_perror(req, "header read");
	    return 0;
        }
    }

    if(bytes == 0)
	req->status = READ_BODY;

    check = buffer;

    while(check < (buffer + bytes)) {

	if(req->headerpos == MAX_HEADER_LENGTH) {
	    send_r_error(req);  
	    return 0;
	}

	switch(req->status) {
	  case READ_HEADER:
	    if(*check == '\r') 
		req->status = ONE_CR;
	    else if(*check == '\n') 
		req->status = ONE_LF;
	    else
		req->header_line[req->headerpos++] = *check;

	    break;

	  case ONE_CR:
	    if(*check == '\n')
		req->status = ONE_LF;
	    else {
		req->status = READ_HEADER;
		req->header_line[req->headerpos++] = *check;
	    }

	    break;

	  case ONE_LF:
	    if(*check == '\r')
		req->status = TWO_CR;
	    else if(*check == '\n')
		req->status = READ_BODY;
	    else {
		req->status = READ_HEADER;
		req->header_line[req->headerpos++] = *check;
	    }

	    break;

	  case TWO_CR:
	    if(*check == '\n')
		req->status = READ_BODY;
	    else {
		req->status = READ_HEADER;
		req->header_line[req->headerpos++] = *check;
	    }
	
	    break;

	  default:
	    break;
	}

	check++;
	buf_bytes_left--;

	if(req->status == ONE_LF) {
	    req->header_line[req->headerpos] = '\0';
	    req->headerpos = 0;

	    if(!*req->logline) {
		if(process_logline(req) == 0)
		    return 0;

		if(req->simple) {
		    req->status = WRITE;
		    if(req->method == M_GET && req->is_cgi) {
			init_cgi(req);
			return 0;
		    }
		    return init_get(req);
		}
	    }
	    else {
		process_header_line(req);
	    }
	}
	else if(req->status == READ_BODY) {
	    break;
	}
    }

    if(req->status == READ_BODY) {

	if(!*req->logline) {
	    send_r_error(req);
	    return 0;
	}

	if(req->method == M_POST) {

            tmpfilep = tmpnam(NULL);
	    if(!tmpfilep) {
		boa_perror(req, "tmpnam");
		return 0;
	    }

	    /* open temp file for post data */
            if((req->post_data_fd = open(tmpfilep, O_RDWR | O_CREAT)) == -1) {
		boa_perror(req, "tmpfile open");
		return 0;
	    }
	    req->post_file_name = strdup(tmpfilep);

	    if(write(req->post_data_fd, check, buf_bytes_left) != 
	      buf_bytes_left) {
	        boa_perror(req, "tmpfile write");
	        return 0;	
	    }
	}
	else {
	    req->status = WRITE;		/* skip READ_BODY */

	    if(req->method == M_GET && req->is_cgi) {
		init_cgi(req);
		return 0;
	    }
	   
	    return init_get(req);		/* get and head */
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
 */

int read_body(request * req)
{ 
    int bytes;
    char buffer[SOCKETBUF_SIZE + 1];

    bytes = read(req->fd, buffer, SOCKETBUF_SIZE);

    if(bytes <= 0) {
	if (errno == EINTR) {
	    return 1;
	} else if (bytes == 0 || errno == EAGAIN || errno == EWOULDBLOCK) {
            req->status = WRITE;
            init_cgi(req);
            return 0;
	} else if (errno == EBADF) { /* connection reset by peer */
	    return 0;
	} else {
	    boa_perror(req, "body read");
	    return 0;
	}
    }

    if(write(req->post_data_fd, buffer, bytes) != bytes) {
	boa_perror(req, "tmpfile write");
        return 0;
    }

    return 1;
}

