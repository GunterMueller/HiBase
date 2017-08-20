/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Sirkku-Liisa Paajenen <sirkku@iki.fi>
 */


/* A logarithmic queue. For more information see author's thesis
   Tietorakenteiden tietoa tuhoamaton toteutus. */

#ifndef INCL_LQ
#define INCL_LQ

#include "includes.h"
#include "shades.h"

#define LQ_INSERT_MAX_ALLOCATION  	(14)
#define LQ_REMOVE_MAX_ALLOCATION  	(3 + 3 * 31 * 4)

/* Return the first item in the given queue, or NULL_PTR if the qiven
   queue is empty.  This function does not allocate memory. */
ptr_t lq_get_first(ptr_t queue);

#if 0
/* Return the last item in the given queue, or NULL_PTR if the qiven
   queue is empty. */
ptr_t lq_get_last(ptr_t queue);
#endif

/* Create a new queue with the given data item inserted last into the
   queue.  Returns the new queue.  Assumes
   `can_allocate(LQ_INSERT_MAX_ALLOCATION)'. */
ptr_t lq_insert_last(ptr_t queue, ptr_t data);

/* Create a new queue with the first item removed. */
ptr_t lq_remove_first(ptr_t queue);


#endif /* INCL_LQ */
