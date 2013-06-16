/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996,97 Larry Doolittle <ldoolitt@jlab.org>
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

/* boa: request.c */

#include "boa.h"

int sockbufsize = SOCKETBUF_SIZE;

extern int server_s;			/* boa socket */

/*
 * Name: new_request
 * Description: Obtains a request struct off the free list, or if the
 * free list is empty, allocates memory 
 * 
 * Return value: pointer to initialized request
 */

request *new_request(void)
{
	request *req;

	if (request_free) {
		req = request_free;		/* first on free list */
		dequeue(&request_free, request_free);	/* dequeue the head */
	} else {
		req = (request *) malloc(sizeof(request));
		if (!req)
			die(OUT_OF_MEMORY);
	}

	memset(req, 0, sizeof(request) - NO_ZERO_FILL_LENGTH);

	return req;
}


/*
 * Name: get_request
 * 
 * Description: Polls the server socket for a request.  If one exists, 
 * does some basic initialization and adds it to the ready queue;.
 */

void get_request(void)
{
	int fd;						/* socket */
	struct sockaddr_in remote_addr;		/* address */
	int remote_addrlen = sizeof(struct sockaddr_in);
	request *conn;				/* connection */

	remote_addr.sin_family = 0xdead;
	fd = accept(server_s, (struct sockaddr *) &remote_addr, &remote_addrlen);

	if (fd == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)	/* no requests */
			return;
		else {					/* accept error */
			log_error_time();
			perror("accept");
			return;
		}
	}
#ifdef DEBUGNONINET
	/*  This shows up due to race conditions in some Linux kernels 
	 *  when the client closes the socket sometime between 
	 *  the select() and accept() syscalls.
	 *  Code and description by Larry Doolittle <ldoolitt@jlab.org>
	 */
#define HEX(x) (((x)>9)?(('a'-10)+(x)):('0'+(x)))
	if (remote_addr.sin_family != AF_INET) {
		struct sockaddr *bogus = (struct sockaddr *) &remote_addr;
		char *ap, ablock[44];
		int i;
		close(fd);
		log_error_time();
		for (ap = ablock, i = 0; i < remote_addrlen && i < 14; i++) {
			*ap++ = ' ';
			*ap++ = HEX((bogus->sa_data[i] >> 4) & 0x0f);
			*ap++ = HEX(bogus->sa_data[i] & 0x0f);
		}
		*ap = '\0';
		fprintf(stderr, "non-INET connection attempt: socket %d, "
				"sa_family = %hu, sa_data[%d] = %s\n",
				fd, bogus->sa_family, remote_addrlen, ablock);
		return;
	}
#endif

	if ((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *) &sock_opt,
					sizeof(sock_opt))) == -1)
		die(NO_SETSOCKOPT);

	conn = new_request();
	conn->fd = fd;
	conn->status = READ_HEADER;
	conn->header_line = conn->client_stream;
	conn->time_last = time(NULL);

	/* nonblocking socket */
	if (fcntl(conn->fd, F_SETFL, NOBLOCK) == -1) {
		log_error_time();
		perror("request.c, fcntl");
	}
	/* large buffers */
	if (setsockopt(conn->fd, SOL_SOCKET, SO_SNDBUF, (void *) &sockbufsize,
				   sizeof(sockbufsize)) == -1)
		die(NO_SETSOCKOPT);

	/* for log file and possible use by CGI programs */
	strncpy(conn->remote_ip_addr, (char *) inet_ntoa(remote_addr.sin_addr), 20);

	/* for possible use by CGI programs */
	conn->remote_port = ntohs(remote_addr.sin_port);

	if (virtualhost) {
		struct sockaddr_in salocal;
		int dummy;

		dummy = sizeof(salocal);
		if (getsockname(conn->fd, (struct sockaddr *) &salocal, &dummy) == -1)
			die(SERVER_ERROR);
		conn->local_ip_addr = strdup(inet_ntoa(salocal.sin_addr));
	}	
	status.requests++;
	enqueue(&request_ready, conn);
}


/*
 * Name: free_request
 *
 * Description: Deallocates memory for a finished request and closes 
 * down socket.
 */

void free_request(request ** list_head_addr, request * req)
{
	if (req->buffer_end)
		return;
	
	dequeue(list_head_addr, req);	/* dequeue from ready or block list */
	
	if (req->buffer_end)
		FD_CLR(req->fd, &block_write_fdset);
	else {
		switch (req->status) {
		case PIPE_WRITE:
		case WRITE:
			FD_CLR(req->fd, &block_write_fdset);
			break;
		case PIPE_READ:
			FD_CLR(req->data_fd, &block_read_fdset);
			break;
		case BODY_WRITE:
			FD_CLR(req->post_data_fd, &block_write_fdset);
			break;
		default:
			FD_CLR(req->fd, &block_read_fdset);
		}
	}

	if (req->logline)			/* access log */
		log_access(req);

	if (req->data_mem)
		munmap(req->data_mem, req->filesize);
	
	if (req->data_fd)
			close(req->data_fd);
	
	if (req->response_status >= 400)
		status.errors++;

	if ((req->keepalive == KA_ACTIVE) &&
		(req->response_status < 400) &&
		(++req->kacount < ka_max)) {

		request *conn;

		conn = new_request();
		conn->fd = req->fd;
		conn->status = READ_HEADER;
		conn->header_line = conn->client_stream;
		conn->time_last = time(NULL);
		conn->kacount = req->kacount;
		
		/* we don't need to reset the fd parms for conn->fd because
		   we already did that for req */
		/* for log file and possible use by CGI programs */
		
		strcpy(conn->remote_ip_addr, req->remote_ip_addr);

		/* for possible use by CGI programs */
		conn->remote_port = req->remote_port;
		
		if (req->local_ip_addr)
			conn->local_ip_addr = strdup(req->local_ip_addr);

		status.requests++;
		
		if (conn->kacount + 1 == ka_max)
			SQUASH_KA(conn);
				
		conn->pipeline_start = req->client_stream_pos - 
								req->pipeline_start;
		
		if (conn->pipeline_start) {
			memcpy(conn->client_stream,
				req->client_stream + req->pipeline_start,
				conn->pipeline_start);			
			enqueue(&request_ready, conn);				
		} else
			block_request(conn);
	} else
		close(req->fd);

	if (req->cgi_env) {
		int i = COMMON_CGI_VARS;
		while (req->cgi_env[i])
			free(req->cgi_env[i++]);
		free(req->cgi_env);
	}
	if (req->pathname)
		free(req->pathname);
	if (req->path_info)
		free(req->path_info);
	if (req->path_translated)
		free(req->path_translated);
	if (req->script_name)
		free(req->script_name);
	if (req->query_string)
		free(req->query_string);
	if (req->local_ip_addr)
		free(req->local_ip_addr);
	
	enqueue(&request_free, req);	/* put request on the free list */

	return;
}


/*
 * Name: process_requests
 * 
 * Description: Iterates through the ready queue, passing each request 
 * to the appropriate handler for processing.  It monitors the
 * return value from handler functions, all of which return -1
 * to indicate a block, 0 on completion and 1 to remain on the
 * ready list for more procesing.
 */

void process_requests(void)
{
	int retval = 0;
	request *current, *trailer;

	current = request_ready;

	while (current) {
		if (current->buffer_end)
			retval = req_flush(current);
		else {
			switch (current->status) {
			case READ_HEADER:
			case ONE_CR:
			case ONE_LF:
			case TWO_CR:
				retval = read_header(current);
				break;
			case BODY_READ:
				retval = read_body(current);
				break;
			case BODY_WRITE:
				retval = write_body(current);
				break;
			case WRITE:
				retval = process_get(current);
				break;
			case PIPE_READ:
				retval = read_from_pipe(current);
				break;
			case PIPE_WRITE:
				retval = write_from_pipe(current);
				break;
			default:
				retval = 0;
				fputs("Unknown status!  Closing.\n", stderr);
				break;
			}
		}
		
		if (lame_duck_mode)
			SQUASH_KA(current);

		switch (retval) {
		case -1:				/* request blocked */
			trailer = current;
			current = current->next;
			block_request(trailer);
			break;
		case 0:				/* request complete */
			trailer = current;
			current = current->next;
			free_request(&request_ready, trailer);
			break;
		case 1:				/* more to do */
			current->time_last = time(NULL);
			current = current->next;
			break;
		default:
			break;
		}
	}
}

/*
 * Name: process_logline
 *
 * Description: This is called with the first req->header_line received
 * by a request, called "logline" because it is logged to a file.
 * It is parsed to determine request type and method, then passed to
 * translate_uri for further parsing.  Also sets up CGI environment if
 * needed.
 */

int process_logline(request * req)
{
	char *stop, *stop2;
	static char *SIMPLE_HTTP_VERSION = "HTTP/0.9";
	
	req->logline = req->header_line;
	if (!memcmp(req->logline, "GET ", 4))
		req->method = M_GET;
	else if (!memcmp(req->logline, "HEAD ", 5))		
		/* head is just get w/no body */
		req->method = M_HEAD;
	else if (!memcmp(req->logline, "POST ", 5))
		req->method = M_POST;
	else {
		log_error_time();
		fprintf(stderr, "malformed request: \"%s\"\n", req->logline);
		send_r_bad_request(req);
		return 0;
	}
	
	/* Guaranteed to find ' ' since we matched a method above */
	stop = req->logline + 3;
	if (*stop != ' ')
		++stop;
	
	/* scan to start of non-whitespace */
	while (*(++stop) == ' ');

	stop2 = stop;

	/* scan to end of non-whitespace */
	while (*stop2 != '\0' && *stop2 != ' ')
		++stop2;

	if (stop2 - stop > MAX_HEADER_LENGTH) {
		log_error_time();
		fprintf(stderr, "URI too long %d: \"%s\"\n", MAX_HEADER_LENGTH,
				req->logline);
		send_r_bad_request(req);
		return 0;
	}
	memcpy(req->request_uri, stop, stop2 - stop);
	req->request_uri[stop2 - stop] = '\0';

	if (*stop2 == ' ') {		
		/* if found, we should get an HTTP/x.x */
		int p1, p2;

		if (sscanf(++stop2, "HTTP/%d.%d", &p1, &p2) == 2 && p1 >= 1) {
			req->http_version = stop2;
			req->simple = 0;
		} else {
			log_error_time();
			fprintf(stderr, "bogus HTTP version: \"%s\"\n", stop2);
			send_r_bad_request(req);
			return 0;
		}
		
	} else {
		req->http_version = SIMPLE_HTTP_VERSION;
		req->simple = 1;
	}
	
	if (req->method == M_HEAD && req->simple) {
		send_r_bad_request(req);
		return 0;
	}

	if (translate_uri(req) == 0) {	/* unescape, parse uri */
		SQUASH_KA(req);
		return 0;		/* failure, close down */
	}
	if (req->is_cgi)
		create_env(req);
	return 1;
}

/*
 * Name: process_header_end
 *
 * Description: takes a request and performs some final checking before
 * init_cgi or init_get 
 * Returns 0 for error or NPH, or 1 for success
 */

int process_header_end(request * req)
{
	if (!req->logline) {
		send_r_error(req);
		return 0;
	}
	if (req->method == M_POST) {
		char *tmpfilep = tmpnam(NULL);

		if (!tmpfilep) {
			boa_perror(req, "tmpnam");
			return 0;
		}
		/* open temp file for post data */
		if ((req->post_data_fd = open(tmpfilep, O_RDWR | O_CREAT)) == -1) {
			boa_perror(req, "tmpfile open");
			return 0;
		}
		req->post_file_name = strdup(tmpfilep);
		return 1;
	}
	if (req->is_cgi)
		return init_cgi(req);
	req->status = WRITE;
	return init_get(req);		/* get and head */
}

/*
 * Name: process_option_line
 *
 * Description: Parses the contents of req->header_line and takes
 * appropriate action.
 */

void process_option_line(request * req)
{
	char c, *value, *line = req->header_line;

/* Start by aggressively hacking the in-place copy of the header line */

#ifdef FASCIST_LOGGING	
	fprintf(stderr, "\"%s\"\n", line);
#endif

	value = strchr(line, ':');
	if (value == NULL)
		return;
	*value++ = '\0';			/* overwrite the : */
	to_upper(line);				/* header types are case-insensitive */
	while ((c = *value) && (c == ' ' || c == '\t'))
		value++;

	if (!memcmp(line, "IF_MODIFIED_SINCE", 18) && !req->if_modified_since)
		req->if_modified_since = value;

	else if (!memcmp(line, "CONTENT_TYPE", 13) && !req->content_type)
		req->content_type = value;

	else if (!memcmp(line, "CONTENT_LENGTH", 15) && !req->content_length)
		req->content_length = value;

	else if (!memcmp(line, "CONNECTION", 11) &&
			 ka_max &&
			 req->keepalive != KA_STOPPED)
		req->keepalive = (!strncasecmp(value, "Keep-Alive", 10) ?
						  KA_ACTIVE : KA_STOPPED);
	
#ifdef ACCEPT_ON
	else if (!memcmp(line, "ACCEPT", 7))
		add_accept_header(req, value);
#endif		
	/* Silently ignore unknown header lines unless is_cgi */

	else if (req->is_cgi) {
		add_cgi_env(req, line, value);
	}	
	return;
}

/*
 * Name: add_accept_header
 * Description: Adds a mime_type to a requests accept char buffer
 *   silently ignore any that don't fit -
 *   shouldn't happen because of relative buffer sizes
 */

void add_accept_header(request * req, char *mime_type)
{
#ifdef ACCEPT_ON
	int l = strlen(req->accept);

	if ((strlen(mime_type) + l + 2) >= MAX_HEADER_LENGTH)
		return;

	if (req->accept[0] == '\0')
		strcpy(req->accept, mime_type);
	else {
		sprintf(req->accept + l, ", %s", mime_type);
	}
#endif
}

void free_requests(void)
		
{
	request *ptr, *next;
	
	ptr = request_free;
	while(ptr != NULL) {
		next = ptr->next;
		free(ptr);
		ptr = next;
	}
	request_free = NULL;
}
