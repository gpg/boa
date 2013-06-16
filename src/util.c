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

/* boa: util.c */

#include "boa.h"

time_t cur_time;

char * error_log_name;
char * access_log_name;
char * aux_log_name;

#define HEX_TO_DECIMAL(char1, char2)	\
  (((char1 >= 'A') ? (((char1 & 0xdf) - 'A') + 10) : (char1 - '0')) * 16) + \
  (((char2 >= 'A') ? (((char2 & 0xdf) - 'A') + 10) : (char2 - '0')))

/*
 * Name: req_write
 * 
 * Description: Buffers response header before sending to client.
 */

void req_write(request * req, char * msg)
{
    unsigned int msg_len = strlen(msg);

    if((req->response_len + msg_len) >= RESPONSEBUF_SIZE-1) {
	write(req->fd, req->response, req->response_len);
	req->response[0] = '\0';  /* fluff */
	req->response_len=0;
    }

    /* fail soft: shouldn't happen */
    if (msg_len >= RESPONSEBUF_SIZE) msg_len=RESPONSEBUF_SIZE-1;

    memcpy(req->response+req->response_len, msg, msg_len);
    req->response_len += msg_len;
    req->response[req->response_len] = '\0';  /* fluff */
}

/*
 * Name: flush_response
 * 
 * Description: Sends any backlogged response to client.
 */

void flush_response(request * req)
{
    if (req->response_len) {
	write(req->fd, req->response, req->response_len);
	req->response_len = 0;
	req->response[0] = '\0';  /* fluff */
    }
}

/*
 * Name: clean_pathname
 * 
 * Description: Replaces unsafe/incorrect instances of:
 *  //[...] with /
 *  /./ with /
 *  /../ with / (technically not what we want, but browsers should deal 
 *   with this, not servers)
 *
 * Stops parsing at '?'
 */

void clean_pathname(char * pathname)
{
    char * cleanpath, c;
    int cgiarg=0;

    cleanpath = pathname;
    while((c = *pathname++)) {
	if(c == '/' && !cgiarg) {
	    while(1) {
		if(*pathname == '/')
		    pathname++;
		else if(*pathname == '.' && *(pathname + 1) == '/')
		    pathname += 2;
		else if(*pathname == '.' && *(pathname + 1) == '.' &&
		  *(pathname + 2) == '/') {
		    pathname += 3;
		}
		else
		    break;
	    }
	    *cleanpath++ = '/';
	}
	else {
	    *cleanpath++ = c;
	    cgiarg |= ( c == '?' );
	}
    }

    *cleanpath = '\0';
}

	    
/*
 * Name: get_rfc822_time
 * 
 * Description: Returns time as specified by parameter in static char  
 * buffer.  If parameter is 0, returns current time.
 */

char * get_rfc822_time(time_t t)
{
    struct tm * gmt;
    static char timebuf[TIMEBUF_SIZE];

    if(!t) {					/* use current time */
	time(&cur_time);
	gmt = gmtime(&cur_time);
    }
    else
	gmt = gmtime(&t);

    /* time format per RFC 822/1123 <nels0988@tc.umn.edu> */
    strftime(timebuf, TIMEBUF_SIZE, "%a, %d %b %Y %T GMT", gmt);

    return timebuf;
}

/*
 * Name: modified_since
 * Description: Decides whether a file's mtime is newer than the 
 * If-Modified-Since header of a request.
 * 
 * RETURN VALUES:
 *  0: File has not been modified since specified time.
 *  1: File has been.
 */

int modified_since(time_t * mtime, char * if_modified_since)
{
    struct tm * file_gmt;
    char * ims_info;
    char monthname[MAX_HEADER_LENGTH + 1];
    int day, month, year, hour, minute, second;
    int comp;

    ims_info = if_modified_since;

    while(*ims_info++ != ' ');		/* skip day of week */

    if(sscanf(ims_info, "%d %3s %d %d:%d:%d GMT",             /* RFC 1123 */
      &day, (char *)&monthname, &year, &hour, &minute, &second) == 6) {
	year -= 1900;				/* this breaks come 2000 */
    }
    else if(sscanf(ims_info, "%d-%3s-%d %d:%d:%d GMT",    /* RFC 1036 */
      &day, (char *)&monthname, &year, &hour, &minute, &second) == 6);
    else if(sscanf(ims_info, "%3s %d %d:%d:%d %d",    /* asctime() format */
      (char *)&monthname, &day, &hour, &minute, &second, &year) == 6) {
	year -= 1900;
    }
    else 
      return -1;				/* error */

    file_gmt = gmtime(mtime);
    month = month2int(monthname);

    /* Go through from years to seconds -- if they are ever unequal,
       we know which one is newer and can return */

    if((comp = file_gmt->tm_year - year))
	return (comp > 0);
    if((comp = file_gmt->tm_mon - month))
	return (comp > 0);
    if((comp = file_gmt->tm_mday - day))
	return (comp > 0);
    if((comp = file_gmt->tm_hour - hour))
	return (comp > 0);
    if((comp = file_gmt->tm_min - minute))
	return (comp > 0);
    if((comp = file_gmt->tm_sec - second))
	return (comp > 0);

    return 0;    /* this person must really be into the latest/greatest */
}

/*
 * Name: month2int
 * 
 * Description: Turns a three letter month into a 0-11 int
 * 
 * Note: This function is from wn-v1.07 -- it's clever and fast
 */

int month2int(char * monthname)
{
    switch (*monthname) {
      case 'A':
        return ( *++monthname == 'p' ? 3 : 7);
      case 'D':
        return (11);
      case 'F':
        return (1);
      case 'J':
        if(*++monthname == 'a')
            return (0);
        return (*++monthname == 'n' ? 5 : 6);
      case 'M':
        return (*(monthname+2) == 'r' ? 2 : 4);
      case 'N':
        return (10);
      case 'O':
        return (9);
      case 'S':
        return (8);
      default:
        return (-1);
    }
}


/*
 * Name: to_upper
 * 
 * Description: Turns a string into all upper case (for HTTP_ header forming)
 * AND changes - into _ 
 */

char * to_upper(char * str)
{
    char * start = str;

    while(*str) {
        if(*str == '-')
            *str = '_';
        else 
            *str = toupper(*str);
    
        str++;
    }
	
    return start;
}

/*
 * Name: unescape_uri
 *
 * Description: Decodes a uri, changing %xx encodings with the actual 
 * character.  The query_string should already be gone.
 * 
 * Return values:
 *  1: success
 *  0: illegal string
 */

int unescape_uri(char * uri)
{
    char c, d;
    char * uri_old;

    uri_old = uri;

    while((c = *uri_old)) {
        if(c == '%') {
            uri_old++;
            if((c = *uri_old++) && (d = *uri_old++))
                *uri++ = HEX_TO_DECIMAL(c, d);
            else
                return 0;    /* NULL in chars to be decoded */
        }
        else {
            *uri++ = c;
            uri_old++;
        }
    }

    *uri = '\0';
    return 1;
}

/*
 * Name: escape_uri
 * 
 * Description: escapes unsafe characters as per RFC 1738.  Returns
 * pointer to static char buffer with escaped uri.  This is not done
 * right at the moment.
 */

char * escape_uri(char * uri)
{
    static char buffer[(MAX_PATH_LENGTH * 3) + 1];
    char c;
    char * bufp;

    bufp = buffer;

    while((c = *uri++)) {
	switch(c) {
	  case ' ':
	  case '<':
	  case '>':
	  case '"':
	  case '#':
	  case '%':
	  case '{':
	  case '}':
	  case '|':
	  case '\\':
	  case '^':
	  case '~':
	  case '[':
	  case ']':
	  case '`':

	  case '\'':

	  case '?':
	  /* case ':': */
	  case '@':
	  case '=':
	  case '&':
	    sprintf(bufp, "%%%X", (int)c);
	    bufp += 3;
	    break;
	  default:
	    *bufp++ = c;
	}
    }

    *bufp = '\0';
    return buffer;
}

/*
 *
 * Name: get_commonlog_time
 *
 * Description: Returns the current time in common log format in a static
 * char buffer.
 */

char * get_commonlog_time()
{
    struct tm * gmt;
    static char timebuf[TIMEBUF_SIZE];

    time(&cur_time);
    gmt = localtime(&cur_time);

    strftime(timebuf, TIMEBUF_SIZE, "[%d/%b/%Y:%H:%M:%S] ", gmt);

    return timebuf;
}

/*
 *
 * Name: create_tmp_dir
 *
 * Description: creates and chmod's tmp directory for boa.
 */ 

void create_tmp_dir()

{
    if (cachedir == NULL)
        return;
        
    if (mkdir(cachedir, S_IRUSR | S_IWUSR | S_IXUSR) && (errno != EEXIST))
	die(NO_CREATE_TMP);

    if (chmod(cachedir, S_IRUSR | S_IWUSR | S_IXUSR))
        die(WRONG_TMP_STAT);
}
