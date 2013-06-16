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

/* $Id: compat.h,v 1.11 2001/10/20 02:52:42 jnelson Exp $*/

#ifndef _COMPAT_H
#define _COMPAT_H

#include "config.h"


#ifndef HAVE_STRSTR
char *strstr(char *s1, char *s2);
#endif
#ifndef HAVE_STRDUP
char *strdup(char *s);
#endif

#ifdef TIME_WITH_SYS_TIME
/* maybe-defined in config.h */
#include <sys/time.h>
#endif

#ifndef OPEN_MAX
#define OPEN_MAX 256
#endif

#ifndef NI_MAXHOST
#define NI_MAXHOST 20
#endif

#ifndef SO_MAXCONN
#define SO_MAXCONN 250
#endif

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif


#ifdef SUNOS
#define NOBLOCK O_NDELAY
#else
#define NOBLOCK O_NONBLOCK
#endif

#ifndef MAP_FILE
#define MAP_OPTIONS MAP_PRIVATE /* Sun */
#else
#define MAP_OPTIONS MAP_FILE|MAP_PRIVATE /* Linux */
#endif

#ifdef INET6
#define SOCKADDR sockaddr_storage
#define S_FAMILY __s_family
#define SERVER_AF AF_INET6
#else
#define SOCKADDR sockaddr_in
#define S_FAMILY sin_family
#define SERVER_AF AF_INET
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#endif
