#!/usr/bin/perl

# Remember that CGI programs have to close out the HTTP header
# (with a pair of newlines), after giving the Content-type:
# and any other relevant or available header information.

$SCRIPT_NAME=$ENV{"SCRIPT_NAME"};

if ( $SCRIPT_NAME =~ /html.gz$/ ) {
	print "Content-type: ";
	print $ENV{"CONTENT_TYPE"};
} else {
	print "Content-type: text/plain";
}

print "\n\n";
print `gunzip -c $SCRIPT_NAME`;
exit 0;
