/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Antti-Pekka Liedes <apl@cs.hut.fi>
 */

/* Definitions for the file asynchronous IO interface.
 */

#ifndef INCL_ASYNCIO
#define INCL_ASYNCIO 1

/* These are the asynchronous IO subsystem functions to be called
   from the IO system and NOWHERE ELSE! */

/* This must be called before anything else in this module.  Used to
   establish the internal data structures. */
void asyncio_init (unsigned long num_of_files);

/* These two must be called right after opening a new or existing
   database file.  Used to initialize the per-file internal data
   structures and possibly to launch file-specific threads. */
void asyncio_create_file (unsigned long file_number, int fd);

void asyncio_open_file (unsigned long file_number, int fd);

/* Must be called whenever a database file is closed. */
void asyncio_close_file (unsigned long file_number, int fd);

/* Give the current load of a file. */
long asyncio_get_file_load (unsigned long file_number);

/* Artificially reduce the load of a file. */
void asyncio_reduce_file_load (unsigned long file_number);

/* These are used to asynchronous read/write a specific page from
   specific file number.  The decisions of file/page number must be
   made before calling. */
int asyncio_write_page (unsigned long file_number, unsigned long page_number,
			ptr_t ptr, unsigned long number_of_bytes);

int asyncio_read_page (unsigned long file_number, unsigned long page_number,
		       ptr_t ptr, unsigned long number_of_bytes);

/* These are used to flush all pending reads or writes.  The function
   does not return before all asynchronous operations of the type
   are completely finished. */
void asyncio_drain_pending_writes (void);

void asyncio_drain_pending_reads (void);

#endif /* INCL_ASYNCIO */
