GCC_FLAGS = -Wstrict-prototypes -Wpointer-arith -Wcast-align -Wcast-qual\
  -Wtraditional\
  -Wshadow\
  -Wconversion\
  -Waggregate-return\
  -Wmissing-prototypes\
  -Wnested-externs\
  -Wall

# Add -DVERBOSE_CGI_LOGS if you want logs of CGI program start and stop times

CMDARGS = -c -O2

# Change these if necessary

YACC = bison -y
LEX = flex
CC = gcc

all:	boa

# parser dependencies

y.tab.c y.tab.h:	boa_grammar.y
	${YACC} -d boa_grammar.y

y.tab.o:	y.tab.c globals.h
	${CC} ${CMDARGS} y.tab.c

lex.yy.c:	boa_lexer.l
	${LEX} boa_lexer.l
	
lex.yy.o:	lex.yy.c y.tab.h
	${CC} ${CMDARGS} lex.yy.c

# boa dependencies

boa:	lex.yy.o y.tab.o alias.o boa.o cgi.o config.o get.o hash.o log.o \
queue.o read.o request.o response.o signals.o util.o
	${CC} -o boa lex.yy.o y.tab.o alias.o boa.o cgi.o config.o get.o \
hash.o log.o queue.o read.o request.o response.o signals.o util.o

# boa at large

alias.o:	alias.c boa.h globals.h defines.h
	${CC} ${CMDARGS} alias.c

boa.o:	boa.c boa.h globals.h defines.h
	${CC} ${CMDARGS} boa.c

cgi.o:	cgi.c boa.h globals.h defines.h
	${CC} ${CMDARGS} cgi.c

config.o:	config.c boa.h globals.h defines.h
	${CC} ${CMDARGS} config.c

get.o:	get.c boa.h globals.h defines.h
	${CC} ${CMDARGS} get.c

hash.o:	hash.c boa.h globals.h defines.h
	${CC} ${CMDARGS} hash.c

log.o:	log.c boa.h globals.h defines.h
	${CC} ${CMDARGS} log.c

queue.o:	queue.c boa.h globals.h defines.h
	${CC} ${CMDARGS} queue.c

read.o:	read.c boa.h globals.h defines.h
	${CC} ${CMDARGS} read.c

request.o:	request.c boa.h globals.h defines.h
	${CC} ${CMDARGS} request.c

response.o:	response.c boa.h globals.h defines.h
	${CC} ${CMDARGS} response.c

signals.o:	signals.c boa.h globals.h defines.h
	${CC} ${CMDARGS} signals.c

util.o:	util.c boa.h globals.h defines.h
	${CC} ${CMDARGS} util.c

clean:
	rm -f *.o boa core lex.yy.c y.tab.c y.tab.h

#debug.o:	debug.c boa.h globals.h defines.h
#	${CC} ${CMDARGS} debug.c

