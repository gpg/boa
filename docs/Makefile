# $Id: Makefile.in,v 1.1.2.3 2002/09/19 03:03:54 jnelson Exp $

srcdir = .


all:   boa.html boa.dvi boa.info boa.txt

boa.info:	boa.texi
	makeinfo --number-sections $^

boa.dvi:	boa.texi
	texi2dvi --clean $^

boa.pdf:	boa.texi
	texi2dvi --pdf --clean $^

boa.ps:	boa.dvi
	dvips -o $@ $^

boa_toc.html:	boa.texi
	texi2html -split_chapter -menu $^

boa.html:	boa.texi
	makeinfo --html --number-sections --no-split -o - $^ | \
        sed -e 's/Node:.*//' | sed -e 's/Next:.*//' | \
        sed -e 's/Previous:.*//' | sed -e 's/Up:.*//' > $@

boa.txt:	boa.texi
	makeinfo --no-headers --no-split -o $@ $^


clean:
	rm -f boa.{html,txt,dvi,ps,pdf,info} boa_*.html
	rm -f boa.{cp,fn,fns,ky,log,pg,toc,tp,vr,vrs,aux} *~

distclean:	mrclean

mrclean:	clean
	rm -f Makefile
