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

/* boa: signals.c */

#include "boa.h"
#include <sys/wait.h>           /* wait */
#include <signal.h>             /* signal */

void sigsegv(int);
void sigbus(int);
void sigterm(int);
void sighup(int);
void sigint(int);
void sigchld(int);

/*
 * Name: init_signals
 * Description: Sets up signal handlers for all our friends.
 */

void init_signals(void)
{
    signal(SIGSEGV, sigsegv);
    signal(SIGBUS,  sigbus);
    signal(SIGTERM, sigterm);
    signal(SIGHUP,  sighup);
    signal(SIGINT,  sigint);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, sigchld);
}

void sigsegv(int dummy)
{
    log_error_time();
    fprintf(stderr, "caught SIGSEGV, dumping core\n");
    fclose(stderr);
    abort();
}

void sigbus(int dummy)
{
    log_error_time();
    fprintf(stderr, "caught SIGBUS, dumping core\n");
    fclose(stderr);
    abort();         
}

void sigterm(int dummy)
{
    lame_duck_mode=1;
}

void lame_duck_mode_run(void)
{
    log_error_time();
    fprintf(stderr, "caught SIGTERM, starting shutdown\n");
    lame_duck_mode=2;
}
    

void sighup(int dummy)
{
    signal(SIGHUP, sighup);
    sighup_flag=1;
}

void sighup_run(void)
{
    sighup_flag=0;
    log_error_time();
    fprintf(stderr, "caught SIGHUP, restarting\n");

    /* Philosophy change for 0.92: don't close and attempt reopen of logfiles,
     * since usual permission structure prevents such reopening.
     */

    read_config_files();

    log_error_time();
    fprintf(stderr, "successful restart\n");
}

void sigint(int dummy)
{
    log_error_time();
    fprintf(stderr, "caught SIGINT, shutting down\n");
    fclose(stderr);
    exit(1);
}

void sigchld(int dummy)
{
    signal(SIGCHLD, sigchld);
    sigchld_flag=1;
}

void sigchld_run(void)
{
    int status;
    pid_t pid;

    sigchld_flag=0;

    while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
#ifdef VERBOSE_CGI_LOGS
        log_error_time();
        fprintf(stderr, "Reaping child %d: status %d\n", pid, status);
#endif
    }

    return;
}

