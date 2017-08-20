/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Antti-Pekka Liedes <apl@cs.hut.fi>
 */

/* Definitions for the file IO interface.
 */

#ifndef INCL_IO
#define INCL_IO 1

#include "includes.h"
#include "params.h"


/* Current implementation requires `DISK_BLOCK_SIZE'fuls of writes to
   be atomic, i.e. either to succeed completely or to never occur.
   We also assume we can write items of any size multiple of this. */
#define DISK_BLOCK_SIZE 512


/* A page location on disk(s). */
typedef word_t disk_page_number_t;

/* An invalid page location on disk, one that can never occur. */
#define INVALID_DISK_PAGE_NUMBER  (~ (disk_page_number_t) 0)


/* Create a file that will host the database, but contains no used
   pages. */
void io_create_file(void);

/* Alternatively, instead of creating a new database, open an old
   one. */
void io_open_file(void);

/* Close the database file.  This is here mostly for symmetry; nothing
   should break even if the file were never properly closed. */
void io_close_file(void);


/* Write to disk the given memory area, at most PAGE_SIZE in size.
   The writing may happen asynchronously, but the disk page number is
   returned immediately.  The caller must ensure that the given page
   will not be changed until the next `io_write_root' has returned.  */
disk_page_number_t io_write_page(ptr_t ptr, unsigned long number_of_bytes);

/* Normally `io_write_page' may assume the data behind the `ptr' given
   to it does not change.  Calling this function allows the callee to
   change any written page again.  `io_write_root' performs the same
   task implicitely, but with the side effect of also writing the root
   block. */
void io_allow_page_changes(void);

/* Write atomically the root block starting from `root' and extending
   for `number_of_bytes' to the disk.  All pending page are written
   synchronously before `root' is written.  The root block is written
   to and read from a predetermined position in the file. */
/* `root' is now directly from root.h and not passed as argument anymore. */
void io_write_root(void);


/* Read the given number of bytes to the given place from the given
   disk page. */
void io_read_page(ptr_t ptr,
		  unsigned long number_of_bytes,
		  disk_page_number_t disk_page_number);

/* Start reading a page from a disk asynchronously.  This is not guaranteed
   to finish before `io_read_page_wait' is called. */
void io_read_page_start(ptr_t ptr,
			unsigned long number_of_bytes,
			disk_page_number_t disk_page_number);

/* Waits for all pending reads from all files to complete. */
void io_read_page_wait(void);

/* Read the root of the database. */
void io_read_root(void);


/* The IO system defines itself the disk page allocation policy, but
   the rest of shades defines the disk page freeing policy.  The disk
   page allocation status can be either "allocated", "free", or
   "unknown".  After `io_create_file', all disk pages are "free";
   after `io_open_file', all disk pages are "unknown".  During
   recovery, "unknown" statuses will be resolved to either "free" or
   "unused" with the routines below.  No disk page should be read
   before it is declared "allocated".  During normal operation,
   `io_write_page' allocates "free" disk pages into "allocated", and
   usually after successful group commits, Shades "free"s a handful of
   "allocated" disk pages. */

/* This is used by the embedding system to declare the IO and disk
   space manager that the given disk page number is in use. */
void io_declare_disk_page_allocated(disk_page_number_t disk_page_number);

/* This is used by the embedding system to declare the IO and space
   manager that the given disk page number is no longer in use. */
void io_free_disk_page(disk_page_number_t disk_page_number);

/* This might be used in the end of recovery. */
void io_declare_unallocated_pages_free(void);

/* The IO system maintains these counters for us. */
unsigned long io_number_of_free_disk_pages(void);


/* Should be called after reading parameters, but before
   `io_create_file' and `io_open_file'.  Returns non-zero on
   failure. */
int io_init(void);


#endif /* INCL_IO */
