/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996,97 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1996 Charles F. Randall <crandall@goldsys.com>
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

/* $Id: util.c,v 1.46 2001/07/18 03:43:38 jnelson Exp $ */

#include "boa.h"

#define HEX_TO_DECIMAL(char1, char2)	\
    (((char1 >= 'A') ? (((char1 & 0xdf) - 'A') + 10) : (char1 - '0')) * 16) + \
    (((char2 >= 'A') ? (((char2 & 0xdf) - 'A') + 10) : (char2 - '0')))

const char month_tab[48] =
    "Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec ";
const char day_tab[] = "Sun,Mon,Tue,Wed,Thu,Fri,Sat,";

/*
 * Name: strstr and strdup
 *
 * These are the standard library utilities.  We define them here for
 * systems that don't have them.
 */
#ifndef HAVE_STRSTR
char *strstr(char *s1, char *s2)
{                               /* from libiberty */
    register char *p;
    register int len = strlen(s2);

    if (*s2 == '\0')            /* everything matches empty string */
        return s1;
    for (p = s1; (p = strchr(p, *s2)) != NULL; p = strchr(p + 1, *s2)) {
        if (strncmp(p, s2, len) == 0)
            return (p);
    }
    return NULL;
}
#endif

#ifndef HAVE_STRDUP
char *strdup(char *s)
{
    char *retval;

    retval = (char *) malloc(strlen(s) + 1);
    if (retval == NULL) {
        perror("boa: out of memory in strdup");
        exit(1);
    }
    return strcpy(retval, s);
}
#endif


/*
 * Name: clean_pathname
 *
 * Description: Replaces unsafe/incorrect instances of:
 *  //[...] with /
 *  /./ with /
 *  /../ with / (technically not what we want, but browsers should deal
 *   with this, not servers)
 */

void clean_pathname(char *pathname)
{
    char *cleanpath, c;

    cleanpath = pathname;
    while ((c = *pathname++)) {
        if (c == '/') {
            while (1) {
                if (*pathname == '/')
                    pathname++;
                else if (*pathname == '.' && *(pathname + 1) == '/')
                    pathname += 2;
                else if (*pathname == '.' && *(pathname + 1) == '.' &&
                         *(pathname + 2) == '/') {
                    pathname += 3;
                } else
                    break;
            }
            c = '/';
        }
        *cleanpath++ = c;
    }

    *cleanpath = '\0';
}

/*
 * Name: get_commonlog_time
 *
 * Description: Returns the current time in common log format in a static
 * char buffer.
 *
 * commonlog time is exactly 25 characters long
 * because this is only used in logging, we add " [" before and "] " after
 * making 29 characters
 * "[27/Feb/1998:20:20:04 +0000] "
 *
 * Altered 10 Jan 2000 by Jon Nelson ala Drew Streib for non UTC logging
 *
 */

char *get_commonlog_time(void)
{
    struct tm *t;
    char *p;
    unsigned int a;
    static char buf[30];
    int time_offset;

    if (use_localtime) {
        t = localtime(&current_time);
        time_offset = timezone;
    } else {
        t = gmtime(&current_time);
        time_offset = 0;
    }

    p = buf + 29;
    *p-- = '\0';
    *p-- = ' ';
    *p-- = ']';
    a = abs(time_offset / 60);
    *p-- = '0' + a % 10;
    a /= 10;
    *p-- = '0' + a % 6;
    a /= 6;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = (time_offset > 0) ? '-' : '+';
    *p-- = ' ';

    a = t->tm_sec;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = ':';
    a = t->tm_min;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = ':';
    a = t->tm_hour;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = ':';
    a = 1900 + t->tm_year;
    while (a) {
        *p-- = '0' + a % 10;
        a /= 10;
    }
    /* p points to an unused spot */
    *p-- = '/';
    p -= 2;
    memcpy(p--, month_tab + 4 * (t->tm_mon), 3);
    *p-- = '/';
    a = t->tm_mday;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p = '[';
    return p;                   /* should be same as returning buf */
}

/*
 * Name: month2int
 *
 * Description: Turns a three letter month into a 0-11 int
 *
 * Note: This function is from wn-v1.07 -- it's clever and fast
 */

int month2int(char *monthname)
{
    switch (*monthname) {
    case 'A':
        return (*++monthname == 'p' ? 3 : 7);
    case 'D':
        return (11);
    case 'F':
        return (1);
    case 'J':
        if (*++monthname == 'a')
            return (0);
        return (*++monthname == 'n' ? 5 : 6);
    case 'M':
        return (*(monthname + 2) == 'r' ? 2 : 4);
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
 * Name: modified_since
 * Description: Decides whether a file's mtime is newer than the
 * If-Modified-Since header of a request.
 *

 Sun, 06 Nov 1994 08:49:37 GMT    ; RFC 822, updated by RFC 1123
 Sunday, 06-Nov-94 08:49:37 GMT   ; RFC 850, obsoleted by RFC 1036
 Sun Nov  6 08:49:37 1994         ; ANSI C's asctime() format

 * RETURN VALUES:
 *  0: File has not been modified since specified time.
 *  1: File has been.
 * -1: Error!
 */

int modified_since(time_t * mtime, char *if_modified_since)
{
    struct tm *file_gmt;
    char *ims_info;
    char monthname[3 + 1];
    int day, month, year, hour, minute, second;
    int comp;

    ims_info = if_modified_since;
    while (*ims_info != ' ' && *ims_info != '\0')
        ++ims_info;
    if (*ims_info != ' ')
        return -1;

    /* the pre-space in the third scanf skips whitespace for the string */
    if (sscanf(ims_info, "%d %3s %d %d:%d:%d GMT", /* RFC 1123 */
               &day, monthname, &year, &hour, &minute, &second) == 6);
    else if (sscanf(ims_info, "%d-%3s-%d %d:%d:%d GMT", /* RFC 1036 */
                    &day, monthname, &year, &hour, &minute, &second) == 6)
        year += 1900;
    else if (sscanf(ims_info, " %3s %d %d:%d:%d %d", /* asctime() format */
                    monthname, &day, &hour, &minute, &second, &year) == 6);
    else {
        log_error_time();
        fprintf(stderr, "Error in %s, line %d: Unable to sscanf \"%s\"\n",
                __FILE__, __LINE__, ims_info);
        return -1;              /* error */
    }

    file_gmt = gmtime(mtime);
    month = month2int(monthname);

    /* Go through from years to seconds -- if they are ever unequal,
       we know which one is newer and can return */

    if ((comp = 1900 + file_gmt->tm_year - year))
        return (comp > 0);
    if ((comp = file_gmt->tm_mon - month))
        return (comp > 0);
    if ((comp = file_gmt->tm_mday - day))
        return (comp > 0);
    if ((comp = file_gmt->tm_hour - hour))
        return (comp > 0);
    if ((comp = file_gmt->tm_min - minute))
        return (comp > 0);
    if ((comp = file_gmt->tm_sec - second))
        return (comp > 0);

    return 0;                   /* this person must really be into the latest/greatest */
}


/*
 * Name: to_upper
 *
 * Description: Turns a string into all upper case (for HTTP_ header forming)
 * AND changes - into _
 */

char *to_upper(char *str)
{
    char *start = str;

    while (*str) {
        if (*str == '-')
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

int unescape_uri(char *uri, char ** query_string)
{
    char c, d;
    char *uri_old;

    uri_old = uri;

    while ((c = *uri_old)) {
        if (c == '%') {
            uri_old++;
            if ((c = *uri_old++) && (d = *uri_old++))
                *uri++ = HEX_TO_DECIMAL(c, d);
            else
                return 0;       /* NULL in chars to be decoded */
        } else if (c == '?') { /* query string */
            if (query_string)
                *query_string = ++uri_old;
            /* stop here */
            *uri = '\0';
            return(1);
            break;
        } else if (c == '#') { /* fragment */
            /* legal part of URL, but we do *not* care.
             * However, we still have to look for the query string */
            if (query_string) {
                ++uri_old;
                while((c = *uri_old)) {
                    if (c == '?') {
                        *query_string = ++uri_old;
                        break;
                    }
                    ++uri_old;
                }
            }
            break;
        } else {
            *uri++ = c;
            uri_old++;
        }
    }

    *uri = '\0';
    return 1;
}

/* rfc822 (1123) time is exactly 29 characters long
 * "Sun, 06 Nov 1994 08:49:37 GMT"
 */

void rfc822_time_buf(char *buf, time_t s)
{
    struct tm *t;
    char *p;
    unsigned int a;

    if (!s) {
        t = gmtime(&current_time);
    } else
        t = gmtime(&s);

    p = buf + 28;
    /* p points to the last char in the buf */

    p -= 3;
    /* p points to where the ' ' will go */
    memcpy(p--, " GMT", 4);

    a = t->tm_sec;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = ':';
    a = t->tm_min;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = ':';
    a = t->tm_hour;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = ' ';
    a = 1900 + t->tm_year;
    while (a) {
        *p-- = '0' + a % 10;
        a /= 10;
    }
    /* p points to an unused spot to where the space will go */
    p -= 3;
    /* p points to where the first char of the month will go */
    memcpy(p--, month_tab + 4 * (t->tm_mon), 4);
    *p-- = ' ';
    a = t->tm_mday;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = ' ';
    p -= 3;
    memcpy(p, day_tab + t->tm_wday * 4, 4);
}

char *simple_itoa(int i)
{
    /* 21 digits plus null terminator, good for 64-bit or smaller ints */
    /* for bigger ints, use a bigger buffer! */
    static char local[22];
    char *p = &local[21];
    *p-- = '\0';
    do {
        *p-- = '0' + i % 10;
        i /= 10;
    } while (i > 0);
    return p + 1;
}
