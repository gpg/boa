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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>		/* struct passwd */
#include <grp.h>		/* struct group */
#include "globals.h"

int yyerror(char * msg);

void add_alias(char * fakename, char * realname, int script);
void add_mime_type(char * extension, char * type);

struct passwd * passwdbuf;
struct group * groupbuf;
char mime_type[256];		/* global to inherit */
char fakename[256];		/* ScriptAlias */

/* yydebug = 1; */

%}

%union {
    char *	sval;
    int		ival;
};

/* boa.conf tokens */
%token B_PORT B_USER B_GROUP
%token B_SERVERADMIN B_SERVERROOT B_ERRORLOG B_ACCESSLOG
%token B_CGILOG B_VERBOSECGILOGS
%token B_SERVERNAME
%token B_VIRTUALHOST

/* srm.conf tokens */
%token B_DOCUMENTROOT B_USERDIR B_DIRECTORYINDEX
%token B_DIRECTORYMAKER
%token B_MIMETYPES B_DEFAULTTYPE B_ADDTYPE
%token B_ALIAS B_SCRIPTALIAS B_REDIRECT 
%token B_KEEPALIVEMAX B_KEEPALIVETIMEOUT

%token <sval> MIMETYPE
%token <sval> STRING
%token <ival> INTEGER

%start ConfigFiles

%%

ConfigFiles:		BoaConfigStmts MimeTypeStmts
	;

BoaConfigStmts:		BoaConfigStmts BoaConfigStmt
	|		/* empty */
	;

BoaConfigStmt:		
			PortStmt
	|		UserStmt
	|		GroupStmt
	|		ServerAdminStmt
	|		ServerRootStmt
	|		ErrorLogStmt
	|		AccessLogStmt
	|		CgiLogStmt
	|		VerboseCGILogsStmt
	|		ServerNameStmt
	|		VirtualHostStmt	
	|		DocumentRootStmt
	|		UserDirStmt
	|		DirectoryIndexStmt
	|		DirectoryMakerStmt
	|		KeepAliveMaxStmt
	|		KeepAliveTimeoutStmt
	|		DefaultTypeStmt
	|		MimeTypesStmt
	|		AddTypeStmt
	|		AliasStmt
	|		ScriptAliasStmt
	|		RedirectStmt
	;

PortStmt:		B_PORT INTEGER
		{ server_port = $2; }
	;

UserStmt:		B_USER STRING
		{ passwdbuf = getpwnam($2);
		  if(!passwdbuf) {
		    fprintf(stderr, "No such user: %s\n", $2);
		    exit(1);
		  }
		  server_uid = passwdbuf->pw_uid; 
		}
	|		B_USER INTEGER
		{ server_uid = $2; }
	;

GroupStmt:		B_GROUP STRING
		{ groupbuf = getgrnam($2);
		  if(!groupbuf) {
		    fprintf(stderr, "No such group: %s\n", $2);
		    exit(1);
		  }
		  server_gid = groupbuf->gr_gid;
		}
	|		B_GROUP INTEGER
		{ server_gid = $2; }
	;

ServerAdminStmt:	B_SERVERADMIN STRING
		{ if(server_admin)
			free(server_admin);
		  server_admin = strdup($2); 
		}
	;

ServerRootStmt:		B_SERVERROOT STRING
		{ if(server_root)
			free(server_root);
		  server_root = strdup($2); 
		  if(chdir(server_root) == -1) {
		      fprintf(stderr, "Could not chdir to ServerRoot.\n");
		      exit(1);
		  } 
		}
	;

ErrorLogStmt:		B_ERRORLOG STRING
		{ if(error_log_name)
			free(error_log_name);
		  error_log_name = strdup($2); 
		}
	;

AccessLogStmt:		B_ACCESSLOG STRING
		{ if(access_log_name)
			free(access_log_name);
		  access_log_name = strdup($2); 
		}
	;
	
CgiLogStmt:			B_CGILOG STRING
		{ if(cgi_log_name)
			free(cgi_log_name);
		  cgi_log_name = strdup($2);
		}
	;
	
VerboseCGILogsStmt:	B_VERBOSECGILOGS
		{ 
		  verbose_cgi_logs = 1;
		 }
	;

ServerNameStmt:		B_SERVERNAME STRING
		{ if(server_name)
			free(server_name);
		  server_name = strdup($2); 
		}
	;

VirtualHostStmt:	B_VIRTUALHOST
		{ virtualhost = 1; }
	;

DocumentRootStmt:	B_DOCUMENTROOT STRING
		{ if(document_root)
			free(document_root);
		  document_root = strdup($2); 
		}
	;

UserDirStmt:		B_USERDIR STRING
		{ if(user_dir)
			free(user_dir);
		  user_dir = strdup($2); 
		}
	;

DirectoryIndexStmt:	B_DIRECTORYINDEX STRING
		{ if(directory_index)
			free(directory_index);
		  directory_index = strdup($2); 
		}
	;

DirectoryMakerStmt: B_DIRECTORYMAKER STRING
		{ if (dirmaker)
			free(dirmaker);
		  dirmaker = strdup($2);
		}
	;

KeepAliveMaxStmt:		B_KEEPALIVEMAX INTEGER
		{ ka_max = $2; }
	;

KeepAliveTimeoutStmt:		B_KEEPALIVETIMEOUT INTEGER
		{ ka_timeout = $2; }
	;

MimeTypesStmt:	B_MIMETYPES STRING
		{ if (mime_types)
			free(mime_types);
		  mime_types = strdup($2);
		}
	;

DefaultTypeStmt:	B_DEFAULTTYPE STRING
		{ if(default_type)
			free(default_type);
		  default_type = strdup($2); 
		}
	;

AddTypeStmt:		B_ADDTYPE MimeTypeStmt
	;

AliasStmt:		B_ALIAS STRING
		{ strcpy(fakename, $2); }
			STRING
		{ add_alias(fakename, $4, ALIAS); }

ScriptAliasStmt:	B_SCRIPTALIAS STRING 
		{ strcpy(fakename, $2); }
			STRING
		{ add_alias(fakename, $4, SCRIPTALIAS); }
	;

RedirectStmt:		B_REDIRECT STRING 
		{ strcpy(fakename, $2); }
			STRING
		{ add_alias(fakename, $4, REDIRECT); }
	;


/******************* mime.types **********************/

MimeTypeStmts:		MimeTypeStmts MimeTypeStmt
	|		/* empty */
	;

MimeTypeStmt:		MIMETYPE 
		{ strcpy(mime_type, $1); }
			ExtensionList
	;

ExtensionList:		ExtensionList Extension
	|		/* empty */
	;

Extension:		STRING
		{ add_mime_type($1, mime_type); }
	;

%%

