
Boa, v0.91beta, by Paul Phillips <psp@well.com>

*** Introduction ***

This is the Boa webserver, version 0.91beta.  Boa is a singletasking
http server.  That means that unlike traditional webservers, it does
not fork for each incoming connection, nor does it fork many copies of
itself to handle multiple connections.  It internally multiplexes all
of the ongoing http connections, and forks only for CGI programs
(which must be separate processes.)

The primary design goals of Boa are speed and security, though it may not 
achieve reasonable speed; we'll see.  Boa will not be a feature-packed 
server now or ever.  The only future improvements to Boa that I anticipate 
are in the areas of speed, security, robustness, and portability, though I
may implement desired features if it can be done without negatively
impacting these goals.

The Boa WWW page is <URL:http://www.cerf.net/~paulp/boa/> -- information
and server distributions will be available here.


*** Installation and Usage ***

 * cd into the src directory and type "make".  
 * edit conf/boa.conf to reflect your site's information (the configuration
 options are documented in boa.conf)

I only expect it to work on Linux at this moment; SunOS should follow 
shortly.  Old Linux kernels may not work because of deficient mmap 
implementations.

You can compile a default SERVER_ROOT into the binary by setting the
#define at the top of defines.h, or you can specify it on the command
line with the -c option (command line takes precedence.) I start boa
out of rc.local with

/usr/local/boa/boa -c /usr/local/boa

The configuration files must be in a subdirectory of the server root
called conf.  This is the default configuration.  All relative paths 
are relative to server_root.  There is no option to run chrooted, but 
this will be added at some point.

I realize this isn't exactly comprehensive documentation; most of the 
general concepts are similar to other webservers.  The documentation at 
<URL:http://hoohoo.ncsa.uiuc.edu> for NCSA httpd should be helpful if 
you are new to http servers.


*** Potential problems ***

There are many issues that become more difficult to resolve in a single 
tasking webserver than in the normal forking model.  Here is a partial 
list -- there are probably many that I haven't encountered yet.

 * Directory indexing and stat() calls over NFS.  NFS mounted filesystems 
 can be very slow when under heavy load, and the directory indexing for a 
 particular request is done all at once while the other processes wait.  If 
 there are a large number of files or the NFS server is slow to respond, 
 other connections will suffer.

 * DNS lookups.  Writing a nonblocking gethostbyaddr is a difficult
 and not very enjoyable task.  I experimented with several methods,
 including a separate logging process, before removing hostname lookups
 entirely.  There is a companion program with boa (util/resolved.pl) that 
 will postprocess the logfiles and replace IP addresses with hostnames, 
 which is much faster no matter what sort of server you run.

 * The REMOTE_HOST environment variable is not set in CGI scripts, for
 the same reason.  This is easily worked around because the IP address
 is provided in the REMOTE_ADDR variable, so gethostbyaddr or a variant
 can be used in the CGI script.

 * Identd lookups.  Same difficulties as hostname lookups; not included.

 * Password file lookups via NIS.  If users are allowed to server HTML
 from their home directories, password file lookups can potentially 
 block the process.  To lessen the impact, each user's home directory
 is cached by boa so it need only be looked up once.

 * Since a file descriptor is needed for every ongoing connection,
 it is possible though highly improbable to run out of file
 descriptors. 


*** Differences between boa and other webservers ***

In the pursuit of speed and simplicity, some aspects of Boa differ
from the popular webservers.  In no particular order:

 * CGI programs output directly to the client rather than to
 the server.  In most webservers, data from the CGI goes
 through the server before reaching the client.  This allows the
 server to log the size of the transfer, and to buffer the data
 for more efficient use of the network.  Since Boa does not do
 this, it cannot log the size of CGI programs, nor can it handle
 error conditions in CGIs.  Once the CGI has begun, Boa is no 
 longer aware of it.  The advantage is that this is one less
 connection Boa has to participate in.

 * On other servers, "nph-" CGIs speak directly to the client.
 In Boa, the only difference between a regular CGI and an nph-
 CGI is that the server does not output any header lines in
 an nph- script.

 * There are no server side includes in Boa.  I don't like them,
 and it's too slow to parse.


*** Known bugs ***

 * URL escaping/unescaping is probably broken on weird cases; I'm
 not entirely clear on what should be escaped and when.  This should
 be fixed shortly.

*** Possible improvements ***

The following areas are under consideration for a future relase of Boa:

 * command line argv parsing to get CGI compliance (certain)
 * built in imagemap support (very likely)
 * access control (likely)
 * ports to other platforms (depends on what I can gain access to.  If
 you can provide me with a development account on a platform other than
 Linux or SunOS 4.1.4, please send email.)
 * "VirtualHost" capability (possible)
 * various flexibility improvements in configuration file
 * [your idea here]


Suggestions, comments, and bug reports welcomed at <psp@well.com>,
or post to comp.infosystems.www.servers.unix if it is of general interest.

*** License ***

This program is distributed under the GNU general public license,
as noted in each source file:

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


*** Acknowledgements ***

Thanks to everyone in the WWW community, in general a great bunch of
people.  Special thanks to Clem Taylor <ctaylor@eecis.udel.edu>, who 
provided invaluable feedback on many of my ideas, and offered good ones
of his own.  Also thanks to John Franks, author of wn, for writing what
I believe is the best webserver out there.


