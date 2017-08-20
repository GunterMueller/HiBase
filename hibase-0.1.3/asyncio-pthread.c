/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Antti-Pekka Liedes <apl@cs.hut.fi>
 */

/* Asynchronous IO routines using POSIX.4a threads.
 */

static char *rev_id = "$Id: asyncio-pthread.c,v 1.10 1997/09/16 09:20:08 apl Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


#include <sys/times.h>

#include "includes.h"
#include "params.h"
#include "io.h"
#include "cookies.h"
#include "bitops.h"
#include "root.h"


/* Asynchronous IO with POSIX threads.  */
#include <pthread.h>
#include <signal.h>
#include <errno.h>

/* If `O_DSYNC' is given, use it in file modes.  Otherwise, use
   `O_SYNC'.  If even it isn't given, use `fsync'-calls. */
#ifdef O_DSYNC
#define O_SYNC_FLAG  O_DSYNC
#else
#ifdef O_SYNC
#define O_SYNC_FLAG  O_SYNC
#else
#define USE_FSYNC  1
#define O_SYNC_FLAG  0
#endif
#endif


#ifdef FILE_LOAD_BALANCING
/* The size of the queue to hold the history of times taken to write
   data to the file.  The longer the queue, the longer the information
   of how long a write took is memorized and included to the load
   of the file. */
#define FILE_LOAD_HISTORY_SIZE  50
/* This is a small load value of a single write, used when the load of a file
   is artificially decreased. */
/* XXX 1000 is a value for ares.  Other values are probably applicable on
   other systems, 0 is always safe but quite low, especially with short
   history queues. */
#define SMALL_LOAD  1000
#endif


/* This is used to list all pending writes. */

typedef enum {
  AIO_READ, AIO_WRITE
} aio_type_t;

typedef struct aio_t {
  unsigned long page_number;
  ptr_t data;
  unsigned long number_of_bytes;
  struct aio_t *next;
} aio_t;

static aio_t *list_of_free_aios = NULL;
static pthread_mutex_t free_aios_lock;

/* A struct for each file (disk) we use.  Stores information required
   by the threads.  NOTE that this is not the same file_t as in io.c. */
typedef struct {
  unsigned long number;
  int fd;
  pthread_t thread;
  pthread_cond_t write_is_waiting;
  pthread_cond_t write_is_finished;
  aio_t *head_of_pending_writes;
  aio_t *tail_of_pending_writes;
  pthread_mutex_t writes_lock;
  unsigned long number_of_pending_writes;

  pthread_cond_t read_is_waiting;
  pthread_cond_t read_is_finished;
  aio_t *head_of_pending_reads;
  aio_t *tail_of_pending_reads;
  pthread_mutex_t reads_lock;
  unsigned long number_of_pending_reads;

#ifdef FILE_LOAD_BALANCING
  long load;
  long load_history[FILE_LOAD_HISTORY_SIZE];
  unsigned long load_cursor;
#endif
#ifdef FILE_USAGE_STATISTICS
  unsigned long number_of_writes;
#endif
} file_t;


static file_t *file = NULL;

static unsigned long number_of_files = 0;

#ifdef FILE_USAGE_STATISTICS
static unsigned long number_of_writes;
#endif

/* These functions serve incoming IO-calls, each file has a thread for reading
   and writing looping in these functions. */
static void *reader_thread (void *arg);
static void *writer_thread (void *arg);

static void prepare_file (unsigned long file_number);

/* See asyncio.h for more documentation about asyncio_ -functions. */

void asyncio_init (unsigned long num_of_files)
{
  number_of_files = num_of_files;
  file = (file_t *) malloc(number_of_files * sizeof(file_t));
  if (file == NULL) {
    perror("asyncio_init/malloc");
    exit(1);
  }

  if (pthread_mutex_init(&free_aios_lock, NULL)) {
    perror("asyncio_init/sem_init");
    exit(1);
  }
#ifndef FILE_LOAD_BALANCING
  if (file_load_is_displayed)
    fprintf(stderr, "Warning: no file loads are displayed because\n"
	            "         file load balancing is not enabled.\n");
#endif
#ifndef FILE_USAGE_STATISTICS
  if (file_usage_is_displayed)
    fprintf(stderr, "Warning: no file usage statistics are displayed because\n"
	            "         file usage statistics are not enabled.\n");
#endif
}

void asyncio_create_file (unsigned long file_number, int fd)
{
  /* Here we duplicate the filedescriptor to avoid badly timed
     lseek()s from messing file cursors. */
  file[file_number].fd = dup(fd);
  if (file[file_number].fd == -1) {
    perror("asyncio_create_file/dup");
    exit(1);
  }
  prepare_file(file_number);
}

void asyncio_open_file (unsigned long file_number, int fd)
{
  /* Here we duplicate the filedescriptor to avoid badly timed
     lseek()s from messing file cursors. */
  file[file_number].fd = dup(fd);
  if (file[file_number].fd == -1) {
    perror("asyncio_open_file/dup");
    exit(1);
  }
  prepare_file(file_number);
}

void prepare_file (unsigned long file_number)
{
  file_t *f = &file[file_number];
  unsigned long i;

  f->number = file_number;

  pthread_cond_init(&f->write_is_waiting, NULL);
  pthread_cond_init(&f->write_is_finished, NULL);
  f->head_of_pending_writes = NULL;
  f->tail_of_pending_writes = NULL;
  pthread_mutex_init(&f->writes_lock, NULL);
  f->number_of_pending_writes = 0;

  if (pthread_create(&f->thread, NULL, writer_thread, f) != 0) {
    perror("prepare_file/pthread_create");
    exit(1);
  }

  pthread_cond_init(&f->read_is_waiting, NULL);
  pthread_cond_init(&f->read_is_finished, NULL);
  f->head_of_pending_reads = NULL;
  f->tail_of_pending_reads = NULL;
  pthread_mutex_init(&f->reads_lock, NULL);
  f->number_of_pending_reads = 0;

  if (pthread_create(&f->thread, NULL, reader_thread, f) != 0) {
    perror("prepare_file/pthread_create");
    exit(1);
  }

#ifdef FILE_LOAD_BALANCING
  for (i = 0; i < FILE_LOAD_HISTORY_SIZE; i++) {
    f->load_history[i] = 0;
  }
  f->load = 0;
  f->load_cursor = 0;
#endif
#ifdef FILE_USAGE_STATISTICS
  f->number_of_writes = 0;
#endif
}

void asyncio_close_file (unsigned long file_number, int fd)
{
  /* XXX what if there are pending operations for this file? */
  close(file[file_number].fd);
}

long asyncio_get_file_load (unsigned long file_number)
{
#ifdef FILE_LOAD_BALANCING
  if (file_load_is_displayed)
    fprintf(stderr, "Load for file %ld is %ld.\n",
	    file_number,
	    (file[file_number].number_of_pending_writes + 1)
	    * file[file_number].load);
  return (file[file_number].number_of_pending_writes + 1)
    * file[file_number].load;
#else
  return 0;
#endif
}

/* Artificially reduce the load of a file.  This is used to add good
   values to load history to give every file a chance every now and then. */
void asyncio_reduce_file_load (unsigned long file_number)
{
#ifdef FILE_LOAD_BALANCING
  file_t *f = &file[file_number];

  f->load -= f->load_history[f->load_cursor];
  f->load += SMALL_LOAD;
  f->load_history[f->load_cursor] = SMALL_LOAD;
  f->load_cursor = (f->load_cursor + 1) % FILE_LOAD_HISTORY_SIZE;
#endif
}

int asyncio_write_page (unsigned long file_number, unsigned long page_number,
			ptr_t ptr, unsigned long number_of_bytes)
{
  aio_t *aio;
  int err;
  file_t *f = &file[file_number];

  /* Malloc, or preferably pop from the `list_of_free_aios', a new aio
     and push it to the end of the list of pending writes to this file. */
  if (pthread_mutex_lock(&free_aios_lock)) {
    perror("asyncio_write_page/pthread_mutex_lock");
    exit(1);
  }
  if (list_of_free_aios == NULL) {
    aio = malloc(sizeof(aio_t));
    if (aio == NULL) {
      perror("asyncio_write_page/malloc");
      exit(1);
    }
  } else {
    aio = list_of_free_aios;
    list_of_free_aios = aio->next;
  }
  if (pthread_mutex_unlock(&free_aios_lock)) {
    perror("asyncio_write_page/pthread_mutex_unlock");
    exit(1);
  }
  aio->next = NULL;
  aio->page_number = page_number;
  aio->data = ptr;
  aio->number_of_bytes = number_of_bytes;
  
  /* Add this to the end of the pending writes queue and signal the
     writer thread that one or more writes are pending. */
  if (pthread_mutex_lock(&f->writes_lock)) {
    perror("asyncio_write_page/pthread_mutex_lock");
    exit(1);
  }
  if (f->head_of_pending_writes == NULL)
    f->head_of_pending_writes = f->tail_of_pending_writes = aio;
  else {
    f->tail_of_pending_writes->next = aio;
    f->tail_of_pending_writes = aio;
  }
  f->number_of_pending_writes++;
  if (pthread_cond_signal(&f->write_is_waiting)) {
    perror("asyncio_write_page/pthread_cond_signal");
    exit(1);
  }
  if (pthread_io_is_verbose) {
    fprintf(stderr, "write request: fn = %d dpn = %d nbytes = %d\n",
	    file_number, page_number, number_of_bytes);
  }
  if (pthread_mutex_unlock(&f->writes_lock)) {
    perror("asyncio_write_page/pthread_mutex_unlock");
    exit(1);
  }

#ifdef FILE_USAGE_STATISTICS
  number_of_writes++;
#endif

  return 0;
}

int asyncio_read_page (unsigned long file_number, unsigned long page_number,
		       ptr_t ptr, unsigned long number_of_bytes)
{
  aio_t *aio, *new_aio;
  int err;
  file_t *f = &file[file_number];

  /* Malloc, or preferably pop from the `list_of_free_aios', a new
     aio to queue a new pending read for this file. */
  if (pthread_mutex_lock(&free_aios_lock)) {
    perror("asyncio_read_page/pthread_mutex_lock");
    exit(1);
  }
  if (list_of_free_aios == NULL) {
    new_aio = malloc(sizeof(aio_t));
    if (new_aio == NULL) {
      perror("asyncio_read_page/malloc");
      exit(1);
    }
  } else {
    new_aio = list_of_free_aios;
    list_of_free_aios = new_aio->next;
  }
  if (pthread_mutex_unlock(&free_aios_lock)) {
    perror("asyncio_read_page/pthread_mutex_unlock");
    exit(1);
  }
  new_aio->next = NULL;
  new_aio->page_number = page_number;
  new_aio->data = ptr;
  new_aio->number_of_bytes = number_of_bytes;

  /* Add this to the end of the pending reads queue and signal
     the reader thread that at least one read is waiting. */
  if (pthread_mutex_lock(&f->reads_lock)) {
    perror("asyncio_read_page/pthread_mutex_lock");
    exit(1);
  }
  if (f->head_of_pending_reads == NULL) {
    f->head_of_pending_reads =
      f->tail_of_pending_reads = new_aio;
  } else {
    f->tail_of_pending_reads->next = new_aio;
    f->tail_of_pending_reads = new_aio;
  }
  f->number_of_pending_reads++;
  if (pthread_cond_signal(&f->read_is_waiting)) {
    perror("asyncio_read_page/pthread_cond_signal");
    exit(1);
  }
  if (pthread_io_is_verbose) {
    fprintf(stderr, "read request: fn = %d dpn = %d nbytes = %d\n",
	    file_number, page_number, number_of_bytes);
  }
  if (pthread_mutex_unlock(&f->reads_lock)) {
    perror("asyncio_read_page/pthread_mutex_unlock");
    exit(1);
  }

  return 0; /* No error. */
}

/* Asynchronous IO-server loop for writes. */
void *writer_thread (void *arg)
{
  file_t *f;
  aio_t *aio;
#ifdef FILE_LOAD_BALANCING
  long start_usec, start_sec, usec_taken;
  struct timeval tv;
#endif

  f = arg;

  while (1) {
    /* First acquire the pending writes lock, and if the queue is empty,
       wait for at least one pending write to be queued. */
    if (pthread_mutex_lock(&f->writes_lock)) {
      perror("writer_thread/pthread_mutex_lock");
      exit(1);
    }
    if (f->head_of_pending_writes == NULL) {
      if (pthread_cond_wait(&f->write_is_waiting, &f->writes_lock)) {
	perror("writer_thread/pthread_cond_wait");
	exit(1);
      }
    }

    assert(f->head_of_pending_writes != NULL);
    assert(f->number_of_pending_writes > 0);

    /* Now we have the queue to ourselves, pop one request from the head
       and release the lock. */
    aio = f->head_of_pending_writes;
    f->head_of_pending_writes = aio->next;
    /* The emptiness of this queue is checked by comparing head to NULL,
       so no extra work is required here, even if this was the last member
       of the queue (in which case aio->next == NULL). */
    /* Release the queue lock now so that further requests can arrive
       while we're still writing this one. */
    if (pthread_mutex_unlock(&f->writes_lock)) {
      perror("writer_thread/pthread_mutex_unlock");
      exit(1);
    }
    
    if (pthread_io_is_verbose) {
      fprintf(stderr, "write beginning: fn = %d, dpn = %d, nbytes = %d\n", 
	      f - file, aio->page_number, aio->number_of_bytes);
    }

#ifdef FILE_LOAD_BALANCING
    gettimeofday(&tv, NULL);
    start_sec = tv.tv_sec;
    start_usec = tv.tv_usec;
#endif
    
    if (lseek(f->fd, disk_skip_nbytes + aio->page_number * PAGE_SIZE,
	      SEEK_SET) == -1) {
      perror("writer_thread/lseek");
      exit(1);
    }
    if (write(f->fd, (void *) aio->data, aio->number_of_bytes)
	!= (signed long) aio->number_of_bytes) {
      perror("writer_thread/write");
      exit(1);
    }
#ifdef FILE_LOAD_BALANCING
    gettimeofday(&tv, NULL);
    usec_taken = (tv.tv_sec - start_sec) * 1000000
      + tv.tv_usec - start_usec;
    assert(usec_taken >= 0);
    f->load -= f->load_history[f->load_cursor];
    f->load += usec_taken;
    f->load_history[f->load_cursor] = usec_taken;
    f->load_cursor = (f->load_cursor + 1) % FILE_LOAD_HISTORY_SIZE;
#endif
#ifdef FILE_USAGE_STATISTICS
    f->number_of_writes++;
    if (file_usage_is_displayed)
      fprintf(stderr, "file %d: %d writes (%f%%) out of %d total.\n",
	      f->number,
	      f->number_of_writes,
	      100 * (double) f->number_of_writes / (double) number_of_writes,
	      number_of_writes);
#endif

    /* Add the processed aio to free list, lock list for adding. */
    if (pthread_mutex_lock(&free_aios_lock)) {
      perror("writer_thread/pthread_mutex_lock");
      exit(1);
    }
    aio->next = list_of_free_aios;
    list_of_free_aios = aio;
    if (pthread_mutex_unlock(&free_aios_lock)) {
      perror("writer_thread/pthread_mutex_unlock");
      exit(1);
    }
    
    /* XXX we could use different lock for queuing the requests and
       signaling their completion. */
    if (pthread_mutex_lock(&f->writes_lock)) {
      perror("writer_thread/pthread_mutex_lock");
      exit(1);
    }
    /* This is probably atomic on every possible system there is,
       the locking here is to guarantee that the conditional signal
       is received properly. */
    f->number_of_pending_writes--;
    if (pthread_cond_signal(&f->write_is_finished)) {
      perror("writer_thread_pthread_cond_signal");
      exit(1);
    }
    if (pthread_mutex_unlock(&f->writes_lock)) {
      perror("writer_thread/pthread_mutex_unlock");
      exit(1);
    }
  }
}

/* Asynchronous IO-server loop for reads. */
/* This is symmetric to `writer_thread' above, see it for further
   documentation. */
void *reader_thread (void *arg)
{
  file_t *f;
  aio_t *aio;

  f = arg;

  while (1) {
    if (pthread_mutex_lock(&f->reads_lock)) {
      perror("reader_thread/pthread_mutex_lock");
      exit(1);
    }
    if (f->head_of_pending_reads == NULL) {
      if (pthread_cond_wait(&f->read_is_waiting, &f->reads_lock)) {
	perror("reader_thread/pthread_cond_wait");
	exit(1);
      }
    }

    assert(f->head_of_pending_reads != NULL);
    assert(f->number_of_pending_reads > 0);

    aio = f->head_of_pending_reads;
    f->head_of_pending_reads = aio->next;

    if (pthread_mutex_unlock(&f->reads_lock)) {
      perror("reader_thread/pthread_mutex_unlock");
      exit(1);
    }

    if (pthread_io_is_verbose) {
      fprintf(stderr, "read beginning: fn = %d, dpn = %d, nbytes = %d\n", 
	      f - file, aio->page_number, aio->number_of_bytes);
    }

    if (lseek(f->fd, disk_skip_nbytes + aio->page_number * PAGE_SIZE,
	      SEEK_SET) == -1) {
      perror("writer_thread/lseek");
      exit(1);
    }
    if (read(f->fd, (void *) aio->data, aio->number_of_bytes)
	!= (signed long) aio->number_of_bytes) {
      perror("reader_thread/read");
      exit(1);
    }

    if (pthread_mutex_lock(&free_aios_lock)) {
      perror("reader_thread/pthread_mutex_lock");
      exit(1);
    }
    aio->next = list_of_free_aios;
    list_of_free_aios = aio;
    if (pthread_mutex_unlock(&free_aios_lock)) {
      perror("reader_thread/pthread_mutex_unlock");
      exit(1);
    }

    if (pthread_mutex_lock(&f->reads_lock)) {
      perror("reader_thread/pthread_mutex_lock");
      exit(1);
    }
    f->number_of_pending_reads--;
    if (pthread_cond_signal(&f->read_is_finished)) {
      perror("reader_thread/pthread_cond_signal");
      exit(1);
    }
    if (pthread_mutex_unlock(&f->reads_lock)) {
      perror("reader_thread/pthread_mutex_unlock");
      exit(1);
    }
  }
}

void asyncio_drain_pending_writes (void)
{
  unsigned long i, j;
  file_t *f;
  aio_t *aio;
  int sval;
  int err;

  for (i = 0; i < number_of_files; i++) {
    f = &file[i];
    if (pthread_mutex_lock(&f->writes_lock)) {
      perror("asyncio_drain_pending_writes/pthread_mutex_lock");
      exit(1);
    }
    while (f->number_of_pending_writes > 0) {
      if (pthread_cond_wait(&f->write_is_finished, &f->writes_lock)) {
	perror("asyncio_drain_pending_writes/pthread_cond_wait");
	exit(1);
      }
    }
    if (pthread_mutex_unlock(&f->writes_lock)) {
      perror("asyncio_drain_pending_writes/pthread_mutex_unlock");
      exit(1);
    }
#if 0
    if (fsync(f->fd) == -1) {
      perror("asyncio_drain_pending_writes/fsync");
      exit(1);
    }
#endif
  }
}

void asyncio_drain_pending_reads (void)
{
  unsigned long i, j;
  file_t *f;
  aio_t *aio;
  int sval;

  for (i = 0; i < number_of_files; i++) {
    f = &file[i];
    if (pthread_mutex_lock(&f->reads_lock)) {
      perror("asyncio_drain_pending_reads/pthread_mutex_lock");
      exit(1);
    }
    while (f->number_of_pending_reads > 0) {
      if (pthread_cond_wait(&f->read_is_finished, &f->reads_lock)) {
	perror("asyncio_drain_pending_reads/pthread_cond_wait");
	exit(1);
      }
    }
    if (pthread_mutex_unlock(&f->reads_lock)) {
      perror("asyncio_drain_pending_reads/pthread_mutex_unlock");
      exit(1);
    }
#if 0
    if (fsync(f->fd) == -1) {
      perror("asyncio_drain_pending_reads/fsync");
      exit(1);
    }
#endif
  }
}

