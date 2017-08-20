/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Antti-Pekka Liedes <apl@iki.fi>
 */

/* Network interface.  This implementation uses BSD sockets (found on most
 * UNIX-like operating systems) for connections and select() for polling
 * which connections might be resumed.  All information about connections
 * is stored internally and the file descriptor (fd) is given outside
 * to distinguish between connections.  To the extent that it is possible,
 * all calls try to guarantee real-time response, ie. they don't block
 * for a "long" time.
 *
 * Whenever a net_ call that may block does so, the blocking status
 * is recorded and the file descriptor is added to the right file descriptor
 * set so that it can be later select()ed to test its resumability.
 * The sockets are polled using select() when `get_number_of_wakeups' is
 * called.  Some types of pending events are completed immediately,
 * the rest are left for the original caller to complete.  `net_get_wakeup'
 * can be used to iterate all the possible wakeups, for example:
   unsigned long i;
   net_return_t net_return;
   for (i = net_number_of_wakeups(); i > 0; i--) {
     net_return = net_get_wakeup();
      * net_return now includes information about the wakeup,
      * do something about it (try the same call that blocked again).
      *
   }
 */

static char *rev_id = "$Id: net.c,v 1.51 1998/03/29 20:36:16 apl Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

#include "net.h"
#include "shtring.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

/* Length of the listen queue.  This many accept()able connections
   may be pending on a listening socket before further attempts
   for connections are refused. */
#define LISTEN_QUEUE_LENGTH  5


/* Buffer handling.  This network code uses dynamic buffers for intermediate
   storing of all data.  Each connection has a separate buffer for reading
   and writing.  Buffers are organized in cyclic lists of constant and
   equal size chunks, each containing header `buf_header_t' and some
   amount of bytes for data.  Read and write positions are expressed
   as a pointer to the chunk and integer position within that chunk.
   The space from read position onwards to the write position contains
   live data and the space from write postion to read position (the
   "other side") contains dead data and is considered free space.
   Each buffer is associated with an `empty_space' parameter telling
   how much free space there should at least be.  Whenever a chunk
   becomes completely dead data and at least `empty_space' bytes
   of free space is available, that chunk is freed and placed to the
   free chunks list.  When reading, if read position reaches write
   position, the buffer contains no more live data.  When writing, if
   write position reaches the read chunk, a new chunk is inserted
   between the current write chunk and the read chunk, and writing
   is resumed into this new chunk.  Chunks are allocated from the free list,
   or if none are available, malloc()ed. */

/* Buffers are allocated in chunks, each chunk including a header and
   (chunk_size - sizeof(buf_header_t)) bytes of actual buffer. */
typedef struct buf_header {
  /* pointer to previous and next buffer chunk in the cyclic list. */
  struct buf_header *next;
  struct buf_header *prev;
} buf_header_t;

typedef struct {
  buf_header_t buf_header;
  /* These are never allocated as such, so the number of `data' items
     should not have any signifficance, 1 is just a value all compilers
     accept. */
  /* This doesn't exactly conform to the C standard, but works on all
     known compilers. */
  char data[1];
} buf_chunk_t;

typedef struct {
  /* Total size of all chunks (minus headers). */
  unsigned long total_size;
  unsigned long bytes_in_buffer;
  /* Each buffer has a cyclic list of buffer chunks.  The read and write
     positions are expressed with pointer to the chunk and integer position
     in that chunk. */
  buf_chunk_t *read_chunk;
  unsigned long read_position;
  buf_chunk_t *write_chunk;
  unsigned long write_position;
  /* Number of bytes this buffer tries to keep empty.  Higher number means
     more unused chunks in the chain, resulting in possible memory wastage
     and improved performance when reading/copying into the buffer. */
  unsigned long empty_space;
} buffer_t;

/* Default amount of empty space in buffers. */
#define DEFAULT_BUFFER_EMPTY_SPACE  0

/* Macro for calculating the address of a certain position of
   a buffer chunk. */
#define CHUNK_DATA_ADDRESS(chunk, pos) (&chunk->data[pos])

/* List of free buffer chunks. */
static buf_chunk_t *free_buf_chunks = NULL;


/* Possible states of a connection, only one applies at a time. */
typedef enum {
  /* Socket is not any of the below. */
  CONN_NONE,
  /* Socket has been prepared for connecting. */
  CONN_READY_FOR_CONNECTING,
  /* Socket is actively connecting to a destination. */
  CONN_CONNECTING,
  /* Socket is listening and ready to accept a connection. */
  CONN_LISTENING,
  /* Socket is connected to an endpoint. */
  CONN_CONNECTED,
  /* Socket is closing, but has still data in write buffer. */
  CONN_CLOSING,
} connection_status_t;

/* One of these is used to hold information of each connection. */
typedef struct {
  /* File descriptor of the socket. */
  int fd;
  /* Socket status, as defined above. */
  connection_status_t status;
  /* Buffers for intermediate data storage for this connection */
  buffer_t read_buffer;
  buffer_t write_buffer;
  /* Which thread we should wake up on an event. */
  word_t thread_id;
  /* How many bytes may reside in the write buffer before
     writing into the socket returns a blocking status. */
  unsigned long write_block_threshold;
  /* Memorize error condition when trying to write in
     `net_number_of_wakeups()'. */
  long error;
  /* Destination host and port this socket is trying to connect, in case
     status == CONN_CONNECTING. */
  char *destination_host;
  short destination_port;
} connection_t;

/* Table used to hold information for each connection.  This table is indexed
   using the handle of the connection. */
static connection_t *connection = NULL;
/* Current size of the above table. */
static unsigned long connection_table_size = 0;
/* How much the above table is incremented in size every time it is
   out of space for new connections. */
#define CONNECTION_TABLE_SIZE_INCREMENT  10

/* The write block threshold defaults to this value for all new
   connections, it can be later changed using
   `net_set_write_block_threshold()'. */
#define DEFAULT_WRITE_BLOCK_THRESHOLD  (200 * 1024)

/* FD sets for select().  The BSD system call select() is used for
   polling the filedescriptors, it takes as an argument `fd_set's
   (bitfields) of filedescriptors that are tested for the possibility
   to read and write.  After the call the `fd_set's passed as arguments
   contain the descriptors that can be read from/written to.
   In `read_set' and `write_set' information of which connections wants
   to read/write is kept all the time, `return_*' sets are used for
   returned `fd_set's.  NOTE: when a socket is accepting connections,
   it will be select()ed for reading, a socket that is connecting will
   be select()ed for writing. */
/* Currently we don't do anything that might cause an exception in
   select(), so no expection set. */
static fd_set read_set;
static fd_set write_set;
static fd_set return_read_set;
static fd_set return_write_set;

/* Record the (approximately) largest value of file descriptors in use
   for select(). */
static int max_fd = 0;

/* Maximum number of memory areas given to readv and writev at a time. */
#define NUMBER_OF_IOVECS  8

/* Minimum number of iovecs. */
#define MINIMUM_IOVECS  2


static void buffer_init (void);
static buf_chunk_t *insert_chunk_before (buffer_t *buffer, buf_chunk_t *chunk);
static void free_buf_chunk (buffer_t *buffer, buf_chunk_t *buf_chunk);
static long init_buffer (buffer_t *buffer);
static long copy_to_buffer (buffer_t *buffer,
			    ptr_t p,
			    unsigned long number_of_bytes);
static long shtring_to_buffer (buffer_t *buffer,
			       ptr_t shtring,
			       unsigned long number_of_bytes);
static long read_to_buffer (buffer_t *buffer,
			   int fd,
			   unsigned long wanted_bytes);
static long copy_from_buffer (buffer_t *buffer,
			      ptr_t p,
			      unsigned long number_of_bytes);
static long make_shtring (buffer_t *buffer, ptr_t *shtring,
			  unsigned long number_of_bytes);
static long write_from_buffer (buffer_t *buffer, int fd);
static void reset_buffer (buffer_t *buffer);
static void release_buffer (buffer_t *buffer);
static net_handle_t new_connection_data (int fd);
static void reset_connection_data (net_handle_t handle);
static void destroy_connection (net_handle_t handle);


/* Buffer handling functions. */
/* Initialization: allocate buffer_pool if not yet allocated, and clear it.
   Allocated room for initial free list headers. */
static void buffer_init (void)
{
  free_buf_chunks = NULL;
}

/* Allocate (preferably from the free list, if none available, malloc())
   a new buffer chunk and insert it to the buffer `buffer' before chunk
   `chunk', if `chunk' is not NULL.  If `chunk' is NULL, set `next' and
   `prev' pointers of the new chunk to point to itself.
   Returns pointer to the new chunk, or NULL on error. */
static buf_chunk_t *insert_chunk_before (buffer_t *buffer, buf_chunk_t *chunk)
{
  buf_chunk_t *new_chunk;

  if (free_buf_chunks != NULL) {
    new_chunk = free_buf_chunks;
    free_buf_chunks = (buf_chunk_t *) free_buf_chunks->buf_header.next;
  } else {
    new_chunk = malloc(sizeof(buf_header_t) + buffer_chunk_size + 3);
    if (new_chunk == NULL) {
      perror("insert_chunk_before/malloc");
      /* Error state is passed onwards to higher level. */
      return NULL;
    }
  }

  /* Pointer magic, insert the new chunk before the given chunk
     in the buffer. */
  if (chunk != NULL) {
    chunk->buf_header.prev->next = (buf_header_t *) new_chunk;
    new_chunk->buf_header.prev = chunk->buf_header.prev;
    chunk->buf_header.prev = (buf_header_t *) new_chunk;
    new_chunk->buf_header.next = (buf_header_t *) chunk;
  } else {
    new_chunk->buf_header.prev = (buf_header_t *) new_chunk;
    new_chunk->buf_header.next = (buf_header_t *) new_chunk;
  }

  buffer->total_size += buffer_chunk_size;

  return new_chunk;
}

/* Free a buffer chunk.  Place it to the list of free chunks of corresponding
   size. */
static void free_buf_chunk (buffer_t *buffer, buf_chunk_t *chunk)
{
  chunk->buf_header.prev->next = chunk->buf_header.next;
  chunk->buf_header.next->prev = chunk->buf_header.prev;
  buffer->total_size -= buffer_chunk_size;

  chunk->buf_header.next = (buf_header_t *) free_buf_chunks;
  free_buf_chunks = chunk;
}

/* Check that the buffer has at least one allocated chunk and initialize
   it if not.
   Returns error status (0 for no error). */
static long init_buffer (buffer_t *buffer)
{
  if (buffer->write_chunk == NULL) {
    /* Allocate buffer and set it up. */
    buffer->read_chunk =
      buffer->write_chunk =
      insert_chunk_before(buffer, NULL);
    if (buffer->read_chunk == NULL) {
      return NET_BUFFER_ERROR;
    }
    buffer->read_position =
      buffer->write_position = 0;
  }

  return 0;
}


/* Copy data into a buffer.  Data is written onwards from the current
   write position.  If the chunk we are reading this buffer from is reached,
   new chunks are added before it for not to overwrite any unread data. */
static long copy_to_buffer (buffer_t *buffer,
			    ptr_t p,
			    unsigned long number_of_bytes)
{
  buf_chunk_t *rc, /* as in read chunk */
    *wc, /* as in write chunk */
    *new_chunk, *start_chunk;
  unsigned long bytes_to_copy, wp, /* as in write position */
    bib; /* bytes in buffer */
  char *ptr = (char *) p;
  
  /* First check if there is no buffer allocated yet. */
  init_buffer(buffer);

  /* Use temporary variables to hold the writing position in the buffer
     so we can get back to the original position with no data written
     to the buffer if an error occurs. */
  bib = buffer->bytes_in_buffer;
  wp = buffer->write_position;
  rc = buffer->read_chunk;
  wc = buffer->write_chunk;

  /* Copy the data to the buffer, chunk by chunk. */
  while (number_of_bytes > 0) {
    if (wp == buffer_chunk_size) {
      /* Change to the next buffer chunk. */
      if (wc->buf_header.next == (buf_header_t *) rc) {
	/* Do not overwrite any unread data, but rather add more buffer
	   chunks between write and read chunks. */
	new_chunk = insert_chunk_before(buffer, rc);
	if (new_chunk == NULL) {
	  /* Do not change any buffer information so situation remains
	     as if this function was never called and the same data can
	     later be copied to the buffer without duplicating any of it. */
	  return NET_BUFFER_ERROR;
	}
	wc = new_chunk;
      } else {
	wc = (buf_chunk_t *) wc->buf_header.next;
      }
      wp = 0;
    }

    /* How much we can copy? */
    bytes_to_copy = buffer_chunk_size - wp;
    if (bytes_to_copy > number_of_bytes) {
      bytes_to_copy = number_of_bytes;
    }

    /* Write as much as we can to this buffer chunk. */
    memcpy(CHUNK_DATA_ADDRESS(wc, wp), ptr, bytes_to_copy);
    bib += bytes_to_copy;
    wp += bytes_to_copy;
    number_of_bytes -= bytes_to_copy;
    ptr += bytes_to_copy;
  }

  /* Set the buffer information to correspond the new situation after
     copying the data in. */
  buffer->bytes_in_buffer = bib;
  buffer->write_position = wp;
  buffer->write_chunk = wc;

  return 0; /* No error. */
}

/* Copy the content of a shtring into a buffer as in `copy_to_buffer'
   above. */
static long shtring_to_buffer (buffer_t *buffer,
			       ptr_t shtring,
			       unsigned long number_of_bytes)
{
  buf_chunk_t *rc, /* as in read chunk */
    *wc, /* as in write chunk */
    *new_chunk, *start_chunk;
  unsigned long pos, len, wp, /* as in write position */
    bib; /* bytes in buffer */
  
  /* First check if there is no buffer allocated yet. */
  init_buffer(buffer);

  /* Use temporary variables to hold the writing position in the buffer
     so we can get back to the original position with no data written
     to the buffer if an error occurs. */
  bib = buffer->bytes_in_buffer;
  wp = buffer->write_position;
  rc = buffer->read_chunk;
  wc = buffer->write_chunk;
  pos = 0;

  /* Copy the data from the shtring to the buffer, chunk by chunk. */
  while (pos < number_of_bytes) {
    if (wp == buffer_chunk_size) {
      /* Change to the next buffer chunk. */
      if (wc->buf_header.next == (buf_header_t *) rc) {
	/* Do not overwrite any unread data, but rather add more buffer
	   chunks between write and read chunks. */
	new_chunk = insert_chunk_before(buffer, rc);
	if (new_chunk == NULL) {
	  /* Do not change any buffer information so situation remains
	     as if this function was never called and the same data can
	     later be copied to the buffer without duplicating any of it. */
	  return NET_BUFFER_ERROR;
	}
	wc = new_chunk;
      } else {
	wc = (buf_chunk_t *) wc->buf_header.next;
      }
      wp = 0;
    }

    /* How much we can copy? */
    len = buffer_chunk_size - wp;
    if (len > number_of_bytes - pos) {
      len = number_of_bytes - pos;
    }

    /* Write as much as we can to this buffer chunk. */
    shtring_strat(CHUNK_DATA_ADDRESS(wc, wp),
		  shtring, pos, len);
    bib += len;
    wp += len;
    pos += len;
  }

  /* Set the buffer information to correspond the new situation after
     copying the data in. */
  buffer->bytes_in_buffer = bib;
  buffer->write_position = wp;
  buffer->write_chunk = wc;

  return 0; /* No error. */
}

/* Read data into a socket's buffer from the give file descriptor.
   Data is written to the buffer as in `copy_to_buffer()' above.
   Stop immediatly if the read() call would block and return the
   error condition. */
/* Some of the variables may have somewhat misleading names.  We READ from
   the socket but we WRITE to the buffer. */
static long read_to_buffer (buffer_t *buffer,
			    int fd,
			    unsigned long wanted_bytes)
{
  buf_chunk_t *new_chunk, *rc, /* read chunk */
    *wc; /* write chunk */
  unsigned long bytes_to_read, wp, /* write position */
    bib, /* bytes in buffer */ i;
  long bytes_read, error = 0;
  struct iovec iov[NUMBER_OF_IOVECS];

  /* First check if there is no buffer allocated yet. */
  init_buffer(buffer);

  rc = buffer->read_chunk;
  bib = buffer->bytes_in_buffer;

  /* read() the data to the buffer, chunk by chunk. */
  while (wanted_bytes > bib && error == 0) {
    wc = buffer->write_chunk;
    wp = buffer->write_position;
    if (wp == buffer_chunk_size) {
      /* Change to the next buffer chunk. */
      if (wc->buf_header.next == (buf_header_t *) rc) {
	/* Do not overwrite any unread data, but rather add more chunks
	   in between write and read chunks. */
	new_chunk = insert_chunk_before(buffer, rc);
	if (new_chunk == NULL) {
	  /* Update buffer information to correspond to current situation.
	     We can continue reading from the socket later when there is
	     hopefully buffer space available again. */
	  error = NET_BUFFER_ERROR;
	  goto escape_loop;
	}
	wc = new_chunk;
      } else {
	wc = (buf_chunk_t *) wc->buf_header.next;
      }
      wp = 0;
    }

    bytes_to_read = 0;
    for (i = 0; i < NUMBER_OF_IOVECS; i++) {
      iov[i].iov_base = CHUNK_DATA_ADDRESS(wc, wp);
      iov[i].iov_len = buffer_chunk_size - wp;
      bytes_to_read += buffer_chunk_size - wp;
      wc = (buf_chunk_t *) wc->buf_header.next;
      wp = 0;
      if (wc == rc) {
	if (i < MINIMUM_IOVECS) {
	  new_chunk = insert_chunk_before(buffer, rc);
	  if (new_chunk == NULL) {
	    error = NET_BUFFER_ERROR;
	    goto escape_loop;
	  }
	  wc = new_chunk;
	} else break;
      }
    }

    wc = buffer->write_chunk;
    wp = buffer->write_position;

    /* Read as much as we can to this buffer chunk without blocking. */
    bytes_read = readv(fd, iov, i);
    if (bytes_read > 0) {
      bib += bytes_read;
      while (bytes_read > 0) {
	if (bytes_read > (buffer_chunk_size - wp)) {
	  bytes_read -= (buffer_chunk_size - wp);
	  wc = (buf_chunk_t *) wc->buf_header.next;
	  wp = 0;
	} else {
	  wp += bytes_read;
	  break;
	}
      }
      assert(wp <= buffer_chunk_size);
      buffer->write_chunk = wc;
      buffer->write_position = wp;
    } else if (bytes_read == 0) {
      error = NET_EOF;
      goto escape_loop;
    } else if (bytes_read == -1) {
      if (errno != EAGAIN) {
	error = errno;
      } else {
	error = NET_BLOCKED;
      }
      goto escape_loop;
    }
  }

escape_loop:
  buffer->bytes_in_buffer = bib;

  return error;
}

/* Read from buffer to memory area pointed to by `p'.  This function
   presumes enough data already exists in the buffer.  Copies data
   one chunk at a time, and advances read position in the buffer
   correspondingly. */
/* Note the naming of some variables, we READ from the buffer
   when we copy the data elsewhere. */
static long copy_from_buffer (buffer_t *buffer,
			      ptr_t p,
			      unsigned long number_of_bytes)
{
  buf_chunk_t *rc, *next_rc, *wc; /* read chunk */
  unsigned long bytes_to_copy, rp, /* read position */
    bib; /* bytes in buffer */
  char *ptr = (char *) p;
  
  rc = buffer->read_chunk;
  rp = buffer->read_position;
  wc = buffer->write_chunk;
  bib = buffer->bytes_in_buffer;

  while (number_of_bytes > 0) {
    if (rp == buffer_chunk_size) {
      /* A full chunk has been succesfully read, if we are not writing
	 into this chunk at the moment, we can free it.
	 Note that checking for chunk we are writing to also prevents
	 from freeing all chunks of this buffer. */
      /*
      if ((buffer->total_size - bib > buffer->empty_space) && (wc != rc)) {
	next_rc = (buf_chunk_t *) rc->buf_header.next;
	free_buf_chunk(buffer, rc);
	rc = next_rc;
      } else */ {
	rc = (buf_chunk_t *) rc->buf_header.next;
      }
      rp = 0;
    }

    /* How much data is left in this chunk to be copied. */
    bytes_to_copy = buffer_chunk_size - rp;
    if (bytes_to_copy > number_of_bytes) {
      bytes_to_copy = number_of_bytes;
    }

    memcpy(ptr, CHUNK_DATA_ADDRESS(rc, rp), bytes_to_copy);
    bib -= bytes_to_copy;
    rp += bytes_to_copy;
    number_of_bytes -= bytes_to_copy;
    ptr += bytes_to_copy;
  }

  /* Set the buffer values to reflect the situation after copying the data
     out of the buffer. */
  buffer->read_chunk = rc;
  buffer->read_position = rp;
  /* wc should never be changed in this function. */
  buffer->bytes_in_buffer = bib;

  return 0; /* No error. */
}

/* Make a shtring of length `number_of_bytes' from data in `buffer'.
   Make a shtring of each buffer chunk at a time and concatenate them
   to the shtring made so far.  If `prev_shtring' contains an already
   made shtring (ie. not NULL_PTR), the new shtrings will be concatenated
   to it. */
static long make_shtring (buffer_t *buffer, ptr_t *shtring,
			  unsigned long number_of_bytes)
{
  buf_chunk_t *rc, *next_rc, *wc; /* read chunk */
  unsigned long len, rp, /* read position */
    bib; /* bytes in buffer */
  ptr_t shtr;
  long error = 0;

  rc = buffer->read_chunk;
  rp = buffer->read_position;
  wc = buffer->write_chunk;
  bib = buffer->bytes_in_buffer;

  while (number_of_bytes > 0 && error == 0) {
    if (rp == buffer_chunk_size) {
      /* XXX we can't free chunks here as in `copy_from_buffer' because
	 we don't know if this call will succeed (`copy_from_buffer' can't
	 fail), so we need another freeing strategy. */
      rc = (buf_chunk_t *) rc->buf_header.next;
      rp = 0;
    }

    /* How much data is left in this chunk to be shtringified. */
    len = buffer_chunk_size - rp;
    if (len > number_of_bytes) {
      len = number_of_bytes;
    }

    if (!can_allocate(shtring_create_max_allocation(len))) {
      error = NET_ALLOCATION_ERROR;
    } else if (shtring_create(&shtr, CHUNK_DATA_ADDRESS(rc, rp), len) == -1) {
      error = NET_SHTRING_ERROR;
    } else if (*shtring != NULL_PTR) {
      if (!can_allocate(shtring_cat_max_allocation(*shtring, shtr))) {
	error = NET_ALLOCATION_ERROR;
      } else {
	bib -= len;
	rp += len;
	number_of_bytes -= len;
	*shtring = shtring_cat(*shtring, shtr);
      }
    } else {
      bib -= len;
      rp += len;
      number_of_bytes -= len;
      *shtring = shtr;
    }
  }

  /* Set the buffer values to reflect the situation after copying the data
     out of the buffer. */
  buffer->read_chunk = rc;
  buffer->read_position = rp;
  /* wc should never be changed in this function. */
  buffer->bytes_in_buffer = bib;

  return error;
}

/* Write to the give filedescriptor from the given buffer.  Read data
   as in `copy_from_buffer()' above.  If write() blocks, stop immediatly
   and return the error status, otherwise continue until all data has
   been written. */
static long write_from_buffer (buffer_t *buffer, int fd)
{
  buf_chunk_t *rc, *next_rc, *wc; /* read chunk */
  unsigned long bytes_to_write, rp, /* read position */
    bib, /* bytes in buffer */ i;
  long bytes_written, error = 0;
  struct iovec iov[NUMBER_OF_IOVECS];

  rc = buffer->read_chunk;
  rp = buffer->read_position;
  wc = buffer->write_chunk;
  bib = buffer->bytes_in_buffer;
  
  while (bib > 0 && error == 0) {
    /* See similar case in `copy_from_buffer()' above. */
    if (rp == buffer_chunk_size) {
      /*      if ((buffer->total_size - bib > buffer->empty_space) && (wc != rc)) {
	next_rc = (buf_chunk_t *) rc->buf_header.next;
	free_buf_chunk(buffer, rc);
	rc = next_rc;
      } else */ {
	rc = (buf_chunk_t *) rc->buf_header.next;
      }
      rp = 0;
    }

    bytes_to_write = 0;
    for (i = 0;
	 i < NUMBER_OF_IOVECS
	   && bib > 0;
	 i++) {
      iov[i].iov_base = CHUNK_DATA_ADDRESS(rc, rp);
      if (bib > (buffer_chunk_size - rp)) {
	iov[i].iov_len = buffer_chunk_size - rp;
	bytes_to_write += buffer_chunk_size - rp;
	bib -= buffer_chunk_size - rp;
	rc = (buf_chunk_t *) rc->buf_header.next;
	rp = 0;
      } else {
	iov[i].iov_len = bib;
	bytes_to_write += bib;
	rp += bib;
	bib = 0;
      }
    }

    rc = buffer->read_chunk;
    rp = buffer->read_position;
    bib = buffer->bytes_in_buffer;

    bytes_written = writev(fd, iov, i);
    if (bytes_written > 0) {
      bib -= bytes_written;
      while (bytes_written > 0) {
	if (bytes_written > (buffer_chunk_size - rp)) {
	  bytes_written -= buffer_chunk_size - rp;
	  rc = (buf_chunk_t *) rc->buf_header.next;
	  rp = 0;
	} else {
	  rp += bytes_written;
	  break;
	}
      }
      buffer->read_chunk = rc;
      buffer->read_position = rp;
      buffer->bytes_in_buffer = bib;
    } else if (bytes_written == -1) {
      if (errno != EAGAIN) {
	error = errno;
      } else {
	error = NET_BLOCKED;
      }
    }
  }

  return error;
}


static void reset_buffer (buffer_t *buffer)
{
  buffer->read_chunk = NULL;
  buffer->write_chunk = NULL;
  buffer->read_position = 0;
  buffer->write_position = 0;
  buffer->total_size = 0;
  buffer->bytes_in_buffer = 0;
  buffer->empty_space = DEFAULT_BUFFER_EMPTY_SPACE;
}

static void release_buffer (buffer_t *buffer)
{
  buf_chunk_t *to_release, *first_buffer, *current;

  current = first_buffer = buffer->read_chunk;

  if (current != NULL) do {
    to_release = current;
    current = (buf_chunk_t *) current->buf_header.next;
    free_buf_chunk(buffer, to_release);
  } while (buffer->total_size > 0);
  
  assert(buffer->total_size == 0);

  reset_buffer(buffer);
}

/* A new socket has been established, make sure the array of connections
   is big enough to hold information for it and reset its data. */
static net_handle_t new_connection_data (int fd)
{
  static net_handle_t cursor = 0;
  unsigned long i;

  for (i = connection_table_size; i > 0;
       i--, cursor = (cursor + 1) % connection_table_size)
    if (connection[cursor].status == CONN_NONE)
      break;
  
  if (i == 0) {
    connection = realloc(connection,
			 (connection_table_size
			  + CONNECTION_TABLE_SIZE_INCREMENT)
			 * sizeof(connection_t));
    if (connection == NULL) {
      perror("new_connection_data/realloc");
      exit(1);
    }
    for (i = connection_table_size;
	 i < connection_table_size + CONNECTION_TABLE_SIZE_INCREMENT;
	 i++) {
      reset_connection_data(i);
    }
    cursor = connection_table_size;
    connection_table_size += CONNECTION_TABLE_SIZE_INCREMENT;
  } else {
    reset_connection_data(cursor);
  }

  connection[cursor].fd = fd;
  if (fd > max_fd)
    max_fd = fd;

  return cursor;
}

static void reset_connection_data (net_handle_t handle)
{
  connection_t *c = &connection[handle];

  c->status = CONN_NONE;
  c->thread_id = NULL_WORD;
  c->write_block_threshold = DEFAULT_WRITE_BLOCK_THRESHOLD;
  c->error = 0;
  c->destination_host = NULL;
  c->destination_port = 0;
  reset_buffer(&(c->read_buffer));
  reset_buffer(&(c->write_buffer));
  c->fd = -1;
}

static void destroy_connection (net_handle_t handle)
{
  connection_t *c = &connection[handle];

  if (c->fd >= 0) {
    FD_CLR(c->fd, &read_set);
    FD_CLR(c->fd, &write_set);
    FD_CLR(c->fd, &return_read_set);
    FD_CLR(c->fd, &return_write_set);
  }
  release_buffer(&c->read_buffer);
  release_buffer(&c->write_buffer);
  close(c->fd);
  reset_connection_data(handle);
}


/* The actual network handling functions.  The usages
   are documented in net.h. */
void net_init (void)
{
  buffer_init();

  connection = NULL;
  connection_table_size = 0;

  FD_ZERO(&read_set);
  FD_ZERO(&write_set);
  FD_ZERO(&return_read_set);
  FD_ZERO(&return_write_set);

  signal(SIGPIPE, SIG_IGN);
}

net_return_t net_write_mem (word_t thread_id,
			    net_handle_t handle, ptr_t p,
			    unsigned long number_of_bytes)
{
  connection_t *c = &connection[handle];
  net_return_t ret;
  long error;

  ret.handle = handle;
  ret.thread_id = thread_id;
  if (c->error != 0) {
    ret.error = c->error;
    if (NET_IS_FATAL_ERROR(c->error))
      destroy_connection(handle);
    return ret;
  }

  /* Sanity check, the socket should be connected. */
  assert(c->status == CONN_CONNECTED);

  if (c->write_buffer.bytes_in_buffer >
      c->write_block_threshold) {
    ret.error = NET_BLOCKED;
    return ret;
  }

  /* Copy data to the socket's buffer before anything else. */
  error = copy_to_buffer(&(c->write_buffer), p, number_of_bytes);
  if (error != 0) {
    ret.error = error;
    return ret;
  }

  /* Attempt to write as much as possible. */
  error = write_from_buffer(&(c->write_buffer), c->fd);

  /* Even if write() blocked, we do not return a blocking condition
     because we can internally buffer data to be written. */
  ret.error = (error != NET_BLOCKED ? error : 0);
  ret.event = NET_WRITE_EVENT;
  ret.bytes_left = c->write_buffer.bytes_in_buffer;

  if (error == 0) {
    /* write() did not block, so all data in the write buffer has been
       written to the socket, make sure socket isn't select()ed for
       writing anymore. */
    FD_CLR(c->fd, &write_set);
  } else if (error == NET_BLOCKED) {
    /* There's still data waiting in the buffer to be written, so mark
       this socket to be select()ed for writing and record the thread ID
       to wake up when pending write has been flushed. */
    FD_SET(c->fd, &write_set);
    c->thread_id = thread_id;
  } else {
    /* Error (other than EAGAIN) occured, socket is not connected anymore. */
    destroy_connection(handle);
    ret.event = NET_ERROR_EVENT;
  }

  return ret;
}

net_return_t net_write_char (word_t thread_id, net_handle_t handle, char c)
{
  return net_write_mem(thread_id, handle, (ptr_t) &c, 1);
}

net_return_t net_write_shtring (word_t thread_id,
				net_handle_t handle, ptr_t shtring,
				unsigned long number_of_bytes)
{
  connection_t *c = &connection[handle];
  net_return_t ret;
  long error;

  ret.handle = handle;
  ret.thread_id = thread_id;
  if (c->error != 0) {
    ret.error = c->error;
    if (NET_IS_FATAL_ERROR(c->error))
      destroy_connection(handle);
    return ret;
  }

  /* Sanity check, the socket should be connected. */
  assert(c->status == CONN_CONNECTED);

  if (c->write_buffer.bytes_in_buffer >
      c->write_block_threshold) {
    ret.error = NET_BLOCKED;
    return ret;
  }

  /* Copy the shtring to the socket's buffer before anything else. */
  if (number_of_bytes == 0 || number_of_bytes > shtring_length(shtring))
    number_of_bytes = shtring_length(shtring);
  error = shtring_to_buffer(&(c->write_buffer),
			    shtring, number_of_bytes);
  if (error != 0) {
    ret.error = error;
    return ret;
  }

  /* Attempt to write as much as possible. */
  error = write_from_buffer(&(c->write_buffer), c->fd);

  ret.handle = handle;
  ret.thread_id = thread_id;
  /* Even if write() blocked, we do not return a blocking condition
     because we can internally buffer data to be written. */
  ret.error = (error != NET_BLOCKED ? error : 0);
  ret.event = NET_WRITE_EVENT;
  ret.bytes_left = c->write_buffer.bytes_in_buffer;

  if (error == 0) {
    /* write() did not block, so all data in the write buffer has been
       written to the socket, make sure socket isn't select()ed for
       writing anymore. */
    FD_CLR(c->fd, &write_set);
  } else if (error == NET_BLOCKED) {
    /* There's still data waiting in the buffer to be written, so mark
       this socket to be select()ed for writing and record the thread ID
       to wake up when pending write has been flushed. */
    FD_SET(c->fd, &write_set);
    c->thread_id = thread_id;
  } else {
    /* Error (other than EAGAIN) occured, socket is not connected anymore. */
    destroy_connection(handle);
    ret.event = NET_ERROR_EVENT;
  }

  return ret;
}

net_return_t net_read_mem (word_t thread_id,
			   net_handle_t handle, ptr_t p,
			   unsigned long number_of_bytes)
{
  connection_t *c = &connection[handle];
  net_return_t ret;
  int error = 0;

  ret.handle = handle;
  ret.thread_id = thread_id;
  if (c->error != 0) {
    ret.error = c->error;
    if (NET_IS_FATAL_ERROR(c->error))
      destroy_connection(handle);
    return ret;
  }

  /* Sanity check.  The socket should be connected if we want to read. */
  assert(c->status == CONN_CONNECTED);

  /* Attempt to read as much as possible to the socket's read buffer
     without blocking, if not enough data is not yet available. */
  if (c->read_buffer.bytes_in_buffer < number_of_bytes)
    error = read_to_buffer(&(c->read_buffer), c->fd, number_of_bytes);

  ret.handle = handle;
  ret.thread_id = thread_id;
  ret.error = error;
  ret.event = NET_READ_EVENT;
  ret.bytes_left = number_of_bytes -
    c->read_buffer.bytes_in_buffer;

  /* Check if we have enough data, if so, copy it to the memory position
     pointed to by p. */
  if (c->read_buffer.bytes_in_buffer >= number_of_bytes) {
    /* We got enough data to the buffer, copy it to the location given
       and make sure this fd is not selected() for reading anymore. */
    copy_from_buffer(&(c->read_buffer), p, number_of_bytes);
    FD_CLR(c->fd, &read_set);
    ret.error = 0;
  } else if ((error == 0) || (error == NET_BLOCKED)) {
    /* read() succeeded or blocked, but whichever the case, there is not
       enough data in the read buffer, so this call blocked.  Set the fd
       to be polled for reading and record the thread ID to be resumed
       when this fd wakes up. */
    /* XXX I don't think error can be == 0 in here. */
    FD_SET(c->fd, &read_set);
    c->thread_id = thread_id;
    ret.event = NET_READ_EVENT;
  } else {
    /* Another (system) error occured, this socket is not connected anymore,
       so make sure it's not polled anymore. */
    destroy_connection(handle);
    ret.event = NET_ERROR_EVENT;
  }

  return ret;
}

net_return_t net_read_shtring (word_t thread_id, ptr_t shtring,
			       net_handle_t handle,
			       unsigned long number_of_bytes)
{
  connection_t *c = &connection[handle];
  net_return_t ret;
  int error;

  ret.handle = handle;
  ret.thread_id = thread_id;
  if (c->error != 0) {
    ret.error = c->error;
    if (NET_IS_FATAL_ERROR(c->error))
      destroy_connection(handle);
    return ret;
  }

  /* Sanity check.  The socket should be connected if we want to read. */
  assert(c->status == CONN_CONNECTED);

  /* Attempt to read as much as possible to the socket's read buffer
     without blocking, if not enough data is available yet. */
  if (c->read_buffer.bytes_in_buffer < number_of_bytes)
    error = read_to_buffer(&(c->read_buffer), c->fd, number_of_bytes);

  ret.handle = handle;
  ret.thread_id = thread_id;
  ret.error = error;
  ret.event = NET_READ_EVENT;
  ret.bytes_left = number_of_bytes - c->read_buffer.bytes_in_buffer;
  ret.shtring = shtring;

  /* Check if we have enough data, if so, shtringify `number_of_bytes'
     bytes. */
  if (c->read_buffer.bytes_in_buffer >= number_of_bytes) {
    /* Shtringify and place the resulting shtring (if succesful) to
       the return value.  `make_shtring' can still fail, return its
       status as the error status of this call. */
    ret.error = make_shtring(&(c->read_buffer),
			     &ret.shtring,
			     number_of_bytes);
    /* Remove the file descriptor from the read set, because enough data
       is in the buffer.  For succesful completion of this call, more
       space should be made available for allocation (ie. first generation
       should be collected). */
    FD_CLR(c->fd, &read_set);
  } else if ((error == 0) || (error == NET_BLOCKED)) {
    /* read() succeeded or blocked, but whichever the case, there is not
       enough data in the read buffer, so this call blocked.  Set the fd
       to be polled for reading and record the thread ID to be resumed
       when this fd wakes up. */
    /* XXX I don't think error can be == 0 in here. */
    FD_SET(c->fd, &read_set);
    c->thread_id = thread_id;
    ret.event = NET_READ_EVENT;
  } else {
    /* Another (system) error occured, this socket is not connected anymore,
       so make sure it's not polled anymore. */
    destroy_connection(handle);
    ret.event = NET_ERROR_EVENT;
  }

  return ret;
}

net_return_t net_read_char (word_t thread_id, net_handle_t handle, char *c)
{
  return net_read_mem(thread_id, handle, (ptr_t) c, 1);
}

net_return_t net_listen (int port)
{
  net_handle_t handle;
  net_return_t ret;
  struct sockaddr_in local_addr;
  int fd;

  if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    perror("net_listen()/socket()");
    ret.error = errno;
    return ret;
  }
  if (fcntl(fd, F_SETFL, (fcntl(fd, F_GETFL) | O_NONBLOCK)) == -1) {
    perror("net_listen()/fcntl");
    close(fd);
    exit(1);
  }

  memset(&local_addr, 0, sizeof(struct sockaddr_in));
  local_addr.sin_family = PF_INET;
  local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  local_addr.sin_port = htons(port);

  if (bind(fd,
	   (struct sockaddr *) &local_addr,
	   sizeof(struct sockaddr_in)) == -1) {
    perror("net_listen()/bind()");
    close(fd);
    ret.error = errno;
    return ret;
  }
  if (listen(fd, LISTEN_QUEUE_LENGTH) == -1) {
    perror("net_listen()/listen");
    close(fd);
    ret.error = errno;
    return ret;
  }

  handle = new_connection_data(fd);
  connection[handle].status = CONN_LISTENING;

  ret.handle = handle;
  ret.thread_id = NULL_WORD;
  ret.error = 0;
  ret.event = NET_NO_EVENT;
  
  return ret;
}

net_return_t net_accept (word_t thread_id, net_handle_t handle)
{
  connection_t *c = &connection[handle];
  net_handle_t new_handle;
  net_return_t ret;
  int new_fd;

  ret.handle = handle;
  ret.thread_id = thread_id;
  if (c->error != 0) {
    ret.error = c->error;
    c->error = 0;
    return ret;
  }

  /* Some sanity checking, the socket to accept from must be listening. */
  assert(c->status == CONN_LISTENING);

  if ((new_fd = accept(c->fd, NULL, 0)) == -1) {
    if (errno == EAGAIN) {
      /* Blocked, mark the fd to be select()ed for reading, as incoming
	 connections are polled by select()ing for reading. */
      FD_SET(c->fd, &read_set);
      c->thread_id = thread_id;
      ret.error = NET_BLOCKED;
      return ret;
    } else {
      /* A system error occured. */
      perror("net_accept()/accept()");
      ret.error = errno;
      return ret;
    }
  }

  /* No blocking accept on the listening fd anymore,
     so remove it from the fd set. */
  FD_CLR(c->fd, &read_set);

  /* Set the new filedescriptor non-blocking. */
  if (fcntl(new_fd, F_SETFL, (fcntl(new_fd, F_GETFL) | O_NONBLOCK)) == -1) {
    perror("net_accept()/fcntl()");
    exit(1);
  }

  new_handle = new_connection_data(new_fd);
  connection[new_handle].status = CONN_CONNECTED;
  
  ret.handle = new_handle;
  ret.thread_id = thread_id;
  ret.error = 0;
  ret.event = NET_ACCEPT_EVENT;

  return ret;
}

net_return_t net_prepare_connect (char *host_name, short port)
{
  net_handle_t handle;
  net_return_t ret;
  int fd;

  fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("net_prepare_connect()/socket()");
    ret.error = errno;
    return ret;
  }
  if (fcntl(fd, F_SETFL, (fcntl(fd, F_GETFL) | O_NONBLOCK)) == -1) {
    perror("net_prepare_connect()/fcntl()");
    exit(1);
  }
  
  handle = new_connection_data(fd);
  connection[handle].status = CONN_READY_FOR_CONNECTING;
  connection[handle].destination_port = port;
  connection[handle].destination_host = strdup(host_name);
  if (connection[handle].destination_host == NULL) {
    fprintf(stderr,
	    "net_prepare_connect(): failed to strdup() target host name.\n");
    exit(1);
  }
  
  ret.error = 0;
  ret.handle = handle;

  return ret;
}
    
net_return_t net_do_connect (word_t thread_id, net_handle_t handle)
{
  connection_t *c = &connection[handle];
  net_return_t ret;
  struct hostent *host;
  struct sockaddr_in addr;

  ret.handle = handle;
  ret.thread_id = thread_id;
  if (c->error != 0) {
    ret.error = c->error;
    c->error = 0;
    return ret;
  }

  /* Sanity check, the socket must be readied for connecting with
     `net_prepare_connect()' or already connecting. */
  assert(c->status == CONN_READY_FOR_CONNECTING ||
	 c->status == CONN_CONNECTING);

  if (c->status == CONN_READY_FOR_CONNECTING) {
    /* This call blocks indefinetly! */
    /* XXX the ways to prevent this would be using threads or another
       process to do the DNS lookup. */
    host = gethostbyname(c->destination_host);
    if (host == NULL) {
      /* The target host is unknown, so clear this socket from being
	 ready for connecting. */
      perror("net_do_connect()/gethostbyname()");
      ret.error = errno;
      destroy_connection(handle);
      return ret;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = PF_INET;
    memcpy(&addr.sin_addr.s_addr, host->h_addr, host->h_length);
    addr.sin_port = htons(c->destination_port);
    
    if (connect(c->fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
      if (errno != EAGAIN) {
	/* System error (other than blocking) occured, mark the socket
	   to be undetermined and make sure it's not polled. */
	perror("net_do_connect()/connect()");
	ret.error = errno;
	destroy_connection(handle);
	return ret;
      } else {
	/* connect() blocked, set this fd to be select()ed for writing
	   to poll for succesful connection later in
	   `net_number_of_wakeups()'. */
	FD_SET(c->fd, &write_set);
	connection[handle].thread_id = thread_id;
	ret.handle = handle;
	ret.thread_id = thread_id;
	ret.error = NET_BLOCKED;
	ret.event = NET_CONNECT_EVENT;
	return ret;
      }
    }
    
    FD_CLR(c->fd, &read_set);
    FD_CLR(c->fd, &write_set);
    c->status = CONN_CONNECTED;

    ret.handle = handle;
    ret.thread_id = thread_id;
    ret.error = 0;
    ret.event = NET_CONNECT_EVENT;
  } else if (c->status == CONN_CONNECTING) {
    /* This socket already blocked and has been succesfully selected for
       writing.  There is no way to check for the success of connecting,
       so we just assume it is connected. */
    FD_CLR(c->fd, &write_set);
    FD_CLR(c->fd, &read_set);
    c->status = CONN_CONNECTED;

    ret.handle = handle;
    ret.thread_id = thread_id;
    ret.error = 0;
    ret.event = NET_CONNECT_EVENT;
  }

  return ret;
}

net_return_t net_close (word_t thread_id, net_handle_t handle)
{
  connection_t *c = &connection[handle];
  net_return_t ret;

  ret.handle = handle;
  ret.thread_id = thread_id;
  if (c->error != 0) {
    ret.error = c->error;
    c->error = 0;
    return ret;
  }

  assert(c->status != CONN_NONE);

  if (c->write_buffer.bytes_in_buffer > 0 &&
      c->status == CONN_CONNECTED) {
    c->status = CONN_CLOSING;
    c->thread_id = thread_id;
    FD_SET(c->fd, &write_set);
    ret.error = NET_BLOCKED;
    ret.bytes_left = c->write_buffer.bytes_in_buffer;
  } else {
    release_buffer(&c->read_buffer);
    release_buffer(&c->write_buffer);
    if (c->destination_host != NULL) {
      free(c->destination_host);
    }
    close(c->fd);
    reset_connection_data(handle);
    ret.error = 0;
    ret.thread_id = c->thread_id;
    ret.event = NET_CLOSE_EVENT;
  }

  return ret;
}

void net_set_read_empty_buffer_size (net_handle_t handle, unsigned long size)
{
  assert(connection[handle].status == CONN_CONNECTED);

  connection[handle].read_buffer.empty_space = size;
}

void net_set_write_empty_buffer_size (net_handle_t handle, unsigned long size)
{
  assert(connection[handle].status == CONN_CONNECTED);

  connection[handle].write_buffer.empty_space = size;
}

void net_set_write_block_threshold (net_handle_t handle,
				    unsigned long number_of_bytes)
{
  assert(connection[handle].status == CONN_CONNECTED);

  connection[handle].write_block_threshold = number_of_bytes;
}

/* The current wakeup handle cursor. */
static unsigned long wakeup_handle;

/* The number of wakeups generated by the last call to
   `net_number_of_wakeups'. */
static int wakeups;

unsigned long net_number_of_wakeups (struct timeval *timeout)
{
  connection_t *c;
  long error;
  unsigned long i;

  wakeup_handle = 0;
  memcpy(&return_read_set, &read_set, sizeof(fd_set));
  memcpy(&return_write_set, &write_set, sizeof(fd_set));
  wakeups = select(max_fd + 1,
		   &return_read_set,
		   &return_write_set,
		   NULL,
		   timeout);

  if (wakeups == -1) {
    /* Do not barf on this error, rather just report no wakeups. */
    perror("net_number_of_wakeups/select");
    wakeups = 0;
  }

  /* Go through all sockets, resume all pending writes on sockets that have
     been succesfully select()ed for writing. */
  for (i = 0; (i < connection_table_size) && (wakeups > 0); i++) {
    c = &connection[i];
    if (c->status != CONN_NONE) {
      /* Note that a socket may have been select()ed succesfully for writing
	 if it's either trying to write or trying to connect. */
      if (FD_ISSET(c->fd, &return_write_set) &&
	  c->status == CONN_CONNECTED) {
	/* This is not a wakeup yet. */
	wakeups--;
	
	error = c->error =
	  write_from_buffer(&(c->write_buffer), c->fd);
	if (error == 0) {
	  /* No error, all data has been written, so this is a wakeup. */
	  wakeups++;
	  FD_CLR(c->fd, &write_set);
	} else if (error == NET_BLOCKED) {
	  /* Still blocked, this is not a wakeup. */
	  FD_SET(c->fd, &write_set);
	  FD_CLR(c->fd, &return_write_set);
	  c->error = 0;
	} else {
	  /* Other error condition, return it as an error event. */
	  FD_CLR(c->fd, &return_write_set);
	  /* If this was returned also for reading, we have to subtract
	     wakeups and clear the reading status, because we return
	     only one wakeup (the error). */
	  if (FD_ISSET(c->fd, &return_read_set)) {
	    FD_CLR(c->fd, &return_read_set);
	    wakeups--;
	  }
	  wakeups++;
	}
      }
    }
  }

  return wakeups;
}
		   
net_return_t net_get_wakeup (void)
{
  connection_t *c;
  net_return_t ret;

  assert(wakeups > 0);

  while (c = &connection[wakeup_handle],
	 wakeup_handle < connection_table_size &&
	 (c->status == CONN_NONE ||
	  (!FD_ISSET(c->fd, &return_read_set) &&
	   !FD_ISSET(c->fd, &return_write_set) &&
	   c->error == 0))) {
    wakeup_handle++;
  }

  if (wakeup_handle >= connection_table_size) {
    ret.error = NET_NO_MORE_WAKEUPS;
    ret.event = NET_ERROR_EVENT;
    return ret;
  }

  ret.handle = wakeup_handle;
  ret.thread_id = c->thread_id;
  ret.error = 0;
  
  /* Check what kind of wakeup event we get, fill in return value
     correspondingly, and remove the blocking status from the
     filedescriptor. */
  if (c->error != 0) {
    ret.event = NET_ERROR_EVENT;
  } else if (FD_ISSET(c->fd, &return_read_set)) {
    if (c->status == CONN_LISTENING) {
      ret.event = NET_ACCEPT_EVENT;
    } else if (c->status == CONN_CONNECTED) {
      ret.event = NET_READ_EVENT;
    } else {
      /* Unknown event, should never happen. */
    }
    /* Remove the file descriptor from both sets so that it won't be
       returned as wakeup before really blocking on a blockable request. */
    FD_CLR(c->fd, &read_set);
    FD_CLR(c->fd, &return_read_set);
  } else if (FD_ISSET(c->fd, &return_write_set)) {
    if (c->status == CONN_CONNECTING) {
      ret.event = NET_CONNECT_EVENT;
    } else if (c->status == CONN_CLOSING) {
      ret.event = NET_CLOSE_EVENT;
    } else if (c->status == CONN_CONNECTED) {
      ret.event = NET_WRITE_EVENT;
    } else {
      /* Unknown event, should never happen. */
      assert(0);
    }
    FD_CLR(c->fd, &write_set);
    FD_CLR(c->fd, &return_write_set);
  }

  wakeups--;

  return ret;
}
