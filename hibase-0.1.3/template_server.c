/* Template for a transaction server. */

#include "includes.h"
#include "net.h"
#include "template_config.h"
#include <signal.h>

/* Maximum number of connections we can use, not including the listening
   connection. */
#define MAX_CONNECTIONS  128

/* Connection slots, these are filled with handles returned from the
   networking code. */
static net_handle_t connection[MAX_CONNECTIONS];
static net_handle_t listening_handle;

/* State of each connection. */
typedef enum {
  NONE,
  READING,
  READ_LAST_ROUND,
} state_t;

static state_t state[MAX_CONNECTIONS];

/* A value to denote a "thread" that is not indexed from the connections
   table. */
#define NO_THREAD  ((word_t) ~0)

static void server_loop (unsigned short);
static void try_accept (void);
static void try_read (unsigned long index);
static void int_handler (int sig);


int main (int argc, char *argv[])
{
  unsigned short port;

  if (argc < 2) {
    fprintf(stderr, "Missing port number\n");
    exit(1);
  }
  port = atoi(argv[1]);
  argv[1] = NULL;

  init_shades(argc, argv);
  net_init();
  signal(SIGINT, int_handler);

  server_loop(port);
  return 0;
}

void int_handler (int sig)
{
  exit(3);
}

/* Server loop establishes a listening connection and then proceeds in
   a loop that requests events from the network and handles them. */
void server_loop (unsigned short port)
{
  net_return_t ret;
  unsigned long i, wakeups;
  struct timeval timeout;

  for (i = 0; i < MAX_CONNECTIONS; i++) {
    state[i] = NONE;
  }

  ret = net_listen(port);
  if (NET_IS_FATAL_ERROR(ret.error)) {
    fprintf(stderr, "Fatal error in net_listen: %s\n", strerror(ret.error));
    exit(1);
  }
  listening_handle = ret.handle;
  try_accept();

  while (1) {
    /* Try an event on each connection that has no pending event. */
    for (i = 0; i < MAX_CONNECTIONS; i++) {
      if (state[i] == READ_LAST_ROUND) {
	try_read(i);
      }
    }

    /* Wait one second for possible events. */
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    wakeups = net_number_of_wakeups(&timeout);

    if (wakeups == 0) {
      /* No events for one second, the network is probably quite idle. */
      /* XXX Do some idle time activities. */
    } else while (wakeups > 0) {
      ret = net_get_wakeup();

      if (ret.handle == listening_handle) {
	try_accept();
      } else {
	try_read(ret.thread_id);
      }

      wakeups--;
    }
  }
}

void try_accept (void)
{
  net_return_t ret;
  unsigned long i;

  while (ret = net_accept(NO_THREAD, listening_handle),
	 ret.error != NET_BLOCKED) {
    if (NET_IS_FATAL_ERROR(ret.error)) {
      fprintf(stderr, "Fatal error trying to accept: %s",
	      strerror(ret.error));
      exit(1);
    }
    
    for (i = 0; i < MAX_CONNECTIONS; i++) {
      if (state[i] == NONE) {
	connection[i] = ret.handle;
	try_read(i);
	break;
      }
    }
  }
}

void try_read (unsigned long index)
{
  net_return_t ret;
  unsigned char packet[PACKET_SIZE];
  unsigned long i;

  ret = net_read_mem(index, connection[index], (ptr_t) packet, PACKET_SIZE);
  if (NET_IS_FATAL_ERROR(ret.error)) {
    fprintf(stderr, "Fatal error reading connection %d: %s\n",
	    index, strerror(ret.error));
    state[index] = NONE;
  } else if (ret.error == 0) {

    /* Check packet for integrity. */
    /* XXX The handling of the packet (transaction logic) would be here. */
    for (i = 0; i < PACKET_SIZE; i++) {
      packet[i] == 256 - packet[i];
    }

    ret = net_write_mem(index, connection[index], (ptr_t) packet, PACKET_SIZE);
    if (NET_IS_FATAL_ERROR(ret.error)) {
      fprintf(stderr, "Fatal error writing connection %d: %s\n",
	      index, strerror(ret.error));
      state[index] = NONE;
    } else if (ret.error == NET_BLOCKED) {
      fprintf(stderr, "Writing to %d blocked\n", index);
      exit(1);
    } else {
      /* Mark this connection as read so reading will be reattempted
	 in the IO loop. */
      state[index] = READ_LAST_ROUND;
    }
  } else if (ret.error == NET_BLOCKED) {
    state[index] = READING;
  } else {
    fprintf(stderr, "Other error %d while reading %d\n", ret.error, index);
  }
}
