/* Template for a transaction client. */

#include "includes.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include "template_config.h"

static void int_handler (int sig);
static void end_client (void);

static struct timeval start_time;
static unsigned long bytes_transferred;


int main (int argc, char *argv[])
{
  int s;
  unsigned long i, j;
  struct hostent *host;
  struct sockaddr_in addr;
  unsigned char write_packet[PACKET_SIZE];
  unsigned char read_packet[PACKET_SIZE];

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
    exit(1);
  }

  host = gethostbyname(argv[1]);
  if (host == NULL) {
    fprintf(stderr, "Cannot find host \"%s\": %s\n", argv[1], strerror(errno));
    exit(1);
  }

  s = socket(PF_INET, SOCK_STREAM, 0);
  if (s == -1) {
    perror("socket");
    exit(1);
  }

  bzero(&addr, sizeof(addr));
  addr.sin_family = PF_INET;
  memcpy(&addr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
  addr.sin_port = htons(atoi(argv[2]));

  if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    perror("connect");
    exit(1);
  }

  for (i = 0; i < PACKET_SIZE; i++) {
    write_packet[i] = i % 256;
  }

  if (gettimeofday(&start_time, NULL) == -1) {
    perror("gettimeofday");
    exit(1);
  }

  signal(SIGINT, int_handler);

  bytes_transferred = 0;

  while (1) {
    /* XXX Create a meaningful packet here. */
    if (write(s, write_packet, PACKET_SIZE) != PACKET_SIZE) {
      perror("write");
      exit(1);
    }
    bytes_transferred += PACKET_SIZE;
    
    i = 0;
    while (i += j = read(s, &read_packet[i], PACKET_SIZE - i), i < PACKET_SIZE) {
      bytes_transferred += j;
      if (j == 0) {
	fprintf(stderr, "EOF reading %d\n", s);
	end_client();
      } else if (j == -1) {
	perror("read");
	exit(1);
      }
    }

    /* XXX Check the returned data here. */
    /*
    for (i = 0; i < PACKET_SIZE; i++) {
     assert(read_packet[i] == i % 256);
    }
    */
  }
  return 0;
}

void int_handler (int sig)
{
  fprintf(stderr, "Interrupt received, quitting.\n");
  end_client();
}

void end_client (void)
{
  unsigned long sec;
  struct timeval end_time;

  if (gettimeofday(&end_time, NULL) == -1) {
    perror("gettimeofday");
    exit(1);
  }
  sec = end_time.tv_sec - start_time.tv_sec;

  fprintf(stderr, "Time taken: %d seconds\n", sec);
  fprintf(stderr, "Bytes transferred: %d\n", bytes_transferred);
  if (sec > 0)
    fprintf(stderr, "%d bytes per second\n", bytes_transferred / sec);
  exit(0);
}
