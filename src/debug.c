/* boa: debug.c */

#define DEBUG printf
#include "boa.h"

void print_set_fds( void )
{
    int i;

    DEBUG("SET FDS:\n");
    for(i = 0; i < OPEN_MAX; i++)
      {
	if(FD_ISSET(i, &block_write_fdset))
	    DEBUG("  %d: writing\n", i);
	if(FD_ISSET(i, &block_read_fdset))
	    DEBUG("  %d: reading\n", i);
      }
}

void print_current_fds( void )
{
    request * current;

    DEBUG("Ready requests: ");
    current = request_ready;
    while(current) 
      {
	DEBUG("%d ", current->fd);
	current = current->next;
      }
    DEBUG("\n");

    DEBUG("Blocked requests: ");
    current = request_block;
    while(current) 
      {
	DEBUG("%d ", current->fd);
	current = current->next;
      }
    DEBUG("\n");

}

