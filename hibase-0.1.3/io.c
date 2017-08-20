/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Antti-Pekka Liedes <apl@cs.hut.fi>
 */

/* IO routines.
 */

static char *rev_id = "$Id: io.c,v 1.60 1998/01/28 11:33:16 apl Exp $";
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

#ifdef ASYNC_IO
#include "asyncio.h"
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


#ifndef OPTIMIZED_ROOT_LOCATION
/* If OPTIMIZED_ROOT_LOCATION is defined, the disk to write root on
   is selected with a round robin algorithm, however, defining this
   will cause root to be written on the specific disk.  If this is
   defined and OPTIMIZED_ROOT_LOCATION is used, page to write the root
   on is selected with round robin on this disk, if OPTIMIZED_ROOT_LOCATION
   is not used, root is written to the end of this disk. */
#define FIXED_ROOT_DISK  0
#endif

#ifdef ASYNC_IO
/* If defined, ask for disk loads from asyncio and determine next disk
   to write on by those, otherwise use round robin.  Sensible only when
   asynchronous IO is enabled. */
#define DISK_LOAD_BALANCING
#endif

/* Filenames are separated by this character. */
#define FILENAME_SEPARATOR ','

/* Round write requests upwards to a multiple of this number.  This is
   for block special devices, e.g. raw disks. */
#define WRITE_BOUNDARY  DISK_BLOCK_SIZE

/* Number of words in a disk block. */
#define WORDS_PER_DISK_BLOCK  (DISK_BLOCK_SIZE / sizeof(word_t))

/* The 32-bit disk page number is divided to N bits for indexing
   pages (of size `PAGE_SIZE')  inside the file, and 32-N bits for
   indexing the file. */
#define PAGE_INDEX_BITS  27


/* The IO system defines itself the disk page allocation policy, but
   Shades defines the disk page freeing policy.  The disk page
   allocation status can be either "allocated", "free", "unknown", or
   "root", if OPTIMIZED_ROOT_LOCATION is #defined.
   After `io_create_file', all disk pages are "free"; after
   `io_open_file', all disk pages are "unknown".  During recovery,
   "unknown" statuses will be resolved to either "free" or "unused"
   with the routines below.  No disk page should be read before it is
   declared "allocated".  During normal operation, `io_write_page'
   allocates "free" disk pages into "allocated", and usually after
   successful group commits, Shades "free"s a handful of "allocated"
   disk pages.  If OPTIMIZED_ROOT_LOCATION is #defined, page status
   "root" indicates that this page contains the latest root block.
   This type of page status shouldn't be of any concern outside of
   IO subsystem. */

typedef enum {
  ALLOCATED, ROOT, FREE, UNKNOWN
} disk_page_status_t;


/* Tools for manipulating fields of the disk page number. */
#define FILE_INDEX_MASK  (~0L << PAGE_INDEX_BITS)
#define PAGE_INDEX_MASK  (~FILE_INDEX_MASK)

#define GET_PAGE_NUMBER(x)  ((x) & PAGE_INDEX_MASK)
#define GET_FILE_NUMBER(x)  ((x) >> PAGE_INDEX_BITS)
#define MAKE_DISK_PAGE_NUMBER(file_number, page_number)	\
  (((file_number) << PAGE_INDEX_BITS) | (page_number))


/* A struct for each file (disk) we use.  Multiple disks are
   particularly useful if we have asynchrony so that we can write to
   several disks in parallel, a.k.a. disk striping. */
typedef struct {
  char *filename;		/* (Possibly raw device name.) */
  int fd;
  unsigned long number_of_pages;
  unsigned long number_of_free_pages;
  /* Array that stores information known about the allocation status
     of disk pages.  Indexed with the disk page number. */
  disk_page_status_t *status;
  unsigned long cursor;
} file_t;


/* Array of file information structs. */
static file_t *file;

/* Number of files in use. */
static unsigned long number_of_files = 0;

/* Current file to write to. */
static unsigned long file_cursor = 0;

/* Number of free disk pages totally in all files. */
static long unsigned number_of_free_disk_pages = 0;

#ifdef OPTIMIZED_ROOT_LOCATION
/* Position of the previous root block among the disk pages. */
static long prev_root_file = -1;
static long prev_root_page = -1;
#endif


/* Initialize the IO subsystem, create file structures. */
int io_init(void)
{
  char *filenames, *filesizes, *p, *q, sep[2];
  unsigned long i, size, prev_size;

  filenames = strdup(disk_filename);
  if (filenames == NULL) {
    perror("io_init/strdup");
    exit(1);
  }

  filesizes = strdup(disk_filesize);
  if (filesizes == NULL) {
    perror("io_init/strdup");
    exit(1);
  }

  /* Count the number of backup files in use. */
  number_of_files = 1;
  for (p = filenames; *p != '\0'; p++)
    if (*p == FILENAME_SEPARATOR)
      number_of_files++;
  
  file = (file_t *) malloc(number_of_files * sizeof(file_t));
  if (file == NULL) {
    perror("io_init/malloc");
    exit(1);
  }

  prev_size = 0;
  /* strtok() requires separator as string, so we make one. */
  sprintf(sep, "%c", FILENAME_SEPARATOR);
  /* Get the first file size here. */
  q = strtok(filesizes, sep);
  for (i = 0, p = filenames; i < number_of_files; i++) {
    /* Parse the individual file names to the corresponding
       `file_t.filename'. */
    file[i].filename = p;
    p = strchr(p, FILENAME_SEPARATOR);
    if (p != NULL) {
      *p = '\0';
      p++;
    }
    /* Parse the size of this file from the `disk_filesize' argument.
       If no size is given or it is 0, use the previous size. */
    if ((q == NULL) || (size = atoi(q)) == 0) {
      size = prev_size;
    } else {
      if (q[strlen(q) - 1] == 'M') {
	size *= 1024 * 1024;
      } else if (q[strlen(q) - 1] == 'k') {
	size *= 1024;
      }
      prev_size = size;
    }
    /* This division cuts, so the file may be up to (`PAGE_SIZE' - 1)
       bytes smaller than what was given, but never bigger.  (Note also
       `disk_skip_nbytes'). */
#ifndef OPTIMIZED_ROOT_LOCATION
    /* Root is placed to the end of the first file, so leave one
       page out of the first file. */
    if (i == FIXED_ROOT_DISK)
      file[i].number_of_pages = (size / PAGE_SIZE) - 1;
    else
#endif
      file[i].number_of_pages = size / PAGE_SIZE;
    file[i].number_of_free_pages = file[i].number_of_pages;
    q = strtok(NULL, sep);
    /* Initialize other fields of this file. */
    file[i].status =
      malloc(file[i].number_of_pages * sizeof(disk_page_status_t));
    if (file[i].status == NULL) {
      perror("io_init/malloc");
      exit(1);
    }
    file[i].cursor = 0;
    file[i].fd = -1;
  }
  file_cursor = 0;

#ifdef ASYNC_IO
  asyncio_init(number_of_files);
#endif

#ifdef OPTIMIZED_ROOT_LOCATION
  prev_root_file = -1;
  prev_root_page = -1;
#endif
  return 0;
}


/* Create files that will host the database, but contains no used
   pages. */
void io_create_file(void)
{
  unsigned long i, j;
  int fd;

#ifdef OPTIMIZED_ROOT_LOCATION
  /* Write this to the beginning of each page on disk. */
  word_t wipe[WORDS_PER_DISK_BLOCK];
  wipe[0] = UNUSED_PAGE_COOKIE;
  if (be_verbose)
    fprintf(stderr, "[Clearing disk files...");
#endif

  for (j = 0; j < number_of_files; j++) {
    /* Open the file for each file.  This is write-only, we do
       not recover from these files. */
    fd = file[j].fd = open(file[j].filename, O_CREAT | O_WRONLY | O_SYNC_FLAG);
    if (fd == -1) {
      perror("io_create_file/open");
    }
    if (disk_file_permissions >= 0 && fchmod(fd, disk_file_permissions) == -1)
      perror("io_create_file/fchmod");
    if (disk_file_group >= 0 && fchown(fd, -1, disk_file_group) == -1)
      perror("io_create_file/fchown");

    /* Free all pages and initialize some internal data fields. */
    for (i = 0; i < file[j].number_of_pages; i++)
      file[j].status[i] = FREE;
    file[j].cursor = 0;
    file[j].number_of_free_pages = file[j].number_of_pages;
    number_of_free_disk_pages += file[j].number_of_pages;

#ifdef OPTIMIZED_ROOT_LOCATION
    /* Wipe out possible old magic cookies on disk because the new
       root allocation on disk depends on them to be correct. */
    for (i = 0; i < file[j].number_of_pages; i++) {
      if (lseek(file[j].fd,
		disk_skip_nbytes + i * PAGE_SIZE,
		SEEK_SET) == -1) {
	perror("io_create_file/seek");
	exit(1);
      }
      if (write(file[j].fd, (void *) wipe, DISK_BLOCK_SIZE)
	  != DISK_BLOCK_SIZE) {
	perror("io_create_file/write");
	exit(1);
      }
    }
#endif

#ifdef ASYNC_IO
    asyncio_create_file(j, fd);
#endif
  }

  file_cursor = 0;

#ifdef OPTIMIZED_ROOT_LOCATION
  if (be_verbose)
    fprintf(stderr, "Done]\n");
  prev_root_file = -1;
  prev_root_page = -1;
#endif
}


/* Open an old database.  The allocation status of the pages will be
   resolved during recovery. */
void io_open_file(void)
{
  unsigned long i, j;
  int fd;

  for (j = 0; j < number_of_files; j++) {
    for (i = 0; i < file[j].number_of_pages; i++)
      file[j].status[i] = UNKNOWN;
    file[j].number_of_free_pages = 0;

    /* Open file for each file for recovery. */
    fd = file[j].fd = open(file[j].filename, O_RDWR | O_SYNC_FLAG);
    if (fd == -1) {
      perror("io_open_file/open");
      exit(1);
    }
    if (disk_file_permissions >= 0 && fchmod(fd, disk_file_permissions) == -1)
      perror("io_open_file/fchmod");
    if (disk_file_group >= 0 && fchown(fd, -1, disk_file_group) == -1)
      perror("io_open_file/fchown");

    file[j].cursor = 0;

#ifdef ASYNC_IO
    asyncio_open_file(j, fd);
#endif
  }

  file_cursor = 0;

#ifdef OPTIMIZED_ROOT_LOCATION
  prev_root_file = -1;
  prev_root_page = -1;
#endif
}


/* Close the database files.  This is here mostly for symmetry; nothing
   should break even if the file were never properly closed. */
void io_close_file(void)
{
  unsigned long i;

  for (i = 0; i < number_of_files; i++) {
    if (close(file[i].fd)) {
      perror("io_close_file/close");
      exit(1);
    }
    file[i].fd = -1;

#ifdef ASYNC_IO
    asyncio_close_file(i, file[i].fd);
#endif
  }
}


/* Write to disk the given memory area, at most PAGE_SIZE in size.
   The corresponding disk page to which the memory page is written is
   selected here.  Writing starts an asynchronous aiowrite process and
   returns the disk page number to the caller immediatly or writes the
   page synchronously and returns only when it is updated on the disk. 

   Uses round robin to select file and disk page, or ask for file load
   from asyncio and use the least loaded file, if DISK_LOAD_BALANCING
   is defined. */

disk_page_number_t io_write_page(ptr_t ptr, unsigned long number_of_bytes)
{
  long files_left, pages_left;
  disk_page_number_t dpn;
  file_t *f;
#ifdef DISK_LOAD_BALANCING
  long load, lowest_load = 0;
  unsigned long fastest_file, free_page;
#endif

  /* Make sure number_of_bytes is at WRITE_BOUNDARY. */
  unsigned long excess_bytes = number_of_bytes % WRITE_BOUNDARY;
  if (excess_bytes != 0)
    number_of_bytes += WRITE_BOUNDARY - excess_bytes;

  assert(ptr[0] == PAGE_MAGIC_COOKIE);
  assert(number_of_bytes <= PAGE_SIZE);

  if (number_of_free_disk_pages == 0) {
    fprintf(stderr, "io_write_page: Disk backupfile full.\n");
    exit(1);
  }

#ifdef DISK_LOAD_BALANCING
  lowest_load = 999999999;
  /* Search for the file to write to using file loads. */
  for (files_left = number_of_files; files_left > 0; files_left--) {
    f = &file[file_cursor];
    /* Make sure there's enough space for this page and root. */
    if (f->number_of_free_pages > 1) {
      load = asyncio_get_file_load(file_cursor);
      if (load < lowest_load) {
	fastest_file = file_cursor;
	lowest_load = load;
      }
    }
    file_cursor = (file_cursor + 1) % number_of_files;
  }
  for (file_cursor = 0; file_cursor < number_of_files; file_cursor++) {
    if (file_cursor != fastest_file)
      asyncio_reduce_file_load(file_cursor);
  }
  file_cursor = fastest_file;
  f = &file[file_cursor];
  /*  fprintf(stderr, "Selected file: %d\n", file_cursor); */
  for (pages_left = f->number_of_pages; pages_left > 0; pages_left--) {
    if (f->status[f->cursor] == FREE) {
      break;
    }
    f->cursor = (f->cursor + 1) % f->number_of_pages;
  }
  /*  fprintf(stderr, "Selected page: %d\n", f->cursor); */
#else
  /* Round robin to search for the disk and page to write to. */
  for (files_left = number_of_files; files_left > 0; files_left--) {
    f = &file[file_cursor];
    if (f->number_of_free_pages > 1) {
      for (pages_left = f->number_of_pages; pages_left > 0; pages_left--) {
	if (f->status[f->cursor] == FREE)
	  goto found_disk_page;
	f->cursor = (f->cursor + 1) % f->number_of_pages;
      }
    }
    file_cursor = (file_cursor + 1) % number_of_files;
  }
  /* This can't happen since we already checked there's enough room
     somewhere in the files. */
  assert(0);

found_disk_page:
#endif
  f->status[f->cursor] = ALLOCATED;
  f->number_of_free_pages--;
  number_of_free_disk_pages--;

#ifdef ASYNC_IO

  if (asyncio_write_page(file_cursor, f->cursor, ptr, number_of_bytes) == 0) {
    goto return_from_write_page;
  }

  /* If `asyncio_write_page' failed, fall through here and write the page
     synchronously. */

#endif /* ASYNC_IO */

  /* Plain write() the data to the disk and return when it's there. */
  if (lseek(f->fd, disk_skip_nbytes + f->cursor * PAGE_SIZE, SEEK_SET) == -1) {
    perror("io_write_page/lseek");
    exit(1);
  }
  if (write(f->fd, (void *) ptr, number_of_bytes)
      != (signed long) number_of_bytes) {
    perror("io_write_page/write");
    exit(1);
  }

return_from_write_page:
  /* Increase file_cursor by one to point to the next file
     so that the next write will go to it (if there's room). */
  dpn = MAKE_DISK_PAGE_NUMBER(file_cursor, f->cursor);
  file_cursor = (file_cursor + 1) % number_of_files;
  f->cursor = (f->cursor + 1) % f->number_of_pages;
  return dpn;
}


/* Normally `io_write_page' may assume the data behind the `ptr' given
   to it does not change.  Calling this function allows the callee to
   change any written page again. */
void io_allow_page_changes(void)
{
  /* We either have to ensure the pages are already on disk, or we
     have to make a temporary private copy of those pages that are not
     on disk.  The former is simpler, so we use it here now. */
#ifdef ASYNC_IO
  asyncio_drain_pending_writes();
#endif
}


/* Write atomically the root block starting from `root' and extending
   for `number_of_bytes' to the disk.  All aios in progress are first
   waited to complete.  If OPTIMIZED_ROOT_LOCATION is #defined, write
   the root block to the next position in the same manner as normal
   data pages.  Otherwise write it to the end of file[FIXED_ROOT_DISK]. */
void io_write_root(void)
{
  long files_left, pages_left;
  unsigned long i;
  file_t *f;

  assert(root[0] == ROOT_MAGIC_COOKIE);

#ifdef ASYNC_IO
  asyncio_drain_pending_writes();
#endif

#ifdef OPTIMIZED_ROOT_LOCATION

  if (number_of_free_disk_pages <= 0) {
    fprintf(stderr, "io_write_root: disk backup file full.\n");
    exit(1);
  }

#ifdef FIXED_ROOT_DISK
  f = &file[FIXED_ROOT_DISK];
  if (f->number_of_free_pages <= 0) {
    fprintf(stderr, "io_write_root: disk backup file full.\n");
    exit(1);
  }
  for (pages_left = f->number_of_pages; pages_left > 0; pages_left--) {
    if (f->status[f->cursor] == FREE)
      goto found_disk_page;
    f->cursor = (f->cursor + 1) % f->number_of_pages;
  }
#else
  /* Search for a free page as we did in `io_write_page'. */
  for (files_left = number_of_files; files_left > 0; files_left--) {
    f = &file[file_cursor];
    for (pages_left = f->number_of_pages; pages_left > 0; pages_left--) {
      if (f->status[f->cursor] == FREE)
	goto found_disk_page;
      f->cursor = (f->cursor + 1) % f->number_of_pages;
    }
    file_cursor = (file_cursor + 1) % number_of_files;
  }
#endif
  assert(0);
found_disk_page:
  f->status[f->cursor] = ROOT;
  f->number_of_free_pages--;
  number_of_free_disk_pages--;

  /* Seek to the point of the root block, and write the root block. */ 
#ifdef USE_FSYNC
  for (i = 0; i < number_of_files; i++)
    fsync(file[i].fd);
#endif
  if (lseek(f->fd, disk_skip_nbytes + f->cursor * PAGE_SIZE, SEEK_SET) == -1) {
    perror("io_write_root/lseek");
    exit(1);
  }
  if (write(f->fd, (void *) root, DISK_BLOCK_SIZE) != DISK_BLOCK_SIZE) {
    perror("io_write_root/write");
    exit(1);
  }
  
  /* This might not be needed if only disk pages are freed suffiently
     late. */
#ifdef USE_FSYNC
  fsync(f->fd);
#endif

  if (prev_root_file != -1 && prev_root_page != -1) {
    /* The old root is now obsolete and may be overwritten. */
    assert(file[prev_root_file].status[prev_root_page] == ROOT);
    file[prev_root_file].status[prev_root_page] = FREE;
    file[prev_root_file].number_of_free_pages++;
    number_of_free_disk_pages++;
  }
  prev_root_file = file_cursor;
  prev_root_page = f->cursor;
  f->cursor = (f->cursor + 1) % f->number_of_pages;
  file_cursor = (file_cursor + 1) % number_of_files;

#else /* !OPTIMIZED_ROOT_LOCATION */

#ifdef USE_FSYNC
  for (i = 0; i < number_of_files; i++)
    if (fsync(file[i].fd) == -1) {
      perror("io_write_root/fsync");
      exit(1);
    }
#endif

  if (lseek(file[FIXED_ROOT_DISK].fd,
	    disk_skip_nbytes
	    + file[FIXED_ROOT_DISK].number_of_pages * PAGE_SIZE,
	    SEEK_SET) == -1) {
    perror("io_write_root/lseek");
    exit(1);
  }
  if (write(file[FIXED_ROOT_DISK].fd,
	    (void *) root, DISK_BLOCK_SIZE) != DISK_BLOCK_SIZE) {
    perror("io_write_root/write");
    exit(1);
  }

  /* This might not be needed if only disk pages are freed
     sufficiently late. */
#ifdef USE_FSYNC
  if (fsync(file[0].fd) == -1) {
    perror("io_write_root/fsync2");
    exit(1);
  }
#endif

#endif /* OPTIMIZED_ROOT_LOCATION */

  if (root_timestamp_is_displayed)
    fprintf(stderr, "Wrote root with timestamp 0x%08lX%08lX\n",
	    GET_ROOT_WORD(time_stamp_hi), GET_ROOT_WORD(time_stamp_lo));
  increment_root_time_stamp();
}


/* Read the given page from the given disk page. */
void io_read_page(ptr_t ptr,
		  unsigned long number_of_bytes,
		  disk_page_number_t disk_page_number)
{
  unsigned long file_number = GET_FILE_NUMBER(disk_page_number);
  unsigned long page_number = GET_PAGE_NUMBER(disk_page_number);
  unsigned long i;

  assert(number_of_bytes <= PAGE_SIZE);
  /* It must have been declared allocated prior to reading it. */
  assert(file[file_number].status[page_number] == ALLOCATED);

  if (lseek(file[file_number].fd,
	    disk_skip_nbytes + page_number * PAGE_SIZE,
	    SEEK_SET) == -1) {
    perror("io_read_page/lseek");
    exit(1);
  }
  if (read(file[file_number].fd, (void *) ptr, number_of_bytes)
      != (signed long) number_of_bytes) {
    perror("io_read_page/read");
    exit(1);
  }

  if (ptr[0] == SWAP_BYTES(PAGE_MAGIC_COOKIE))
    for (i = 0; i * sizeof(word_t) < number_of_bytes; i++)
      ptr[i] = SWAP_BYTES(ptr[i]);

  if (ptr[0] != PAGE_MAGIC_COOKIE) {
    fprintf(stderr, "io_read_page: Incorrect magic cookie 0x%08lX\n", ptr[0]);
    exit(1);
  }
}

/* Start reading a page from a disk asynchronously.  This is not guaranteed
   to finish before `io_read_page_wait' is called. */
void io_read_page_start(ptr_t ptr,
			unsigned long number_of_bytes,
			disk_page_number_t disk_page_number)
{
  unsigned long file_number = GET_FILE_NUMBER(disk_page_number);
  unsigned long page_number = GET_PAGE_NUMBER(disk_page_number);
  unsigned long i;

  assert(number_of_bytes <= PAGE_SIZE);
  /* It must have been declared allocated prior to reading it. */
  assert(file[file_number].status[page_number] == ALLOCATED);

#ifdef ASYNC_IO
  if (asyncio_read_page(file_number, page_number, ptr, number_of_bytes) != 0)
#endif
  {
    if (lseek(file[file_number].fd,
	      disk_skip_nbytes + page_number * PAGE_SIZE,
	      SEEK_SET) == -1) {
      perror("io_read_page/lseek");
      exit(1);
    }
    if (read(file[file_number].fd, (void *) ptr, number_of_bytes)
	!= (signed long) number_of_bytes) {
      perror("io_read_page/read");
      exit(1);
    }
  }
}

/* Waits for all pending reads from all files to complete. */
void io_read_page_wait(void)
{
#ifdef ASYNC_IO
  asyncio_drain_pending_reads();
#endif
}

void io_read_root(void)
{
  unsigned long i, root_page, root_file;

#ifdef OPTIMIZED_ROOT_LOCATION

  /* Apply a binary search on each file to find the newest root block
     in the database. */
  
  disk_page_number_t left_page, right_page, mid_page, first_mid_page;
  unsigned long /* The abbreviation `ts' means "time stamp". */
    left_ts_hi, left_ts_lo, right_ts_hi, right_ts_lo,
    mid_ts_hi, mid_ts_lo, root_ts_hi = 0, root_ts_lo = 0;
  enum { LEFTWARD, RIGHTWARD } first_direction, direction;
  int right_page_is_valid_root = 0;
  file_t *f;
  int uninitialized_file_has_been_warned = 0;

#ifdef FIXED_ROOT_DISK
  f = &file[FIXED_ROOT_DISK];
  {
#else
  for (i = 0; i < number_of_files; i++) {
    f = &file[i];
#endif
    left_page = 0;
    right_page = f->number_of_pages;
    direction = RIGHTWARD;
    right_page_is_valid_root = 0;
    right_ts_hi = 0;
    right_ts_lo = 0;

  search_leftmost_root:
    if (root_search_is_verbose)
      fprintf(stderr, "Searching leftmost root at %d\n", left_page);
    if (lseek(f->fd,
	      disk_skip_nbytes + left_page * PAGE_SIZE,
	      SEEK_SET) == -1) {
      perror("io_read_root/lseek");
      exit(1);
    }
    if (read(f->fd, (void *) root, DISK_BLOCK_SIZE) != DISK_BLOCK_SIZE) {
      perror("io_read_root/read");
      exit(1);
    }
    if (root[0] != ROOT_MAGIC_COOKIE
	&& root[0] != SWAP_BYTES(ROOT_MAGIC_COOKIE)) {
      left_page++;
      if (left_page == f->number_of_pages
	  || root[0] == UNUSED_PAGE_COOKIE
	  || root[0] == SWAP_BYTES(UNUSED_PAGE_COOKIE))
	goto no_root_in_this_file;
      goto search_leftmost_root;
    }
    left_ts_hi = GET_ROOT_WORD(time_stamp_hi);
    left_ts_lo = GET_ROOT_WORD(time_stamp_lo);

  left_or_right_page_changed:
    /* We come here every time left of right disk page number has
       changed */
    if (root_search_is_verbose)
      fprintf(stderr,
	      "Left page:    %ld time stamp: 0x%08lX%08lX\n"
	      "Right page:   %ld time stamp: 0x%08lX%08lX valid: %d\n",
	      left_page, left_ts_hi, left_ts_lo,
	      right_page, right_ts_hi, right_ts_lo, right_page_is_valid_root);
    if (left_page == right_page)
      goto found_newest_root_in_this_file;
    first_direction = direction;
    first_mid_page = mid_page = (right_page + left_page) / 2;

  mid_page_changed:
    /* We jump back here when `mid_page' has changed when scanning for
       the nearest root page to the original `mid_page'. */
    if (root_search_is_verbose)
      fprintf(stderr, "Middle page: %ld\n", mid_page);

    /* In cases where right_page == left_page + 1 we hit the left_page
       with mid_page.  In such cases the search should be terminated as
       succesful. */
    if (mid_page == left_page) {
      goto found_newest_root_in_this_file;
    }

    /* Read in the new root canditate. */
    if (lseek(f->fd,
	      disk_skip_nbytes + mid_page * PAGE_SIZE,
	      SEEK_SET) == -1) {
      perror("io_read_root/lseek");
      exit(1);
    }
    if (read(f->fd, (void *) root, DISK_BLOCK_SIZE) != DISK_BLOCK_SIZE) {
      perror("io_read_root/read");
      exit(1);
    }

    /* Check for the type of page that was read in and proceed
       accordingly. */
    if (root[0] == PAGE_MAGIC_COOKIE 
	|| root[0] == SWAP_BYTES(PAGE_MAGIC_COOKIE)) {
      /* Normal data page, but we're searching for a root page.  Scan
	 in one direction, come back and change direction if left or
	 right page is reached. */
      if (direction == RIGHTWARD) {
	mid_page++;
	if (mid_page == right_page) {
	  if (first_direction != direction)
	    /* Already searched from the other direction.  This means
	       we are between two roots without any other roots in
	       between, so the left or right limiting page of the range
	       must be the neweest root block. */
	    goto found_newest_root_in_this_file;
	  /* We reached the search range limit, start to the other
	     direction from the original middle page position. */
	  mid_page = first_mid_page - 1;
	  direction = LEFTWARD;
	  if (mid_page == left_page)
	    goto found_newest_root_in_this_file;
	}
      } else {
	mid_page--;
	/* See comments above. */
	if (mid_page == left_page) {
	  if (first_direction != direction)
	    goto found_newest_root_in_this_file;
	  mid_page = first_mid_page + 1;
	  direction = RIGHTWARD;
	  if (mid_page == right_page)
	    goto found_newest_root_in_this_file;
	}
      }
      goto mid_page_changed;
    }
    if (GET_ROOT_WORD(magic_cookie) == ROOT_MAGIC_COOKIE
	|| GET_ROOT_WORD(magic_cookie) == SWAP_BYTES(ROOT_MAGIC_COOKIE)) {
      /* A root canditate, store the time stamp and do comparison with
	 the previous one to determine the new search range. */
      mid_ts_hi = GET_ROOT_WORD(time_stamp_hi);
      mid_ts_lo = GET_ROOT_WORD(time_stamp_lo);
      if (time_stamp_is_newer_than(mid_ts_hi, mid_ts_lo,
				   left_ts_hi, left_ts_lo)) {
	left_page = mid_page;
	left_ts_hi = mid_ts_hi;
	left_ts_lo = mid_ts_lo;
      } else {
	right_page = mid_page;
	right_ts_hi = mid_ts_hi;
	right_ts_lo = mid_ts_lo;
	right_page_is_valid_root = 1;
      }
      /* Change the direction to search next time to hopefully get
	 some variance that will improve performance. */
      if (direction == RIGHTWARD)
	direction = LEFTWARD;
      else
	direction = RIGHTWARD;
      goto left_or_right_page_changed;
    }
    if (GET_ROOT_WORD(magic_cookie) == UNUSED_PAGE_COOKIE
	|| GET_ROOT_WORD(magic_cookie) == SWAP_BYTES(UNUSED_PAGE_COOKIE)) {
      /* This disk page has not yet been written, so set this as new
	 right page in order to get into (or at least close to) the
	 range of used pages. */
      right_page = mid_page;
      right_page_is_valid_root = 0;
      goto left_or_right_page_changed;
    }
    if (!uninitialized_file_has_been_warned) {
      fprintf(stderr, 
	      "Warning: The database files were not initialized correctly\n"
	      "  during database creation, or you have extended the disk\n"
	      "  file size since, or the disk file is indeed corrupt\n");
      uninitialized_file_has_been_warned = 1;
    }
    goto mid_page_changed;
    
  found_newest_root_in_this_file:
    /* If the newest root in this file is newer than the newest root
       found so far in previous files, then make this this root the
       newest one (kept in `root_file' and `root_page'.  

       Additionally initialize file cursors to (approximately) newest
       positions, otherwise we may first allocate the rapidly freed
       root pages, which in turn means essentially a linear scan in
       the next recovery (if it is done soon). */
    if (right_page_is_valid_root
	&& time_stamp_is_newer_than(right_ts_hi, right_ts_lo,
				    left_ts_hi, left_ts_lo)) {
      file[i].cursor = right_page;
      if (time_stamp_is_newer_than(right_ts_hi, right_ts_lo,
				   root_ts_hi, root_ts_lo)) {
	root_file = i;
	root_page = right_page;
	root_ts_hi = right_ts_hi;
	root_ts_lo = right_ts_lo;
      }
    } else {
      file[i].cursor = left_page;
      if (time_stamp_is_newer_than(left_ts_hi, left_ts_lo,
				   root_ts_hi, root_ts_lo)) {
	root_file = i;
	root_page = left_page;
	root_ts_hi = left_ts_hi;
	root_ts_lo = left_ts_lo;
      }
    }

  no_root_in_this_file:
    /* Exit loop or search the next file. */
    /* This semicolon is here to keep compilers that want a statement
       after goto label happy. */
    ;
  }

  /* Finally we should have the latest root file and page. */
  prev_root_file = root_file;
  prev_root_page = root_page;
  file[root_file].status[root_page] = ROOT;
  file[root_file].number_of_free_pages--;

#else /* !OPTIMIZED_ROOT_LOCATION */

  root_file = 0;
  root_page = file[0].number_of_pages;

#endif

  if (lseek(file[root_file].fd,
	    disk_skip_nbytes + root_page * PAGE_SIZE,
	    SEEK_SET) == -1) {
    perror("io_read_root/lseek");
    exit(1);
  }
  if (read(file[root_file].fd, (void *) root, DISK_BLOCK_SIZE)
      != DISK_BLOCK_SIZE) {
    perror("io_read_root/read");
    exit(1);
  }

  if (root[0] == SWAP_BYTES(ROOT_MAGIC_COOKIE))
    for (i = 0; i * sizeof(word_t) < DISK_BLOCK_SIZE; i++)
      root[i] = SWAP_BYTES(root[i]);

  if (root[0] != ROOT_MAGIC_COOKIE) {
    fprintf(stderr, "io_read_root: Incorrect magic cookie 0x%08lX\n", root[0]);
    exit(1);
  }

  if (root_timestamp_is_displayed)
    fprintf(stderr, "Read root with timestamp 0x%08lX%08lX\n",
	    GET_ROOT_WORD(time_stamp_hi), GET_ROOT_WORD(time_stamp_lo));

  increment_root_time_stamp();
}


void io_declare_disk_page_allocated(disk_page_number_t disk_page_number)
{
  unsigned long file_number = GET_FILE_NUMBER(disk_page_number);
  unsigned long page_number = GET_PAGE_NUMBER(disk_page_number);

  assert(file[file_number].status[page_number] != FREE);
  file[file_number].status[page_number] = ALLOCATED;
}


void io_free_disk_page(disk_page_number_t disk_page_number)
{
  unsigned long file_number = GET_FILE_NUMBER(disk_page_number);
  unsigned long page_number = GET_PAGE_NUMBER(disk_page_number);

  if (file[file_number].status[page_number] != FREE) {
    file[file_number].status[page_number] = FREE;
    file[file_number].number_of_free_pages++;
    number_of_free_disk_pages++;
  }
}


unsigned long io_number_of_free_disk_pages(void)
{
  return number_of_free_disk_pages;
}


void io_declare_unallocated_pages_free(void)
{
  unsigned long i, j;
  
  for (i = 0; i < number_of_files; i++)
    for (j = 0; j < file[i].number_of_pages; j++)
      if (file[i].status[j] == UNKNOWN) {
	file[i].status[j] = FREE;
	file[i].number_of_free_pages++;
	number_of_free_disk_pages++;
      }
}
