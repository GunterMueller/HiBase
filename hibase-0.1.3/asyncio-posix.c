/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Antti-Pekka Liedes <apl@cs.hut.fi>
 */

/* Asynchronous supplementary IO routines.
 */

static char *rev_id = "$Id: asyncio-posix.c,v 1.5 1997/09/22 00:37:36 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


#include "includes.h"
#include "params.h"
#include "io.h"
#include "cookies.h"
#include "bitops.h"
#include "root.h"


/* Asynchronous IO with POSIX.4 aio_write.  */
#include <aio.h>
#include <signal.h>

#if defined(__sun__) && defined(__sparc__) && defined(__svr4__)
#define HAVE_SOLARIS  1
/* On Solaris, POSIX.4-compliant aio library functions do exist, but
   they merely return an error saying that they are not implemented.
   Because of this brain damage, we use Solaris's own aio calls. */
#include <sys/asynch.h>
#endif

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


/* This is used to list all pending aios. */

typedef enum {
  AIO_READ, AIO_WRITE
} asyncio_type_t;

typedef struct asyncio_t {
#ifdef HAVE_SOLARIS
  aio_result_t aio_result;
#else
  aiocb_t aiocb;
#endif
  asyncio_type_t type;
  struct asyncio_t *next;
} asyncio_t;

static asyncio_t *list_of_free_aios = NULL, *list_of_pending_aios = NULL;

/* Each file is associated with a filedescriptor, the descriptors
   are recorded into this table. */
static int *filedesc = NULL;

/* Number of files in use. */
static unsigned long number_of_files = 0;

/* Initialize the asynchronous IO subsystem, should always be called from
   `io_init' and from nowhere else. */
void asyncio_init (unsigned long num_of_files)
{
  number_of_files = num_of_files;
  filedesc = (int *) malloc(num_of_files * sizeof(int));
  if (filedesc == NULL) {
    fprintf(stderr, "Failed to allocate filedescriptor table for Async IO.\n");
    exit(1);
  }
}

/* These two are called when files are created and opened, respectively.
   Here we duplicate the given descriptors into our own fd table,
   duplication is needed to ensure that file cursor are not messed up
   by a badly timed lseek(). */
void asyncio_create_file (unsigned long file_number, int fd)
{
  filedesc[file_number] = dup(fd);
  if (filedesc[file_number] == -1) {
    perror("asyncio_create_file/dup");
    exit(1);
  }
}

void asyncio_open_file (unsigned long file_number, int fd)
{
  filedesc[file_number] = dup(fd);
  if (filedesc[file_number] == -1) {
    perror("asyncio_open_file/dup");
    exit(1);
  }
}

/* This is called whenever a database file is closed. */
/* XXX what if there are pending operations for this file? */
void asyncio_close_file (unsigned long file_number, int fd)
{
  close(filedesc[file_number]);
}

/* This is called whenever the IO system writes a page.  With asynchronous
   IO, we set up and launch an asynchronous write and return immediatly.
   Returns -1 in case of an error (the caller should use synchronous
   write in this case), 0 on success. */
int asyncio_write_page(unsigned long file_number, unsigned long page_number,
		       ptr_t ptr, unsigned long number_of_bytes)
{
  asyncio_t *aio;
  int err;
  
  /* Malloc, or preferably pop from the `list_of_free_aios', a new
     aio, issue an aiowrite, and push it on the list of
     `list_of_pending_aios'. */
  if (list_of_free_aios == NULL) {
    aio = malloc(sizeof(asyncio_t));
    if (aio == NULL) {
      fprintf(stderr, "asyncio_write_page: malloc failed for aio.\n");
      exit(1);
    }
  } else {
    aio = list_of_free_aios;
    list_of_free_aios = aio->next;
  }
  aio->next = list_of_pending_aios;
  list_of_pending_aios = aio;
  aio->type = AIO_WRITE;
  
  /* Issue to async write.  Use the normal write in case of trouble. */
#ifdef HAVE_SOLARIS
  aio->aio_result.aio_return = AIO_INPROGRESS;
  errno = 0;
  err = aiowrite(filedesc[file_number], (char *) ptr, number_of_bytes,
		 disk_skip_nbytes + page_number * PAGE_SIZE,
		 SEEK_SET, (aio_result_t *) aio);
  if (err) {
    if (errno == EAGAIN)
      return -1;
    else {
      perror("asyncio_write_page/aiowrite");
      exit(1);
    }
  }
#else /* !HAVE_SOLARIS */
  /* AIX introduces some further braindeadism, it has a different API for
     POSIX.4 aio_* calls.  The silly "#ifdef AIX"s are here because of this. */
#ifndef AIX
  aio->aiocb.aio_fildes = filedesc[file_number];
#endif
  aio->aiocb.aio_buf = ptr;
  aio->aiocb.aio_nbytes = number_of_bytes;
  aio->aiocb.aio_offset =
    disk_skip_nbytes + page_number * PAGE_SIZE;
  aio->aiocb.aio_reqprio = 0;
#ifdef SIGEV_NONE
  aio->aiocb.aio_sigevent.sigev_notify = SIGEV_NONE;
#endif
  errno = 0;
#ifdef AIX
  err = aio_write(filedesc[file_number], &aio->aiocb);
#else
  err = aio_write(&aio->aiocb);
#endif
  if (err) {
    if (errno == EAGAIN)
      return -1;
    else {
      perror("asyncio_write_page/aio_write");
      exit(1);
    }
  }
#endif /* !HAVE_SOLARIS */

  return 0; /* No error. */
}

/* Start reading the specified page to the given memory area, called from
   `io_read_page_start' in io.c */
int asyncio_read_page(unsigned long file_number, unsigned long page_number,
		      ptr_t ptr, unsigned long number_of_bytes)
{
  asyncio_t *aio;
  int err;
  
  /* Malloc, or preferably pop from the `list_of_free_aios', a new
     aio, issue an aiowrite, and push it on the list of
     `list_of_pending_aios'. */
  if (list_of_free_aios == NULL) {
    aio = malloc(sizeof(asyncio_t));
    if (aio == NULL) {
      fprintf(stderr, "asyncio_write_page: malloc failed for aio.\n");
      exit(1);
    }
  } else {
    aio = list_of_free_aios;
    list_of_free_aios = aio->next;
  }
  aio->next = list_of_pending_aios;
  list_of_pending_aios = aio;
  aio->type = AIO_READ;
  
  /* Issue to async write.  Use the normal write in case of trouble. */
#ifdef HAVE_SOLARIS
  aio->aio_result.aio_return = AIO_INPROGRESS;
  errno = 0;
  err = aioread(filedesc[file_number], (char *) ptr, number_of_bytes,
		disk_skip_nbytes + page_number * PAGE_SIZE,
		SEEK_SET, (aio_result_t *) aio);
  if (err) {
    if (errno == EAGAIN)
      return -1;
    else {
      perror("asyncio_read_page/aioread");
      exit(1);
    }
  }
#else /* !HAVE_SOLARIS */
  /* AIX introduces some further braindeadism, it has a different API for
     POSIX.4 aio_* calls.  The silly "#ifdef AIX"s are here because of this. */
#ifndef AIX
  aio->aiocb.aio_fildes = filedesc[file_number];
#endif
  aio->aiocb.aio_buf = ptr;
  aio->aiocb.aio_nbytes = number_of_bytes;
  aio->aiocb.aio_offset =
    disk_skip_nbytes + page_number * PAGE_SIZE;
  aio->aiocb.aio_reqprio = 0;
#ifdef SIGEV_NONE
  aio->aiocb.aio_sigevent.sigev_notify = SIGEV_NONE;
#endif
  errno = 0;
#ifdef AIX
  err = aio_read(filedesc[file_number], &aio->aiocb);
#else
  err = aio_read(&aio->aiocb);
#endif
  if (err) {
    if (errno == EAGAIN)
      return -1;
    else {
      perror("asyncio_read_page/aio_read");
      exit(1);
    }
  }
#endif /* !HAVE_SOLARIS */

  return 0; /* No error. */
}

/* Drains all pending writes, suspends the process until all asynchronous
   writes are on the disk. */
void asyncio_drain_pending_writes (void)
{
  asyncio_t *aio;
  unsigned long i;

  /* Drain the pending async writes. */
  for (aio = list_of_pending_aios; aio != NULL; aio = aio->next) {
    if (aio->type == AIO_WRITE) {
#ifdef HAVE_SOLARIS
      if (aiowait(NULL) == (aio_result_t *) -1) {
	perror("asyncio_drain_pending_writes/aiowait");
	exit(1);
      }
#else
      aiocb_t *aiocbpp[1];
      aiocbpp[0] = &aio->aiocb;
#ifdef AIX
      if (aio_suspend(1, aiocbpp)) {
#else
      if (aio_suspend(aiocbpp, 1, NULL)) {
#endif
        perror("asyncio_drain_pending_writes/aio_suspend");
        exit(1);
      }
      if (aio_return(&aio->aiocb) == -1) {
        perror("asyncio_drain_pending_writes/aio_return");
        exit(1);
      }
#endif
    }
  }
  /* Move the contents of `list_of_pending_aios' to
     `list_of_free_aios'. */
  while (list_of_pending_aios != NULL) {
    aio = list_of_pending_aios;
    list_of_pending_aios = aio->next;
    aio->next = list_of_free_aios;
    list_of_free_aios = aio;
  }

  /* XXX this might be at least partially redundant. */
#ifdef USE_FSYNC
  for (i = 0; i < number_of_files; i++) {
    fsync(filedesc[i]);
  }
#endif
}

/* Drains all pending reads, suspends the process until all asynchronous
   reads are in the memory. */
void asyncio_drain_pending_reads (void)
{
  asyncio_t *aio;
  unsigned long i;

  /* Drain the pending async writes. */
  for (aio = list_of_pending_aios; aio != NULL; aio = aio->next) {
    if (aio->type == AIO_READ) {
#ifdef HAVE_SOLARIS
      if (aiowait(NULL) == (aio_result_t *) -1) {
	perror("io_write_root/aiowait");
	exit(1);
      }
#else
      aiocb_t *aiocbpp[1];
      aiocbpp[0] = &aio->aiocb;
#ifdef AIX
      if (aio_suspend(1, aiocbpp)) {
#else
      if (aio_suspend(aiocbpp, 1, NULL)) {
#endif
        perror("asyncio_drain_pending_reads/aio_suspend");
        exit(1);
      }
      if (aio_return(&aio->aiocb) == -1) {
        perror("asyncio_drain_pending_reads/aio_return");
        exit(1);
      }
#endif
    }
  }
  /* Move the contents of `list_of_pending_aios' to
     `list_of_free_aios'. */
  while (list_of_pending_aios != NULL) {
    aio = list_of_pending_aios;
    list_of_pending_aios = aio->next;
    aio->next = list_of_free_aios;
    list_of_free_aios = aio;
  }

  /* XXX this might be at least partially redundant. */
#ifdef USE_FSYNC
  for (i = 0; i < number_of_files; i++) {
    fsync(filedesc[i]);
  }
#endif
}


long asyncio_get_file_load(unsigned long file_number)
{
  return 0;
}

void asyncio_reduce_file_load (unsigned long file_number)
{
}
