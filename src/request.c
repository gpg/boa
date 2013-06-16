/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@cebaf.gov>
 *  Some changes Copyright (C) 1996 Jon Nelson <nels0988@tc.umn.edu>
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

extern int server_s;                           /* boa socket */

static request null_request = 
	{ 0, 0, 0, 0, 0,
	  0, 0, NULL, 0, 0, 0,
	  "", "", "", "",
	  "", 0,
	  "", 0, 0, 0,
	  NULL, NULL, NULL, NULL, NULL, 0,
	  0,
	  0, NULL, 0,
	  0,
	  NULL, NULL, NULL, NULL, NULL, NULL,
	  0,
	  NULL, NULL, NULL
	};


/*
 * Name: new_request
 * Description: Obtains a request struct off the free list, or if the
 * free list is empty, allocates memory 
 * 
 * Return value: pointer to initialized request
 */

request * new_request(void)
{
    request * req;

    if(request_free) {
	req = request_free;			/* first on free list */
	dequeue(&request_free, request_free);	/* dequeue the head */
    }
    else {
	req = (request *)malloc(sizeof(request));
	if(!req)
	    die(OUT_OF_MEMORY);
    }

    *req = null_request;			/* initalize */

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
    int fd;					/* socket */
    struct sockaddr_in remote_addr;		/* address */
    int remote_addrlen = sizeof(struct sockaddr_in);
    request * conn;				/* connection */

    fd = accept(server_s, (struct sockaddr *)&remote_addr, &remote_addrlen);

    if(fd == -1) {
	if(errno == EAGAIN || errno == EWOULDBLOCK) /* no requests */
            return;
	else {                                      /* accept error */
	    log_error_time();
	    perror("accept");
            return;
	}
    }

    if((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&true,
      sizeof(true))) == -1)
	die(NO_SETSOCKOPT);

    conn = new_request();
    conn->fd = fd;
    conn->status = READ_HEADER;
    conn->time_last = time(NULL);

    /* nonblocking socket */
    fcntl(conn->fd, F_SETFL, NOBLOCK);

    /* large buffers */
    if(setsockopt(conn->fd, SOL_SOCKET, SO_SNDBUF, (void *) &sockbufsize, 
      sizeof(sockbufsize)) == -1)
	die(NO_SETSOCKOPT);

    /* for log file and possible use by CGI programs */
    conn->remote_ip_addr = strdup((char *)inet_ntoa(remote_addr.sin_addr));

    /* for possible use by CGI programs */
    conn->remote_port = ntohs(remote_addr.sin_port);

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
    dequeue(list_head_addr, req);	/* dequeue from ready or block list */

    if(*req->logline && req->response_status)	/* access log */
    	log_access(req);

    if(req->data_mem)			/* unmap data memory */
	munmap(req->data_mem, req->filesize);

    if ( (req->keepalive == 3) && (req->response_status < 300)
      && (req->kacount + 1 < ka_max) )  {
        request *conn;
        
        conn = new_request();
        conn->fd = req->fd;
        conn->status = READ_HEADER;
        conn->time_last = req->time_last;
        conn->kacount = req->kacount + 1;

        /* for log file and possible use by CGI programs */
        conn->remote_ip_addr = strdup(req->remote_ip_addr);

        /* for possible use by CGI programs */
        conn->remote_port = req->remote_port;

        enqueue(&request_ready, conn);
    }
    else
        close(req->fd);

    if(req->cgi_env) {
	int i = COMMON_CGI_VARS;
	while(req->cgi_env[i]) {
	    free(req->cgi_env[i++]);
	}
	free(req->cgi_env);
    }
 
    if(req->ret_content_type)
	free(req->ret_content_type);
    if(req->if_modified_since)
	free(req->if_modified_since);
    if(req->user_agent)
	free(req->user_agent);
    if(req->referer)
	free(req->referer);
    if(req->remote_ip_addr)
	free(req->remote_ip_addr);

    if(req->path_info)
	free(req->path_info);
    if(req->path_translated)
	free(req->path_translated);
    if(req->script_name)
	free(req->script_name);
    if(req->query_string)
	free(req->query_string);
    if(req->content_type)
	free(req->content_type);
    if(req->content_length)
	free(req->content_length);
    if(req->post_file_name)
	free(req->post_file_name);

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
    int retval=0;
    request * current, * trailer;

    current = request_ready;

    while(current) {
	switch(current->status) {
	    case READ_HEADER:
	    case ONE_CR:
	    case ONE_LF:
	    case TWO_CR:
		retval = read_header(current);
		break;
	    case READ_BODY:
		retval = read_body(current);
		break;
	    case WRITE:
		if(current->method == M_GET)		/* data from file */
		     retval = process_get(current);
		else {
		     SQUASH_KA(current);
		     retval = 0;			/* other methods? */
		}
		break;
	    default:
		break;
	}

      	if (lame_duck_mode)
	    SQUASH_KA(current);

        switch(retval) {
	    case -1:					/* request blocked */
		trailer = current;
		current = current->next;
		block_request(trailer);
		break;
	    case 0:					/* request complete */
		trailer = current;
		current = current->next;
		free_request(&request_ready, trailer);
		break;
	    case 1:					/* more to do */
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
    int matches;
    char method[MAX_HEADER_LENGTH + 1];

    strcpy(req->logline, req->header_line);

    matches = sscanf(req->logline, "%s %s HTTP/%7s", 
      method, req->request_uri, req->http_version);

    switch(matches) {
      case 3:						/* HTTP/1.0 */
	if (strcmp(req->http_version,"0.9")==0) req->simple=1;
	break;
      case 2:						/* HTTP/0.9 */
	req->simple = 1;
	break;
      default:
	send_r_bad_request(req);
        log_error_time();
	fprintf(stderr, "malformed request: \"%s\"\n", req->logline);
	return 0;
    }

    if(!strcmp(method, "GET")) 
	req->method = M_GET;
    else if(!strcmp(method, "HEAD"))  	/* head is just get w/no body */
	req->method = M_HEAD;
    else if(!strcmp(method, "POST")) 
	req->method = M_POST;
    else {
	send_r_bad_request(req);
        log_error_time();
	fprintf(stderr, "unknown method: \"%s\"\n", method);
	return 0;
    }

    if(translate_uri(req) == 0) {       /* unescape, parse uri */
        SQUASH_KA(req);                 /* force close fd */
	return 0;			/* failure, close down */
    }

    if(req->is_cgi)
        create_env(req);    

    return 1;
}

/*
 * Name: process_header_line
 *
 * Description: Parses the contents of req->header_line and takes
 * appropriate action.
 */

void process_header_line(request * req) 
{
    char c, * value, * line = req->header_line;

/* Start by aggressively hacking the in-place copy of the header line */

    value=strchr(line,':');
    if (value == NULL) return;
    *value++='\0';   /* overwrite the : */
    to_upper(line);  /* header types are case-insensitive */
    while((c = *value) && (c == ' ' || c == '\t'))
          value++;

    if(!memcmp(line, "ACCEPT", 7))
	add_accept_header(req, value);

    else if(!memcmp(line, "IF_MODIFIED_SINCE", 18)&&!req->if_modified_since)
	req->if_modified_since = strdup(value);

    else if(!memcmp(line, "REFERER", 8)&&!req->referer)
	req->referer = strdup(value);

    else if(!memcmp(line, "USER_AGENT", 11)&&!req->user_agent)
	req->user_agent = strdup(value);

    else if(!memcmp(line, "CONTENT_TYPE", 13)&&!req->content_type)
	req->content_type = strdup(value);

    else if(!memcmp(line, "CONTENT_LENGTH", 15)&&!req->content_length)
	req->content_length = strdup(value);

    else if(!memcmp(line, "CONNECTION", 11) && ka_max)
	req->keepalive = (!strncasecmp(value, "Keep-Alive", 10) ? 3 : 2);

/* Silently ignore unknown header lines unless is_cgi */

    else if(req->is_cgi) {
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

void add_accept_header(request * req, char * mime_type)
{
    int l=strlen(req->accept);

    if((strlen(mime_type) + l + 2) >= MAX_HEADER_LENGTH)
	return;

    if(req->accept[0] == '\0') {
	strcpy(req->accept, mime_type);
    }
    else {
	memcpy(req->accept+l, ", ", 2);
	strcpy(req->accept+l+2, mime_type);
    }
}

