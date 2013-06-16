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

/* boa: config.c */

#include "boa.h"

int yyparse (void);    /* Better match the output of lex */

int server_port;
UID_T server_uid;
GID_T server_gid;
char * server_admin;
char * server_root;
char * server_name;

char * document_root;
char * user_dir;
char * directory_index;
char * default_type;
char * cachedir;

int ka_timeout;
int ka_max;

/*
 * Name: read_config_files
 * 
 * Description: Reads config files via yyparse, then makes sure that 
 * all required variables were set properly.
 */

void read_config_files()
{
    char hostnamebuf[MAX_SITENAME_LENGTH + 1];
    struct hostent * hostentbuf;

    yyin = fopen("conf/boa.conf", "r");

    if(!yyin) {
	fprintf(stderr, "Could not open boa.conf for reading.\n");
	exit(1);
    }

    if(yyparse()) {
	fprintf(stderr, "Error parsing config files, exiting\n");
	exit(1);
    }

    if(!server_name) {
	gethostname(hostnamebuf, MAX_SITENAME_LENGTH);
	hostentbuf = gethostbyname(hostnamebuf);
	if(!hostentbuf) {
	    fprintf(stderr, "Cannot determine hostname.  ");
            fprintf(stderr, "Set ServerName in srm.conf.\n");
	    exit(1);
        }
	server_name = strdup(hostentbuf->h_name);
    }
}

