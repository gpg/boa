%{

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

#include "y.tab.h"
#include <stdlib.h>
#include <unistd.h>

char *mime_types = NULL;

static int file = 0;
int lineno = 1;
%}

%a 4000
%p 4000

%s MIME
%%

[ \t]+		;
#.*		;

<MIME>[^ \t\n]+\/[^ \t\n]+	{ yylval.sval = yytext; return MIMETYPE; }

Port		{ BEGIN INITIAL; return B_PORT; }
BackLog         { BEGIN INITIAL; return B_BACKLOG; }
User		{ BEGIN INITIAL; return B_USER; }
Group		{ BEGIN INITIAL; return B_GROUP; }
ServerAdmin	{ BEGIN INITIAL; return B_SERVERADMIN; }
ServerRoot	{ BEGIN INITIAL; return B_SERVERROOT; }
ErrorLog	{ BEGIN INITIAL; return B_ERRORLOG; }
AccessLog	{ BEGIN INITIAL; return B_ACCESSLOG; }
CgiLog		{ BEGIN INITIAL; return B_CGILOG; }
VerboseCGILogs	{ BEGIN INITIAL; return B_VERBOSECGILOGS; }
ServerName	{ BEGIN INITIAL; return B_SERVERNAME; }
Redirect	{ BEGIN INITIAL; return B_REDIRECT; }

DocumentRoot	{ BEGIN INITIAL; return B_DOCUMENTROOT; }
UserDir		{ BEGIN INITIAL; return B_USERDIR; }
DirectoryIndex	{ BEGIN INITIAL; return B_DIRECTORYINDEX; }
DirectoryMaker	{ BEGIN INITIAL; return B_DIRECTORYMAKER; }
KeepAliveMax 	{ BEGIN INITIAL; return B_KEEPALIVEMAX; }
KeepAliveTimeout { BEGIN INITIAL; return B_KEEPALIVETIMEOUT; }
MimeTypes	{ BEGIN INITIAL; return B_MIMETYPES; }
DefaultType	{ BEGIN INITIAL; return B_DEFAULTTYPE; }
AddType		{ BEGIN MIME; return B_ADDTYPE; }
ScriptAlias	{ BEGIN INITIAL; return B_SCRIPTALIAS; }
Alias		{ BEGIN INITIAL; return B_ALIAS; }

-?[0-9]+	{ yylval.ival = atoi(yytext); return INTEGER; }
[^ \t\n]+	{ yylval.sval = yytext; return STRING; }

\n		{ lineno++; }
%%

/* In yywrap we track which file we are on.
 * 1: close boa.conf, open mime.types
 * 2: return 1;
 */

int yywrap()
{
    fclose(yyin);
    file++; 

    switch(file) {
      case 1:
	yyin = fopen(mime_types, "r");
	if(!yyin) {
	    fprintf(stderr, "Could not open mime.types file, \"%s\", "
	    	"for reading\n", mime_types);
	    exit(1);
	}
	BEGIN MIME;
	return 0;
      default:
	BEGIN INITIAL;
	file = 0;		/* in case we reread config files */
	return 1;
    }
}

int yyerror(char * msg)
{
    fprintf(stderr, "Error on line %d of %s: %s\n", lineno, 
      (file == 0 ? "boa.conf" : "mime.types"), msg);
    return 1;
}

