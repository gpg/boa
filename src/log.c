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

/* boa: log.c */

#include "boa.h"

FILE * access_log;

char * error_log_name;
char * access_log_name;
char * aux_log_name;

/*
 * Name: open_logs
 * 
 * Description: Opens up the error log, ties it to stderr, and line 
 * buffers it.
 */

void open_logs()
{
    FILE * error_log;

    if(!(access_log = fopen(access_log_name, "a"))) {
        int errno_save = errno;
        fprintf(stderr, "Cannot open %s for logging: ", access_log_name);
        errno = errno_save;
        perror("logfile open");
        exit(1);
    }
    setvbuf(access_log, (char *)NULL, _IOLBF, 0);

    if(!error_log_name) {
	fputs("No ErrorLog directive specified in boa.conf.\n",stderr);
	exit(1);
    }

    if(!(error_log = fopen(error_log_name, "a")))
	die(NO_OPEN_LOG);

    /* redirect stderr to error_log */
    dup2(fileno(error_log), STDERR_FILENO);
    fclose(error_log);
}

/*
 * Name: close_access_log
 * 
 * Description: closes access_log file
 */
void close_access_log(void)
{
    fclose(access_log);
}

/*
 * Name: log_access
 * 
 * Description: Writes log data to access_log.
 */

void log_access(request * req)
{
    fprintf(access_log, "%s - - %s\"%s\" %d %ld\n",
        req->remote_ip_addr,
        get_commonlog_time(),
        req->logline,
        req->response_status,
        req->filepos
    );
}

/*
 * Name: log_error_time
 *
 * Description: Logs the current time to the stderr (the error log): 
 * should always be followed by an fprintf to stderr
 */

void log_error_time()
{
    int errno_save=errno;
    fputs(get_commonlog_time(),stderr);
    errno=errno_save;
}

/*
 * Name: log_error_doc
 *
 * Description: Logs the current time and transaction identification
 * to the stderr (the error log): 
 * should always be followed by an fprintf to stderr
 */

void log_error_doc(request * req)
{
    int errno_save=errno;
    fprintf(stderr, "%srequest from %s \"%s\": ",
        get_commonlog_time(),
        req->remote_ip_addr,
        req->logline);
    errno=errno_save;
}

/*
 * Name: boa_perror
 *
 * Description: logs an error to user and error file both
 *
 */
void boa_perror(request * req, char *message)
{
    int errno_save=errno;
    send_r_error(req);
    log_error_doc(req);
    errno = errno_save;
    perror(message);
    SQUASH_KA(req);   /* force close fd */
}
