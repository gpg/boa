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

/* boa: get.c */

#include "boa.h"

/*
 * Name: init_get
 * Description: Initializes a non-script GET or HEAD request. 
 * 
 * Return values:
 *   0: finished or error, request will be freed
 *   1: successfully initialized, added to ready queue
 */

int init_get(request * req)
{
    int data_fd;
    struct stat statbuf;

    clean_pathname(req->path_translated);	/* remove double slashes etc. */
    data_fd = open(req->path_translated, O_RDONLY); 

    if(data_fd == -1) {				/* cannot open */
        log_error_doc(req);
	perror("document open");
	if(errno == ENOENT)
	    send_r_not_found(req);
	else if(errno == EACCES)
	    send_r_forbidden(req);
	else
	    send_r_bad_request(req);
	return 0;
    }

    fstat(data_fd, &statbuf);

    if(S_ISDIR(statbuf.st_mode)) {		/* directory */

	if(req->path_translated[strlen(req->path_translated) - 1] != '/') {
	    char buffer[3*MAX_PATH_LENGTH + 128];

	    strcat(req->path_translated, "/");
	    sprintf(buffer, "http://%s:%d%s/", server_name, server_port, 
	      escape_uri(req->request_uri));
	    req->ret_content_type = strdup("text/html");
	    send_redirect(req, buffer);
	    return 0;
	}

	close(data_fd);				/* close dir */
	data_fd = get_dir_file(req,&statbuf);   /* updates statbuf */

	if(data_fd == -1) {           /* couldn't do it */
		return 0;             /* errors reported by get_dir_file */
	}
    }

    if(req->if_modified_since && 
      !modified_since(&(statbuf.st_mtime), req->if_modified_since)) {
	send_r_not_modified(req);
	close(data_fd);
	return 0;
    }

    req->filesize = statbuf.st_size;
    req->last_modified = statbuf.st_mtime;

    if(req->method == M_HEAD) {
	send_r_request_ok(req);
	close(data_fd);
	return 0;
    }

    /* MAP_OPTIONS: see compat.h */
    req->data_mem = mmap(0, statbuf.st_size, PROT_READ, MAP_OPTIONS,
      data_fd, 0);
    close(data_fd);				/* close data file */

    if((int)req->data_mem == -1) {
	boa_perror(req,"mmap");
	exit(1);   /* or should this be "return 0;" ? */
    }

    send_r_request_ok(req);			/* All's well */

    /* We lose statbuf here, so make sure response has been sent */
    return 1;
}

/*
 * Name: process_get
 * Description: Writes a chunk of data to the socket.
 * 
 * Return values:
 *  -1: request blocked, move to blocked queue
 *   0: EOF or error, close it down
 *   1: successful write, recycle in ready queue
 */

int process_get(request * req)
{
    int bytes_written, bytes_to_write;

    bytes_to_write = req->filesize - req->filepos;
    if(bytes_to_write > SOCKETBUF_SIZE)
	bytes_to_write = SOCKETBUF_SIZE;

    bytes_written = write(req->fd, req->data_mem + req->filepos, 
      bytes_to_write);

    if(bytes_written == -1) {
	if(errno == EAGAIN || errno == EWOULDBLOCK) {  /* no room in pipe */
	    return -1;
	} else {		/* something wrong, includes EPIPE possibility */
#if 1
	    log_error_doc(req);  /* Can generate lots of log entries, */
	    perror("write");     /* OK to disable if your logs get too big */
#endif
	    SQUASH_KA(req);      /* force close fd */
	    return 0;
	}
    }

    req->filepos += bytes_written;
    req->time_last = time(NULL);

    if(req->filepos == req->filesize)		/* EOF */
	return 0;
    else
    	return 1;				/* more to do */
}

/*
 * Name: get_dir_file
 * Description: Called from process_get if the request is a directory.
 * statbuf must describe directory on input, since we may need its
 *   device, inode, and mtime.
 * statbuf is updated, since we may need to check mtimes of a cache.
 * returns file descriptor, or -1 on error.
 * Sequence of places to look:
 *  1. User generated index.html
 *  2. Previously generated cachedir file
 *  3. cachedir file we generate now
 * Cases 2 and 3 only work if cachedir is enabled, otherwise
 * issue a 403 Forbidden.
 */

int get_dir_file( request * req, struct stat *statbuf) {

    char pathname_with_index[MAX_PATH_LENGTH];
    int data_fd;
    time_t real_dir_mtime;

    sprintf(pathname_with_index, "%s%s", req->path_translated, 
            directory_index);

    data_fd = open(pathname_with_index, O_RDONLY);

    if(data_fd != -1) {                        /* user's index file */
	strcpy(req->request_uri, directory_index);      /* for mimetype */
	fstat(data_fd, statbuf);
	return data_fd;
    } else if (cachedir == NULL) {
        send_r_forbidden(req);
        return -1;
    }


    real_dir_mtime = statbuf->st_mtime;
    sprintf(pathname_with_index, "%s/dir.%d.%ld",
                       cachedir, statbuf->st_dev, statbuf->st_ino);
    data_fd = open(pathname_with_index, O_RDONLY);

    if (data_fd != -1) {                 /* index cache */

	fstat(data_fd, statbuf);
	if (statbuf->st_mtime > real_dir_mtime) {
		statbuf->st_mtime = real_dir_mtime;         /* lie */
		strcpy(req->request_uri, directory_index);  /* for mimetype */ 
		return data_fd;
	}
	close(data_fd);
	unlink(pathname_with_index);      /* cache is stale, delete it */
    }
    if (index_directory(req, pathname_with_index) == -1) return -1;

    data_fd = open(pathname_with_index, O_RDONLY);      /* Last chance */
    if (data_fd != -1) {
        strcpy(req->request_uri, directory_index);      /* for mimetype */
	fstat(data_fd, statbuf);
	statbuf->st_mtime = real_dir_mtime;         /* lie */
	return data_fd;
    }

    boa_perror(req,"re-opening dircache");
    return -1;    /* Nothing worked. */

}

/*
 * Name: index_directory
 * Description: Called from get_dir_mapping if a directory html
 * has to be generated on the fly
 * If no_slash is true, prepend slashes to hrefs
 * returns -1 for problem, else 0
 */

int index_directory(request * req, char * dest_filename)
{
    DIR * request_dir;
    FILE * fdstream;
    struct stat statbuf;
    struct dirent * dirbuf;
    int bytes = 0;

    if(chdir(req->path_translated) == -1) {
	if(errno == EACCES || errno == EPERM) {
	    send_r_forbidden(req);
	} else {
	    log_error_doc(req);
	    perror("chdir");
	    send_r_bad_request(req);
	}
	return -1;
    }

    request_dir = opendir(".");
    if (request_dir == NULL) {
	int errno_save=errno;
	send_r_error(req);
        log_error_time();
        fprintf(stderr, "directory \"%s\": ", req->path_translated);
	errno=errno_save;
        perror("opendir");
        return -1;
    }

    fdstream = fopen(dest_filename, "w");
    if (fdstream == NULL) {
	boa_perror(req,"dircache fopen");
        return -1;
    }
    
    bytes += fprintf(fdstream, 
      "<HTML><HEAD>\n<TITLE>Index of %s</TITLE>\n</HEAD>\n\n",
      req->request_uri);
    bytes += fprintf(fdstream, "<BODY>\n\n<H2>Index of %s</H2>\n\n<PRE>\n",
      req->request_uri);

    while((dirbuf = readdir(request_dir))) {
	if(!strcmp(dirbuf->d_name, "."))
	    continue;

	if(!strcmp(dirbuf->d_name, "..")) {
	    bytes += fprintf(fdstream, 
	      " [DIR] <A HREF=\"../\">Parent Directory</A>\n");
	    continue;
	}

        if(stat(dirbuf->d_name, &statbuf) == -1)
	    continue;

	if(S_ISDIR(statbuf.st_mode)) {
	    bytes += fprintf(fdstream, " [DIR] <A HREF=\"%s/\">%s</A>\n",
	      escape_uri(dirbuf->d_name), dirbuf->d_name);
	}
	else {
	    bytes += fprintf(fdstream, 
	      "[FILE] <A HREF=\"%s\">%s</A> (%ld bytes)\n", 
	      escape_uri(dirbuf->d_name), dirbuf->d_name, 
	      (long)statbuf.st_size);
	}
    }

    closedir(request_dir);
    bytes += fprintf(fdstream, "</PRE>\n\n</BODY>\n</HTML>\n");
    fclose(fdstream);

    chdir(server_root);

    req->filesize = bytes;		/* for logging transfer size */
    return 0;                           /* success */
}
