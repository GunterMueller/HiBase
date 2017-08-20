/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 *
 * Test/benchmark program for queues.
 */

#include "includes.h"
#include "queue.h"
#include "root.h"
#include "shades.h"
#include "test_aux.h"
#include <time.h>

static char *rev_id = "$Id: test_queue.c,v 1.30 1998/02/18 10:16:41 sirkku Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


int main(int argc, char **argv)
{
  word_t first = 0, last = 0;
  int queue_length = 0, ops = 0, i = 0, loops = 0, j = 0;
  ptr_t p;
  double start_time, end_time;
  
  srandom(getpid());
  fprintf(stderr, "seed = %d\n", getpid());
  
  init_shades(argc, argv);
  create_db();

  ops = 20000000 % 0xFFFFFFFF;
  loops = 1;

  SET_ROOT_PTR(test1, NULL_PTR);
  fprintf(stderr, "\nTesting queues...");
  start_time = give_time();
  while (i < loops) {
    while (j < ops) {
      if (queue_length > 100) {
	SET_ROOT_PTR(test1, NULL_PTR);
	queue_length = 0;
	first = last = 0;
      }
      if (!QUEUE_IS_EMPTY(GET_ROOT_PTR(test1))) {
       	p = QUEUE_GET_FIRST(GET_ROOT_PTR(test1));
	assert(p[1] == first);
      }
      if (QUEUE_IS_EMPTY(GET_ROOT_PTR(test1))
	  || random() % 64 >= 30) {
	/* Insert a new key. */
	while (!can_allocate(QUEUE_MAX_ALLOCATION 
			     + 2 /* The CELL_word_vector below */))
	  flush_batch();
	p = allocate(2, CELL_word_vector);
	p[0] |= 1;
	p[1] = ++last;
	if (QUEUE_IS_EMPTY(GET_ROOT_PTR(test1)))
          first = p[1];
	SET_ROOT_PTR(test1, queue_insert_last(GET_ROOT_PTR(test1), p));
	queue_length++;
      } else {
	/* Remove a key */
	while (!can_allocate(QUEUE_MAX_ALLOCATION 
			     + 2 /* The CELL_word_vector below */))
	  flush_batch();
	SET_ROOT_PTR(test1, queue_remove_first(GET_ROOT_PTR(test1)));
	first++;
	queue_length--;
      }
      assert(GET_ROOT_PTR(test1) == NULL_PTR
            || cell_check_rec(GET_ROOT_PTR(test1)));
      j++;
    }
    j = 0;
    i++;
  }
  end_time = give_time();
  fprintf(stderr, "end. Total duration %.3f\n", end_time - start_time);
  return 0;
}




