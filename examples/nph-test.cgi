#!/usr/bin/perl

# Remember that CGI programs have to close out the HTTP header
# (with a pair of newlines), after giving the Content-type:
# and any other relevant or available header information.

# Strictly speaking, this header (and the double-newline) should
# not be printed if the incoming request was in HTTP/0.9.
# Also, we should stop after the header if REQUEST_METHOD == "HEAD".
# But that's too much refinement for this very crude example.

print "HTTP/1.0 200 OK\r\n";
print "Date: ";
print `date`;
print "Connection: close\r\n";
print "Content-type: text/html\r\n\r\n";

if ($ENV{"REQUEST_METHOD"} eq "HEAD") {
 exit 0;
}
print "<html><head><title>Boa CGI test</title></head><body>\n";
print "<H2>Boa CGI test</H2>\n\n";

print "Date: ";
print `date`;

print "<P>\n\n<UL>\n";

foreach (keys %ENV) {
	print "<LI>$_ == $ENV{$_}\n";
}

print "</UL>\n";

print "id: ";
print `id`;
print "\n<p>\n";

if ($ENV{"QUERY_STRING"}=~/ident/ && $ENV{"REMOTE_PORT"} ne "") {

# Uses idlookup-1.2 from Peter Eriksson  <pen@lysator.liu.se>
# ftp://coast.cs.purdue.edu/pub/tools/unix/ident/tools/idlookup-1.2.tar.gz
# Could use modification to timeout and trap stderr messages
	$a="idlookup ".
	   $ENV{"REMOTE_ADDR"}." ".$ENV{"REMOTE_PORT"}." ".$ENV{"SERVER_PORT"};
	$b=qx/$a/;
	print "ident output:<br><pre>\n$b</pre>\n";
}

print "\n<EM>Boa http server</EM>\n";
print "</body></html>\n";

exit 0;


