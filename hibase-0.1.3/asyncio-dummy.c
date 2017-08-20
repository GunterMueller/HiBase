/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Antti-Pekka Liedes <apl@cs.hut.fi>
 */

/* Asynchronous dummy IO routines.
 */

static char *rev_id = "$Id: asyncio-dummy.c,v 1.4 1997/09/22 00:37:41 cessu Exp $";
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


void asyncio_init (unsigned long num_of_files)
{
}

void asyncio_create_file (unsigned long file_number, int fd)
{
}

void asyncio_open_file (unsigned long file_number, int fd)
{
}

void asyncio_close_file (unsigned long file_number, int fd)
{
}

int asyncio_write_page(unsigned long file_number, unsigned long page_number,
		       ptr_t ptr, unsigned long number_of_bytes)
{
}

int asyncio_read_page(unsigned long file_number, unsigned long page_number,
		      ptr_t ptr, unsigned long number_of_bytes)
{
}

void asyncio_drain_pending_writes (void)
{
}

void asyncio_drain_pending_reads (void)
{
}

long asyncio_get_file_load(unsigned long file_number)
{
  return 0;
}

void asyncio_reduce_file_load (unsigned long file_number)
{
}

