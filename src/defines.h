/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
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

#ifndef _DEFINES_H
#define _DEFINES_H

/***** Change this, or use -c on the command line to specify it *****/

#define SERVER_ROOT "/etc/boa"

/***** Various stuff that you may want to tweak, but probably shouldn't *****/

#define SOCKETBUF_SIZE				4096
#define RESPONSEBUF_SIZE			1024
#define MAX_HEADER_LENGTH			SOCKETBUF_SIZE

#define MIME_HASHTABLE_SIZE			47
#define ALIAS_HASHTABLE_SIZE			17
#define PASSWD_HASHTABLE_SIZE			47
#define DNS_HASHTABLE_SIZE			117

#define REQUEST_TIMEOUT				60

#define CGI_MIME_TYPE    "application/x-httpd-cgi"
#define DEFAULT_PATH     "/bin:/usr/bin:/usr/ucb"

/***** CHANGE ANYTHING BELOW THIS LINE AT YOUR OWN PERIL *****/
/***** You will probably introduce buffer overruns unless you know
       what you are doing *****/

#define TIMEBUF_SIZE				64

#define MAX_SITENAME_LENGTH			256
#define MAX_CGI_VARS				50
#define MAX_LOG_LENGTH				MAX_HEADER_LENGTH + 1024
#define MAX_FILE_LENGTH				256
#define MAX_PATH_LENGTH				512

#define COMMON_CGI_VARS				8

#define SERVER_VERSION				"Boa/0.92o"
#define CGI_VERSION				"CGI/1.1"

/******************* RESPONSE CLASSES *****************/

#define R_INFORMATIONAL	1
#define R_SUCCESS	2
#define R_REDIRECTION	3	
#define R_CLIENT_ERROR	4
#define R_SERVER_ERROR	5

/******************* RESPONSE CODES ******************/

#define R_REQUEST_OK	200
#define R_CREATED	201
#define R_ACCEPTED	202
#define R_PROVISIONAL	203	/* provisional information */
#define R_NO_CONTENT	204

#define R_MULTIPLE	300	/* multiple choices */
#define R_MOVED_PERM	301
#define R_MOVED_TEMP	302
#define R_NOT_MODIFIED	304

#define R_BAD_REQUEST	400
#define R_UNAUTHORIZED	401
#define R_PAYMENT	402	/* payment required */
#define R_FORBIDDEN	403
#define R_NOT_FOUND	404
#define R_METHOD_NA	405	/* method not allowed */
#define R_NONE_ACC	406	/* none acceptable */
#define R_PROXY		407	/* proxy authentication required */
#define R_REQUEST_TO	408	/* request timeout */
#define R_CONFLICT	409
#define R_GONE		410

#define R_ERROR		500	/* internal server error */
#define	R_NOT_IMP	501	/* not implemented */
#define	R_BAD_GATEWAY	502	
#define R_SERVICE_UNAV	503	/* service unavailable */
#define	R_GATEWAY_TO	504	/* gateway timeout */

/****************** METHODS *****************/

#define M_GET		1 
#define M_HEAD		2
#define M_PUT		3
#define M_POST		4
#define M_DELETE	5
#define M_LINK		6
#define M_UNLINK	7

/******************* ERRORS *****************/

#define SERVER_ERROR		1
#define OUT_OF_MEMORY		2
#define NO_CREATE_SOCKET	3
#define NO_FCNTL		4
#define NO_SETSOCKOPT		5
#define NO_BIND			6
#define NO_LISTEN		7
#define NO_SETGID		8
#define NO_SETUID		9
#define NO_OPEN_LOG		10
#define NO_CREATE_TMP		11
#define WRONG_TMP_STAT		12

/************** REQUEST STATUS (req->status) ***************/

#define WRITE                   0
#define READ_HEADER             1
#define READ_BODY               2
#define ONE_CR			3
#define ONE_LF			4
#define TWO_CR			5

/************** CGI STATUS (req->is_cgi) ******************/

#define CGI                     1
#define NPH                     2

/************** ALIAS TYPES (aliasp->type) ***************/

#define ALIAS			0
#define SCRIPTALIAS		1
#define REDIRECT		2

#endif
