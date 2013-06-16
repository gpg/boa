/*
 *  Boa, an http server
 *  cgi_header.c - cgi header parsing and control
 *  Copyright (C) 1997,98 Jon Nelson <nels0988@tc.umn.edu>
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

/* process_cgi_header

 * returns 0 -=> error or HEAD, close down.
 * returns 1 -=> done processing
 * leaves req->cgi_status as WRITE
 */

int process_cgi_header(request * req)
{
	char *buf;
	char *c;

	req->cgi_status = CGI_WRITE;
	buf = req->header_line;

	c = strstr(buf, "\n\r\n");
	if (c == NULL) {
		c = strstr(buf, "\n\n");
		if (c == NULL) {
			log_error_time();
			fputs("cgi_header: unable to find LFLF\n", stderr);
#ifdef FASCIST_LOGGING
			log_error_time();
			fprintf(stderr, "\"%s\"\n", buf);
#endif
			send_r_error(req);
			return 0;
		}
	}
	if (req->simple) {
		if (*(c + 1) == '\r')
			req->header_line = c + 2;
		else
			req->header_line = c + 1;
		return 1;
	}
	if (!strncasecmp(buf, "Status: ", 8)) {
		req->header_line--;
		memcpy(req->header_line, "HTTP/1.0 ", 9);
	} else if (!strncasecmp(buf, "Location: ", 10)) {	/* got a location header */
		c = buf + 10;
		while (*c != '\n' && *c != '\r' && c < req->data_mem + MAX_HEADER_LENGTH)
			++c;
		*c = '\0';

		if (buf[10] == '/') {	/* virtual path -=> not url */
			log_error_time();
			fprintf(stderr, "server does not support internal redirection: " \
					"\"%s\"\n", buf + 10);
			send_r_error(req);
			return 0;

			/* 
			 * We (I, Jon) have declined to support absolute-path parsing
			 * because I see it as a major security hole.
			 * Location: /etc/passwd or Location: /etc/shadow is not funny.
			 */

			/*
			   strcpy(req->request_uri, buf + 10);
			   return internal_redirect(req); 
			 */
		} else {				/* URL */
			send_redirect_temp(req, buf + 10);
			return 0;
		}
	} else
		send_r_request_ok(req);	/* does not terminate */

	if (req->method == M_HEAD) {
		*c = '\0';				/* terminate headers */
		req_write(req, req->header_line);
		req_write(req, "\r\n\r\n");
		req_flush(req);
		return 0;
	} else
		return 1;
}
