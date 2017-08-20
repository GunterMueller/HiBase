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
 * author's Thesis Tietorakenteiden  tietoa tuhoamaton toteutus,
 * Helsingin yliopisto, 1998. */

#include "includes.h"
#include "queue.h"

static char *rev_id = "$Id: queue.c,v 1.50 1998/03/02 15:49:03 sirkku Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

/* If the queue is not empty e.q queue != NULL_PTR, the type of the
entry point of the queue is `CELL_queue' or `CELL_queue_rev'. The
latter is used when transferring process is unfinished.

CELL_queue:

       DECLARE_NONNULL_PTR(p[1]);  Head. 
       DECLARE_PTR(p[2]);	   Tail. 
       DECLARE_WORD(p[3]);	   Number of keys in the head. 
       DECLARE_WORD(p[4]);	   Number of keys in the tail. 
    
CELL_queue_rev has some extra fields also:

       DECLARE_PTR(p[5]);          A pointer to unreversed part of the head.
       DECLARE_PTR(p[6]);	   Reversed head. 
       DECLARE_PTR(p[7]);	   A pointer to unreversed part of the tail.
       DECLARE_PTR(p[8]);	   Reversed tail. 
       DECLARE_WORD(p[9]);	   Number of keys transferred from p[6] 
				   to p[8]. This will be needed in the 
				   second phase. 
       DECLARE_WORD(p[10]);        Number of keys in head when transferring
				   process started.  

Extra fields will be needed during the transferring process: Transferring process consists of two phases:

 1) Transfer items from the head pointed by `p[5]' to the `p[6]'.  At
    the same time transfe items from the tail pointed by `p[7]' to the
    `p[8]'.

 2) Transfer items from `p[6]' to to `p[7]'. 

The tranferring process will be started when the number of items in
the head and tail is same.

*/


/* Create a new queue with the given data item inserted last into the
   queue.  Returns the new queue.  Assumes
   `can_allocate(QUEUE_INSERT_LAST_MAX_ALLOCATION)'. */
ptr_t queue_insert_last(ptr_t queue, ptr_t data)
{
  ptr_t new_queue;
  word_t new_tail;
  unsigned long i;

  if (QUEUE_IS_EMPTY(queue)) {
    new_queue = allocate(WORDS_IN_QUEUE_HEADER, CELL_queue);
    new_queue[1] = WCONS(PTR_TO_WORD(data), NULL_WORD);
    new_queue[2] = NULL_WORD;
    new_queue[3] = 1;
    new_queue[4] = 0;
    return new_queue;
  } 
  if (CELL_TYPE(queue) == CELL_queue) {
    if (queue[3] == queue[4]) {
      /* After inserting the given item, the length of tail will be 
	 greater than the length of head; so start the transferring process. */
      if (queue[3] == 1) {
	/* This is special. There's no need to start any heavy transferring 
	   process because in the queue exists only two data items. */
	new_queue = allocate(WORDS_IN_QUEUE_HEADER, CELL_queue);

	new_tail = WCONS(PTR_TO_WORD(data), NULL_WORD); 
	new_tail = WCONS(WCAR(queue[2]), new_tail);
        new_queue[1] = WCONS(WCAR(queue[1]), new_tail); 
	new_queue[2] = NULL_WORD; 
	new_queue[3] = 3;
	new_queue[4] = 0;
      } else {
	new_queue = allocate(WORDS_IN_QUEUE_HEADER + 6, CELL_queue_rev);
	new_queue[1] = queue[1];
	new_queue[2] = NULL_WORD;
	new_queue[3] = queue[3];
	new_queue[4] = 0;
	new_queue[5] = new_queue[1];
	new_queue[6] = NULL_WORD;
	new_queue[7] = queue[2];
	new_queue[8] = WCONS(PTR_TO_WORD(data), NULL_WORD);
	new_queue[9] = 0;/* Number of reserved items during the 
			    second phase of the transferring process. */ 
	new_queue[10] = queue[3];/* Number of items in tail when the 
				    transferring process started. */ 
	for (i = 0; i < 2 + (new_queue[3] % 2); i++) {
	  /* Take one item from the old head and insert it to the 
	     reversed old head. */
	  new_queue[6] = WCONS(WCAR(new_queue[5]), new_queue[6]);
	  new_queue[5] = WCDR(new_queue[5]);
	  /* Take one item from the old tail and insert it to the 
	     front of new head. */
	  new_queue[8] = WCONS(WCAR(new_queue[7]), new_queue[8]);
	  new_queue[7] = WCDR(new_queue[7]);
	}
      }
    } else {
      /* There's no need to reverse anything. Insert the data item as 
	 normal. */
      new_queue = allocate(WORDS_IN_QUEUE_HEADER, CELL_queue);
      new_queue[1] = queue[1];
      new_queue[2] = WCONS(PTR_TO_WORD(data), queue[2]);
      new_queue[3] = queue[3];
      new_queue[4] = queue[4] + 1;
    }
  } else {
    /* The transferring process is not complete. */
    assert(CELL_TYPE(queue) == CELL_queue_rev);
    /* Insert the new data item as normal. */
    new_queue = allocate(WORDS_IN_QUEUE_HEADER + 6, CELL_queue_rev);
    new_queue[1] = queue[1];
    new_queue[2] = WCONS(PTR_TO_WORD(data), queue[2]);
    new_queue[3] = queue[3];
    new_queue[4] = queue[4] + 1;  
    new_queue[5] = queue[5];
    new_queue[6] = queue[6];
    new_queue[7] = queue[7];
    new_queue[8] = queue[8];
    new_queue[9] = queue[9];
    new_queue[10] = queue[10];
    if (new_queue[5] == NULL_WORD) {
      /* The old head and tail are already reversed. */
      assert(new_queue[7] == NULL_WORD);
      if ((queue[9] == 0) && (new_queue[3] % 2)) {
	new_queue[8] = WCONS(WCAR(new_queue[6]), new_queue[8]);
	new_queue[6] = WCDR(new_queue[6]);
	new_queue[9]++;
      }
      /* Proceed the next two steps of the second phase. */
      for (i = 0; (i < 2) && (new_queue[3] > new_queue[9]); i++) { 
	new_queue[8] = WCONS(WCAR(new_queue[6]), new_queue[8]);
	new_queue[6] = WCDR(new_queue[6]);
	new_queue[9]++;
      }
      if (new_queue[3] == new_queue[9]) {
	/* The transferring operation is complete. */
	queue = allocate(WORDS_IN_QUEUE_HEADER, CELL_queue);
	queue[1] = new_queue[8];
	queue[2] = new_queue[2];
	queue[3] = new_queue[3] + new_queue[10] + 1;
	queue[4] = new_queue[4];
	return queue;
      }
    } else {
      /* Reversal of the old tail is not yet complete. 
	 Reverse the next two data items from the old head and tail. */
	new_queue[6] = WCONS(WCAR(new_queue[5]), new_queue[6]);
	new_queue[5] = WCDR(new_queue[5]);
	new_queue[6] = WCONS(WCAR(new_queue[5]), new_queue[6]);
	new_queue[5] = WCDR(new_queue[5]);
	new_queue[8] = WCONS(WCAR(new_queue[7]), new_queue[8]);
      	new_queue[7] = WCDR(new_queue[7]);
	new_queue[8] = WCONS(WCAR(new_queue[7]), new_queue[8]);
      	new_queue[7] = WCDR(new_queue[7]);
    }
  }
  return new_queue;
}

/* Create a new queue with the first item removed.  Returns the new
queue.  Assumes `can_allocate(QUEUE_MAX_ALLOCATION)'. */
ptr_t queue_remove_first(ptr_t queue)
{

  ptr_t new_queue;
  word_t new_tail;
  unsigned long i;

  assert(!QUEUE_IS_EMPTY(queue));
  assert(CELL_TYPE(WORD_TO_PTR(queue[1])) == CELL_list);
  
  if (CELL_TYPE(queue) == CELL_queue) {
    if (queue[3] == queue[4]) {
      /* Start the transferring process. */
      if (queue[3] == 1) {
	/* This is special. There's no need to start any heavy transferring 
	   process because in the queue shall exist only one data item. */
	new_queue = allocate(WORDS_IN_QUEUE_HEADER, CELL_queue);
	new_queue[1] = queue[2];
	new_queue[2] = NULL_WORD;
	new_queue[3] = 1;
	new_queue[4] = 0;
      } else if (queue[3] == 2) {
	/* This is also special because in the queue shall exist 
	   only three data items. */
	new_queue = allocate(WORDS_IN_QUEUE_HEADER, CELL_queue);
	new_tail = WCONS(WCAR(queue[2]), NULL_WORD);
	new_tail = WCONS(WCADR(queue[2]), new_tail);
	new_queue[1] = WCONS(WCADR(queue[1]), new_tail);
	new_queue[2] = NULL_WORD;
	new_queue[3] = 3;
	new_queue[4] = 0;
      } else {
	new_queue = allocate(WORDS_IN_QUEUE_HEADER + 6, CELL_queue_rev);
	new_queue[1] = WCDR(queue[1]);
	new_queue[2] = NULL_WORD;
	new_queue[3] = queue[3] - 1;
	new_queue[4] = 0;
	new_queue[5] = new_queue[1]; 
	new_queue[6] = NULL_WORD; 
	new_queue[8] = WCONS(WCAR(queue[2]), NULL_WORD);
	new_queue[7] = WCDR(queue[2]); 
	new_queue[9] = 0;/* Number of reversed items during the 
			    second phase of the transferring process. */ 
	new_queue[10] = new_queue[3];/* Number of items in head when the 
					transferring process started. */
	for (i = 0; i < 2 + (new_queue[3] % 2); i++) {
	  new_queue[6] = WCONS(WCAR(new_queue[5]), new_queue[6]);
	  new_queue[5] = WCDR(new_queue[5]);
	  new_queue[8] = WCONS(WCAR(new_queue[7]), new_queue[8]);
	  new_queue[7] = WCDR(new_queue[7]);
	}
      }
    } else {
      /* There's no need to reverse anything. Delete the first of head
	 as normal. */
      if ((queue[3] == 1) && (queue[4] == 0))
	/* The queue became empty. */
	return NULL_PTR;
      new_queue = allocate(WORDS_IN_QUEUE_HEADER, CELL_queue);
      new_queue[1] = WCDR(queue[1]);
      new_queue[2] = queue[2];
      new_queue[3] = queue[3] - 1;
      new_queue[4] = queue[4];
    }
  } else {
    /* The transferring process is not complete. */
    assert(CELL_TYPE(queue) == CELL_queue_rev);
    /* Delete the first of head as normal. */
    new_queue = allocate(WORDS_IN_QUEUE_HEADER + 6, CELL_queue_rev);
    new_queue[1] = WCDR(queue[1]);
    new_queue[2] = queue[2];
    new_queue[3] = queue[3] - 1;
    new_queue[4] = queue[4];
    new_queue[5] = queue[5];
    new_queue[6] = queue[6];
    new_queue[7] = queue[7];
    new_queue[8] = queue[8];
    new_queue[9] = queue[9];
    new_queue[10] = queue[10];    
    if (new_queue[5] == NULL_WORD) {
      /* The old head and tail are already reversed. */
      assert(new_queue[7] == NULL_WORD);
      if ((new_queue[9] == 0) && (new_queue[3] % 2)) {
	/* Start the second phase. */
	new_queue[8] = WCONS(WCAR(new_queue[6]), new_queue[8]);
	new_queue[6] = WCDR(new_queue[6]);
	new_queue[9]++;
      }
      /* Proceed the next two steps in reversing the once reversed old 
	 head to the front of the reversed old tail. */
      for (i = 0; (i < 2) && (new_queue[3] > new_queue[9]); i++) {
	new_queue[8] = WCONS(WCAR(new_queue[6]), new_queue[8]);
	new_queue[6] = WCDR(new_queue[6]);
	new_queue[9]++;
      }
      if (new_queue[3] == new_queue[9]) {
	/* The transferring operation is complete. */
	queue = allocate(WORDS_IN_QUEUE_HEADER, CELL_queue);
	queue[1] = new_queue[8];
	queue[2] = new_queue[2];
	queue[3] = new_queue[3] + new_queue[10] + 1;
	queue[4] = new_queue[4];
	return queue;
      }
    } else {
      /* Reversal of the old tail is not yet complete. 
	 Reverse the next two data items from old head and tail. */
	new_queue[6] = WCONS(WCAR(new_queue[5]), new_queue[6]);
	new_queue[5] = WCDR(new_queue[5]);
	new_queue[6] = WCONS(WCAR(new_queue[5]), new_queue[6]);
	new_queue[5] = WCDR(new_queue[5]);
	new_queue[8] = WCONS(WCAR(new_queue[7]), new_queue[8]);
	new_queue[7] = WCDR(new_queue[7]);
	new_queue[8] = WCONS(WCAR(new_queue[7]), new_queue[8]);
	new_queue[7] = WCDR(new_queue[7]);
    }
  }
  return new_queue;
}
