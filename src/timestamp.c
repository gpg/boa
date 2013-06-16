#include "boa.h"

void timestamp(void)
{
	log_error_time();
	fprintf(stderr, "boa: server version %s\n", SERVER_VERSION);
	log_error_time();
	fprintf(stderr, "boa: server built " __DATE__ " at " __TIME__ \
			".\n");
	log_error_time();
	fprintf(stderr, "boa: starting server pid=%d, port %d\n",
			getpid(), server_port);
}
