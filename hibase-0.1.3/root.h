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


#ifndef INCL_ROOT_H
#define INCL_ROOT_H 1


#include "includes.h"
#include "io.h"


#define ROOT_WORD(name, initial_value)		\
  ROOT_IX_ ## name,
#define ROOT_PTR(name)				\
  ROOT_IX_ ## name,

typedef enum {
#include "root-def.h"
  NUMBER_OF_ROOT_IXS
} root_ix_t;

#undef ROOT_WORD
#undef ROOT_PTR


#define ROOT_BLOCK_SIZE  DISK_BLOCK_SIZE
#define NUMBER_OF_ROOT_BLOCK_ITEMS  (ROOT_BLOCK_SIZE / sizeof(word_t))

extern word_t root[];


#define GET_ROOT_WORD(name)			\
  root[ROOT_IX_ ## name]
#define GET_ROOT_WORD_VECTOR(name, index)	\
  root[ROOT_IX_ ## name + (index)]
#define SET_ROOT_WORD(name, value)		\
  do {						\
    root[ROOT_IX_ ## name] = (value);		\
  } while (0)
#define SET_ROOT_WORD_VECTOR(name, index, value)	\
  do {							\
    root[ROOT_IX_ ## name + (index)] = (value);		\
  } while (0)

#define GET_ROOT_PTR(name)			\
  WORD_TO_PTR(root[ROOT_IX_ ## name])
#define GET_ROOT_PTR_VECTOR(name, index)	\
  WORD_TO_PTR(root[ROOT_IX_ ## name + (index)])
#define SET_ROOT_PTR(name, ptr)			\
  do {						\
    root[ROOT_IX_ ## name] = PTR_TO_WORD(ptr);	\
  } while (0)
#define SET_ROOT_PTR_VECTOR(name, index, ptr)			\
  do {								\
    root[ROOT_IX_ ## name + (index)] = PTR_TO_WORD(ptr);	\
  } while (0)


int root_ix_is_ptr(int i);

void increment_root_time_stamp(void);

int time_stamp_is_newer_than(word_t time_stamp_a_hi, word_t time_stamp_a_lo,
			     word_t time_stamp_b_hi, word_t time_stamp_b_lo);

/* Called from `shades.c' to initialize the root block. */
void init_root(void);


#endif /* INCL_ROOT_H */
