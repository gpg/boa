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
#include <netdb.h>

int yyparse(void);				/* Better match the output of lex */

int server_port;
uid_t server_uid;
gid_t server_gid;
char *server_admin;
char *server_root;
char *server_name;
int virtualhost;

char *document_root;
char *user_dir;
char *directory_index;
char *default_type;
char *dirmaker;

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
	struct hostent *hostentbuf;

	server_port = 0;
	virtualhost = 0;
	ka_timeout = 0;
	ka_max = 0;
	verbose_cgi_logs = 0;

	yyin = fopen("boa.conf", "r");

	if (!yyin) {
		fputs("Could not open boa.conf for reading.\n", stderr);
		exit(1);
	}
	if (yyparse()) {
		fputs("Error parsing config files, exiting\n", stderr);
		exit(1);
	}
	if (!server_name) {
		gethostname(hostnamebuf, MAX_SITENAME_LENGTH);
		hostentbuf = gethostbyname(hostnamebuf);
		if (!hostentbuf) {
			fputs("Cannot determine hostname. Set ServerName in boa.conf.\n",
				  stderr);
			exit(1);
		}
		server_name = strdup(hostentbuf->h_name);
	}
}
