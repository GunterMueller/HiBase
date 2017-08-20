/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Root block management.

   The root block is a fixed size sequence of words that is written at
   the end of each commit batch.  All live data in the database is
   reachable through the root block. */

#include "includes.h"
#include "root.h"
#include "io.h"
#include "shades.h"

static char *rev_id = "$Id: root.c,v 1.3 1997/03/30 22:53:29 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


word_t root[NUMBER_OF_ROOT_BLOCK_ITEMS];

static int ix_is_ptr[NUMBER_OF_ROOT_BLOCK_ITEMS];


#define ROOT_WORD(name, initial_value)		\
  root[ROOT_IX_ ## name] = (initial_value);	\
  ix_is_ptr[ROOT_IX_ ## name] = 0;
#define ROOT_PTR(name)				\
  root[ROOT_IX_ ## name] = NULL_WORD;		\
  ix_is_ptr[ROOT_IX_ ## name] = 1;

void init_root(void)
{
  assert(NUMBER_OF_ROOT_IXS * sizeof(word_t) < DISK_BLOCK_SIZE);
#include "root-def.h"
}

#undef ROOT_WORD
#undef ROOT_PTR


void increment_root_time_stamp(void)
{
  if (GET_ROOT_WORD(time_stamp_lo) == (word_t) 0xFFFFFFFFL) {
    SET_ROOT_WORD(time_stamp_hi, GET_ROOT_WORD(time_stamp_hi) + 1);
    SET_ROOT_WORD(time_stamp_lo, 0);
  } else
    SET_ROOT_WORD(time_stamp_lo, GET_ROOT_WORD(time_stamp_lo) + 1);
}


int time_stamp_is_newer_than(word_t a_hi, word_t a_lo,
			     word_t b_hi, word_t b_lo)
{
  if (a_hi > b_hi)
    return 1;
  if (a_hi < b_hi)
    return 0;
  if (a_lo > b_lo)
    return 1;
  return 0;
}


int root_ix_is_ptr(int i)
{
  assert(i >= 0);
  assert(i < NUMBER_OF_ROOT_IXS);
  return ix_is_ptr[i];
}
