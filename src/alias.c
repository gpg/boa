/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1996-99 Jon Nelson <jnelson@boa.org>
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

/* $Id: alias.c,v 1.51 2000/10/03 01:45:43 jon Exp $ */

#include "boa.h"

static alias *alias_hashtable[ALIAS_HASHTABLE_SIZE];

inline int get_alias_hash_value(char *file);

/*
 * Name: get_alias_hash_value
 *
 * Description: adds the ASCII values of the file letters
 * and mods by the hashtable size to get the hash value
 * Note: stops at first '/' (or '\0')
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
            if (!strcmp(fakename, old->fakename)) /* don't add twice */
                return;
            old = old->next;
        }
    new = (alias *) malloc(sizeof (alias));
    if (!new) {
        log_error_mesg(__FILE__, __LINE__,
                       "out of memory adding alias to hash");
        exit(errno);
    }

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
 * Name: find_alias
 *
 * Description: Locates uri in the alias hashtable if it exists.
 *
 * Returns:
 *
 * alias structure or NULL if not found
 */

alias *find_alias(char *uri)
{
    alias *current;
    int hash;

    /* Find ScriptAlias, Alias, or Redirect */

    hash = get_alias_hash_value(uri);

    current = alias_hashtable[hash];
    while (current) {
#ifdef FASCIST_LOGGING
        fprintf(stderr,
                "%s:%d - comparing \"%s\" (request) to \"%s\" (alias): ",
                __FILE__, __LINE__, uri, current->fakename);
#endif
        if (!memcmp(uri, current->fakename, current->fake_len)) {
#ifdef FASCIST_LOGGING
            fprintf(stderr, "Got it!\n");
#endif
            return current;
        } else
#ifdef FASCIST_LOGGING
            fprintf(stderr, "Don't Got it!\n");
#else
            ;
#endif
        current = current->next;
    }
    return current;
}

/*
 * Name: translate_uri
 *
 * Description: Parse a request's virtual path.
 * Sets query_string, pathname directly.
 * Also sets path_info, path_translated, and script_name via
 *  init_script_alias
 *
 * Note: NPH in user dir is currently broken
 *
 * Note -- this should be broken up.
 *
 * Return values:
 *   0: failure, close it down
 *   1: success, continue
 */

int translate_uri(request * req)
{
    static char buffer[MAX_HEADER_LENGTH + 1];
    char *req_urip;
    alias *current;
    char c, *p;
    int uri_len;

    /* Percent-decode request */
    if (unescape_uri(req->request_uri) == 0) {
        log_error_doc(req);
        fputs("Problem unescaping uri\n", stderr);
        send_r_bad_request(req);
        return 0;
    }

    /* clean pathname */
    clean_pathname(req->request_uri);

    req_urip = req->request_uri;
    if (req_urip[0] != '/') {
        send_r_not_found(req);
        return 0;
    }


    /* Rationale note:
       We have to pull out the query_string stuff *after*
       we unescape the uri because the uri might be sent as:
       "/cgi%2Dbin/test%3Ffoo=bar"
       and decoded as:
       "/cgi-bin/test?foo=bar"
     */

    /* Move anything after ? into req->query_string */
    /* advance to end of path, looking for end or '?' */
    while ((c = *req_urip) && c != '?')
        req_urip++;

    /* query_string ?? */
    if (c == '?') {
        *req_urip++ = '\0';
        req->query_string = strdup(req_urip);
    }

    /*
       this has to be after the query string search, because
       we might shorten the request_uri
     */

    uri_len = strlen(req->request_uri);

    current = find_alias(req->request_uri);
    if (current) {

        /* FIXME: what does the below code do? */
        /*
           if (current->fakename[current->fake_len - 1] != '/' &&
           req->request_uri[current->fake_len] != '/' &&
           req->request_uri[current->fake_len] != '\0') {
           break;
           }
         */

        if (current->type == SCRIPTALIAS) /* Script */
            return init_script_alias(req, current, uri_len);

        /* not a script alias, therefore begin filling in data */

        {
            int len;
            len = current->real_len;
            len += uri_len - current->fake_len;
            if (len > MAX_HEADER_LENGTH) {
                log_error_doc(req);
                fputs("uri too long!\n", stderr);
                send_r_bad_request(req);
                return 0;
            }
            memcpy(buffer, current->realname, current->real_len);
            memcpy(buffer + current->real_len,
                   req->request_uri + current->fake_len,
                   uri_len - current->fake_len + 1);
        }
        /*
           strcpy(buffer, current->realname);
           strcat(buffer, &req->request_uri[current->fake_len]);
         */

        /*
           sprintf(buffer, "%s%s", current->realname,
           &req->request_uri[current->fake_len]);
         */

        if (current->type == REDIRECT) { /* Redirect */
            if (req->method == M_POST) { /* POST to non-script */
                /* it's not a cgi, but we try to POST??? */
                send_r_bad_request(req);
                return 0;       /* not a script alias, therefore begin filling in data */
            }
            send_redirect_temp(req, buffer, "");
            return 0;
        } else {                /* Alias */
            req->pathname = strdup(buffer);
            return 1;
        }
    }

    /*
       The reason why this is *not* an 'else if' is that,
       after aliasing, we still have to check for '~' expansion
     */

    if (user_dir && req->request_uri[1] == '~') {
        char *user_homedir;

        req_urip = req->request_uri + 2;

        p = strchr(req_urip, '/');
        if (p)
            *p = '\0';

        user_homedir = get_home_dir(req_urip);
        if (p)                  /* have to restore request_uri in case of error */
            *p = '/';

        if (!user_homedir) {    /*no such user */
            send_r_not_found(req);
            return 0;
        }
        {
            int l1 = strlen(user_homedir);
            int l2 = strlen(user_dir);
            int l3 = (p ? strlen(p) : 0);

            if (l1 + l2 + l3 + 1 > MAX_HEADER_LENGTH) {
                log_error_doc(req);
                fputs("uri too long!\n", stderr);
                send_r_bad_request(req);
                return 0;
            }

            memcpy(buffer, user_homedir, l1);
            buffer[l1] = '/';
            memcpy(buffer + l1 + 1, user_dir, l2 + 1);
            if (p)
                memcpy(buffer + l1 + 1 + l2, p, l3 + 1);
        }
        /*
           sprintf(buffer, "%s/%s", user_homedir, user_dir);
           if (p)
           strcat(buffer, p);
         */
    } else if (document_root) {
        /* no aliasing, no userdir... */
        int l1, l2, l3;

        l1 = strlen(document_root);
        l2 = strlen(req->request_uri);
        if (virtualhost)
            l3 = strlen(req->local_ip_addr);
        else
            l3 = 0;

        if (l1 + l2 + l3 + 1 > MAX_HEADER_LENGTH) {
            log_error_doc(req);
            fputs("uri too long!\n", stderr);
            send_r_bad_request(req);
            return 0;
        }

        /* the 'l2 + 1' is there so we copy the '\0' as well */
        memcpy(buffer, document_root, l1);
        if (virtualhost) {
            buffer[l1] = '/';
            memcpy(buffer + l1 + 1, req->local_ip_addr, l3);
            memcpy(buffer + l1 + 1 + l3, req->request_uri, l2 + 1);
        } else
            memcpy(buffer + l1, req->request_uri, l2 + 1);
    } else {
        /* not aliased.  not userdir.  not part of document_root.  BAIL */
        send_r_not_found(req);
        return 0;
    }

    /* if here,
     * o it may be aliased but it's not a redirect or a script...
     * o it may be a homedir
     * o it may be a document_root resource (with or without virtual host)
     */

    req->pathname = strdup(buffer);

    /* below we support cgis outside of a ScriptAlias */
    if (strcmp(CGI_MIME_TYPE, get_mime_type(buffer)) == 0) { /* cgi */
#ifdef FASCIST_LOGGING
        log_error_time();
        fprintf(stderr, "%s:%d - buffer is: \"%s\"\n",
                __FILE__, __LINE__, buffer);
#endif
        /* FIXME */
        /* script_name could end up as /cgi-bin/bob/extra_path */
        req->script_name = strdup(req->request_uri);
        if (req->simple)
            req->is_cgi = NPH;
        else
            req->is_cgi = CGI;
        return 1;
    } else if (req->method == M_POST) { /* POST to non-script */
        /* it's not a cgi, but we try to POST??? */
        send_r_bad_request(req);
        return 0;
    } else                      /* we are done!! */
        return 1;
}

/*
 * Name: init_script_alias
 *
 * Description: Performs full parsing on a ScriptAlias request
 * Sets path_info and script_name
 *
 * Return values:
 *
 * 0: failure, shut down
 * 1: success, continue
 */

int init_script_alias(request * req, alias * current1, int uri_len)
{
    static char pathname[MAX_HEADER_LENGTH + 1];
    struct stat statbuf;
    static char buffer[MAX_HEADER_LENGTH + 1];

    int index = 0;
    char c;

    /* copies the "real" path + the non-alias portion of the
       uri to pathname.
     */

    if (uri_len - current1->fake_len + current1->real_len >
        MAX_HEADER_LENGTH) {
        log_error_doc(req);
        fputs("uri too long!\n", stderr);
        send_r_bad_request(req);
        return 0;
    }
    strcpy(pathname, current1->realname);
    strcat(pathname, &req->request_uri[current1->fake_len]);
    /*
       sprintf(pathname, "%s%s", current1->realname,
       &req->request_uri[current1->fake_len]);
     */
#ifdef FASCIST_LOGGING
    log_error_time();
    fprintf(stderr,
            "%s:%d - pathname in init_script_alias is: \"%s\" (\"%s\")\n",
            __FILE__, __LINE__, pathname, pathname + current1->real_len);
#endif
    if (strncmp("nph-", pathname + current1->real_len, 4) == 0
        || req->simple) req->is_cgi = NPH;
    else
        req->is_cgi = CGI;


    /* start at the beginning of the actual uri...
       (in /cgi-bin/bob, start at the 'b' in bob */
    index = current1->real_len;

    do {
        c = pathname[++index];
    } while (c != '/' && c != '\0');

    /* we have path_info if c == '/'... still have to check for query */
    if (c == '/') {
        int hash;
        alias *current;
        int path_len;

        req->path_info = strdup(pathname + index);
        pathname[index] = '\0'; /* strip path_info from path */
        path_len = strlen(req->path_info);

        /* now, we have to re-alias the extra path info....
           this sucks.
         */
        hash = get_alias_hash_value(req->path_info);
        current = alias_hashtable[hash];
        while (current && !req->path_translated) {
            if (!strncmp(req->path_info, current->fakename,
                         current->fake_len)) {

                if (current->real_len +
                    path_len - current->fake_len > MAX_HEADER_LENGTH) {
                    log_error_doc(req);
                    fputs("uri too long!\n", stderr);
                    send_r_bad_request(req);
                    return 0;
                }

                strcpy(buffer, current->realname);
                strcat(buffer, &req->path_info[current->fake_len]);
                /*
                   sprintf(buffer, "%s%s", current->realname,
                   &req->path_info[current->fake_len]);
                 */
                req->path_translated = strdup(buffer);
            }
            current = current->next;
        }
        /* no alias... try userdir */
        if (!req->path_translated && user_dir && req->path_info[1] == '~') {
            char *user_homedir;
            char *p;

            p = strchr(pathname + index + 1, '/');
            if (p)
                *p = '\0';

            user_homedir = get_home_dir(pathname + index + 2);
            if (p)
                *p = '/';

            if (!user_homedir) { /* no such user */
                send_r_not_found(req);
                return 0;
            }
            {
                int l1 = strlen(user_homedir);
                int l2 = strlen(user_dir);
                int l3;
                if (p)
                    l3 = strlen(p);
                else
                    l3 = 0;

                req->path_translated = malloc(l1 + l2 + l3 + 2);
                if (req->path_translated == NULL) {
                    log_error_mesg(__FILE__, __LINE__,
                                   "unable to malloc memory for req->path_translated");
                    return 0;
                }
                memcpy(req->path_translated, user_homedir, l1);
                req->path_translated[l1] = '/';
                memcpy(req->path_translated + l1 + 1, user_dir, l2 + 1);
                if (p)
                    memcpy(req->path_translated + l1 + 1 + l2, p, l3 + 1);

                /*
                   sprintf(buffer, "%s/%s", user_homedir, user_dir);
                   if (p)
                   strcat(buffer, p);
                   req->path_translated = strdup(buffer);
                 */
            }
        }
        if (!req->path_translated) {
            /* no userdir, no aliasing... try document root */
            int l1, l2;
            l1 = strlen(document_root);
            l2 = path_len;

            req->path_translated = malloc(l1 + l2 + 1);
            if (req->path_translated == NULL) {
                log_error_mesg(__FILE__, __LINE__,
                               "unable to malloc memory for req->path_translated");
                return 0;
            }
            memcpy(req->path_translated, document_root, l1);
            memcpy(req->path_translated + l1, req->path_info, l2 + 1);
            /*
               sprintf(buffer, "%s%s", document_root,
               req->path_info);
               req->path_translated = strdup(buffer);
             */
        }
    }
    if (stat(pathname, &statbuf) == -1) {
        send_r_not_found(req);
        return 0;
    } else if (!S_ISREG(statbuf.st_mode) ||
               access(pathname, R_OK | X_OK) == -1) {
        send_r_forbidden(req);
        return 0;
    }
    req->pathname = strdup(pathname);

    /* there used to be some ip stuff in here */

    /* We have to strip some stuff from the pathname because we
       want an http accessible name, not the "real" name.
       Thus, instead of /usr/lib/cgi-bin/cgiprogram, we get
       /cgi-bin/cgiprogram
       Yay!
     */
    req->script_name = strdup(req->pathname + current1->real_len
                              - current1->fake_len);
    return 1;
}

/*
 * Empties the alias hashtable, deallocating any allocated memory.
 */

void dump_alias(void)
{
    int i;
    alias *temp;

    for (i = 0; i < ALIAS_HASHTABLE_SIZE; ++i) { /* these limits OK? */
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
