/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1996,97 Jon Nelson <nels0988@tc.umn.edu>
 *  Some changes Copyright (C) 1996 Russ Nelson <nelson@crynwr.com>
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

/* boa: alias.c */

#include "boa.h"

static alias *alias_hashtable[ALIAS_HASHTABLE_SIZE];

inline int get_alias_hash_value(char *file);

/*
 * Name: get_alias_hash_value
 * 
 * Description: adds the ASCII values of the file letters
 * and mods by the hashtable size to get the hash value
 */

inline int get_alias_hash_value(char *file)
{
	unsigned int hash = 0;
	unsigned int index = 0;
	unsigned char c;

	hash = file[index++];
	while ((c = file[index++]) && c != '/')
		hash += (unsigned int) c;

	return hash % ALIAS_HASHTABLE_SIZE;
}

/*
 * Name: add_alias
 *
 * Description: add an Alias, Redirect, or ScriptAlias to the 
 * alias hash table.
 */

void add_alias(char *fakename, char *realname, int type)
{
	int hash;
	alias *old, *new;

	hash = get_alias_hash_value(fakename);

	old = alias_hashtable[hash];

	if (old)
		while (old->next) {
			if (!strcmp(fakename, old->fakename))	/* dont' add twice */
				return;
			old = old->next;
		}
	new = (alias *) malloc(sizeof(alias));
	if (!new)
		die(OUT_OF_MEMORY);

	if (old)
		old->next = new;
	else
		alias_hashtable[hash] = new;

	new->fakename = strdup(fakename);
	new->fake_len = strlen(fakename);
	new->realname = strdup(realname);
	new->real_len = strlen(realname);

	new->type = type;
	new->next = NULL;
}

/*
 * Name: translate_uri
 * 
 * Description: Parse a request's virtual path.  Sets path_info,
 * query_string, pathname, and script_name data if it's a 
 * ScriptAlias or a CGI.  Note -- this should be broken up.
 * 
 * Return values: 
 *   0: failure, close it down
 *   1: success, continue 
 */

int translate_uri(request * req)
{
	char buffer[MAX_HEADER_LENGTH + 1];
	char *req_urip;
	alias *current;
	int is_nph = 0;
	char c, *p;

	/* clean pathname */
	clean_pathname(req->request_uri);

	/* Move anything after ? into req->query_string */

	req_urip = req->request_uri;
	if (req_urip[0] != '/') {
		send_r_not_found(req);
		return 0;
	}
	while ((c = *req_urip) && c != '?') {
		req_urip++;
		if (c == '/') {
			if (strncmp("nph-", req_urip, 4) == 0)
				is_nph = 1;
			else
				is_nph = 0;
		}
	}

	if (c == '?') {
		*req_urip++ = '\0';
		req->query_string = strdup(req_urip);
	}
	/* Percent-decode request */

	if (unescape_uri(req->request_uri) == 0) {
		log_error_doc(req);
		fputs("Problem unescaping uri\n", stderr);
		send_r_bad_request(req);
		return 0;
	}
	/* Find UserDir */

	if (user_dir && req->request_uri[1] == '~') {
		char *user_homedir;

		req_urip = req->request_uri + 2;

		p = strchr(req_urip, '/');
		if (p)
			*p = '\0';

		user_homedir = get_home_dir(req_urip);
		if (p)
			*p = '/';			/* have to restore request_uri in case of
								   error */

		if (!user_homedir) {	/*no such user */
			send_r_not_found(req);
			return 0;
		}
		sprintf(buffer, "%s/%s", user_homedir, user_dir);
		if (p)
			strcat(buffer, p);
	} else {
		/* Find ScriptAlias, Alias, or Redirect */
		int hash;

		hash = get_alias_hash_value(req->request_uri);

		current = alias_hashtable[hash];
		while (current) {

			if (!memcmp(req->request_uri, current->fakename,
						current->fake_len)) {
				if (current->fakename[current->fake_len - 1] != '/' &&
					req->request_uri[current->fake_len] != '/' &&
					req->request_uri[current->fake_len] != '\0') {
					break;
				}
				if (current->type == SCRIPTALIAS) {		/* Script */
					if (is_nph)
						req->is_cgi = NPH;
					else
						req->is_cgi = CGI;
					return init_script_alias(req, current);
				}
				sprintf(buffer, "%s%s", current->realname,
						&req->request_uri[current->fake_len]);

				if (current->type == REDIRECT) {	/* Redirect */
					send_redirect_temp(req, buffer);
					return 0;
				} else {		/* Alias */
					req->pathname = strdup(buffer);
					return 1;
				}
			}
			current = current->next;
		}
		if (req->local_ip_addr)
			sprintf(buffer, "%s/%s%s", document_root,
					req->local_ip_addr,
					req->request_uri);
		else
			sprintf(buffer, "%s%s", document_root, req->request_uri);
	}

	req->pathname = strdup(buffer);

	if (strcmp(CGI_MIME_TYPE, get_mime_type(buffer)) == 0) {	/* cgi */
		if (is_nph)
			req->is_cgi = NPH;
		else
			req->is_cgi = CGI;
		req->script_name = strdup(req->request_uri);
		return 1;
	} else if (req->method == M_POST) {		/* POST to non-script */
		send_r_not_implemented(req);
		return 0;
	} else
		return 1;
}
/*
 * Name: init_script_alias
 * 
 * Description: Performs full parsing on a ScriptAlias request
 * 
 * Return values:
 *
 * 0: failure, shut down
 * 1: success, continue          
 */

int init_script_alias(request * req, alias * current)
{
	char pathname[MAX_HEADER_LENGTH + 1];
	char path_info[MAX_HEADER_LENGTH + 1];
	char script_name[MAX_HEADER_LENGTH + 1];
	char buffer[MAX_HEADER_LENGTH + 1];
	struct stat statbuf;

	int index = 0, index_trailer = 0;
	char c;

	sprintf(pathname, "%s%s", current->realname,
			&req->request_uri[current->fake_len]);
	strcpy(script_name, req->request_uri);

	index = current->real_len;
	index_trailer = current->fake_len;

	for (c = pathname[index]; c != '\0' && c != '/'; c = pathname[++index])
		index_trailer++;


	pathname[index++] = '\0';

	if (stat(pathname, &statbuf) == -1) {
		send_r_not_found(req);
		return 0;
	} else if (!S_ISREG(statbuf.st_mode) || access(pathname, R_OK | X_OK) == -1) {
		send_r_forbidden(req);
		return 0;
	}
	req->pathname = strdup(pathname);

	script_name[index_trailer] = '\0';
	req->script_name = strdup(script_name);

	index_trailer = 0;

	if (c == '/') {				/* we have path_info */
		int hash;

		while (c != '\0' && c != '?') {
			path_info[index_trailer++] = c;
			c = pathname[index++];
		}
		path_info[index_trailer] = '\0';
		req->path_info = strdup(path_info);

		hash = get_alias_hash_value(req->path_info);

		current = alias_hashtable[hash];
		while (current) {
			if (!strncmp(req->path_info, current->fakename,
						 current->fake_len)) {

				sprintf(buffer, "%s%s", current->realname,
						&req->path_info[current->fake_len]);

				req->path_translated = strdup(buffer);
				return 1;
			}
			current = current->next;
		}
	}
	if (req->local_ip_addr)
		sprintf(buffer, "%s/%s%s", document_root,
				req->local_ip_addr, req->path_info);
	else
		sprintf(buffer, "%s%s", document_root, req->path_info);
	req->path_translated = strdup(buffer);
	return 1;
}

void dump_alias(void)
{
	int i;
	alias *temp;

	for (i = 0; i < ALIAS_HASHTABLE_SIZE; ++i) {	/* these limits OK? */
		if (alias_hashtable[i]) {
			temp = alias_hashtable[i];
			while (temp) {
				alias *temp_next;

				if (temp->fakename)
					free(temp->fakename);
				if (temp->realname)
					free(temp->realname);
				temp_next = temp->next;
				free(temp);
				temp = temp_next;
			}
			alias_hashtable[i] = NULL;
		}
	}
}
