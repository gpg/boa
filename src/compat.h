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

/* $Id: compat.h,v 1.9 2000/04/10 19:45:57 jon Exp $*/

#ifndef _COMPAT_H
#define _COMPAT_H

#include "config.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>             /* OPEN_MAX */
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

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

#include <sys/mman.h>
#include <netdb.h>

#ifndef OPEN_MAX
#define OPEN_MAX 256
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

#endif
