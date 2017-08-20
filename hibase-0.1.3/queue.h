/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@iki.fi>

 * Real-time queues implemented according to Robert Hood, Robert
 * Melville, "Real-Time Queue Operations in Pure LISP", Information
 * Processing Letters, Vol. 13, No. 2, 1981, pp. 50-54.
 *
 * More exact description of the implementation here is written in the
 * author's Thesis Tietorakenteiden tietoa tuhoamaton toteutus,
 * Helsingin yliopisto, 1998.  */


#ifndef INCL_QUEUE
#define INCL_QUEUE

#include "includes.h"
#include "shades.h"
#include "list.h"

/* Size of the CELL_queue in words.  Whenever it changes, check this! */
#define WORDS_IN_QUEUE_HEADER  5

#define QUEUE_MAX_ALLOCATION  	\
  (3 + WORDS_IN_QUEUE_HEADER + WORDS_IN_QUEUE_HEADER + 6 + 3*2*3)

/* Is the given queue empty?  Empty queues are `NULL_PTR's. */
#define QUEUE_IS_EMPTY(queue)  ((queue) == NULL_PTR)

/* Find the first item in the given queue.  This function does not
   allocate memory. */
#if defined(__GNUC__)
#define QUEUE_GET_FIRST(queue)					\
  ({								\
    assert(!QUEUE_IS_EMPTY(queue));				\
    assert(CELL_TYPE(WORD_TO_PTR((queue)[1])) == CELL_list);	\
    WORD_TO_PTR(WCAR((queue)[1]));				\
  })
#else
#define QUEUE_GET_FIRST(queue)  				\
  WORD_TO_PTR(WCAR((queue)[1]))
#endif

/* Create a new queue with the given data item inserted last into the
   queue.  Returns the new queue.  Assumes
   `can_allocate(QUEUE_MAX_ALLOCATION)'. */
ptr_t queue_insert_last(ptr_t queue, ptr_t data);

/* Create a new queue with the first item removed.  Returns the new
queue.  Assumes `can_allocate(QUEUE_MAX_ALLOCATION)'. */
ptr_t queue_remove_first(ptr_t queue);


#endif /* INCL_QUEUE */


