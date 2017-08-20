/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 */


/* Test/benchmark program for queues.
 *
 * This is best run with very small first generation sizes in order to
 * stress the gc-safety.  E.g.
 *   ./configure --enable-page-size=4
 *   ./test_queue --first-generation-size=8k -v
 */

#include "includes.h"
#include "lq.h"
#include "root.h"
#include "shades.h"
#include "test_aux.h"
#include <time.h>
static void test_insert(void) 
{
  ptr_t item;
  int i = 0, number_of_insertions; 
  

  fprintf(stderr, "\nTesting insert operation in logarithmic queue...");  
  /* Insert 4000000 items to the queue. */
  while (1) {
    while (!can_allocate(LQ_INSERT_MAX_ALLOCATION 
			 + 2 /* The CELL_word_vector below */)) { 
      fprintf(stderr, "\nflush_batch();\n");
      number_of_insertions = 0;
      flush_batch();
    }
    /* Insert new item. */
    item = allocate(2, CELL_word_vector);
    item[1] = i++;
    SET_ROOT_PTR(test1, lq_insert_last(GET_ROOT_PTR(test1), item));

    fprintf(stderr, 
	    " %8ld                                                 %c",
	    number_of_insertions, 13);
    number_of_insertions++;
    
    assert(GET_ROOT_PTR(test1) == NULL_PTR
	   || cell_check_rec(GET_ROOT_PTR(test1)));
	   
  }
}

static void test_delete(void) 
{
  ptr_t item;
  int i, first_item_in_queue = 0, last_item_in_queue = 0, number_of_items_in_queue; 
  
  fprintf(stderr, "\nTesting insert and delete operation in logarithmic queue...");  
  /* First insert 100 items to the queue. */
  for (i = 0; i < 100; i++) {
    while (!can_allocate(LQ_INSERT_MAX_ALLOCATION 
			 + 2 /* The CELL_word_vector below */)) { 
      fprintf(stderr, "\nflush_batch();\n");
      flush_batch();
    }
    /* Insert new item. */
    item = allocate(2, CELL_word_vector);
    item[1] = last_item_in_queue++;
    SET_ROOT_PTR(test1, lq_insert_last(GET_ROOT_PTR(test1), item));
    number_of_items_in_queue = last_item_in_queue - first_item_in_queue + 1;
    fprintf(stderr, 
	    " %8ld                                                 %c",
	    number_of_items_in_queue, 13);
  }
  srandom(getpid());

  while (1) {
    if ((random() % 100 <= 49) || (GET_ROOT_PTR(test1) == NULL_PTR)) {
      while (!can_allocate(LQ_INSERT_MAX_ALLOCATION 
			   + 2 /* The CELL_word_vector below */)) { 
	fprintf(stderr, "\nflush_batch();\n");
	flush_batch();
      }
      /* Insert new item. */
      item = allocate(2, CELL_word_vector);
      item[1] = last_item_in_queue++;
      SET_ROOT_PTR(test1, lq_insert_last(GET_ROOT_PTR(test1), item));
    } else {
      while (!can_allocate(LQ_REMOVE_MAX_ALLOCATION)) { 
	fprintf(stderr, "\nflush_batch();\n");
	flush_batch();
      }
      SET_ROOT_PTR(test1, lq_remove_first(GET_ROOT_PTR(test1)));
      first_item_in_queue++;
    }
    number_of_items_in_queue = last_item_in_queue - first_item_in_queue + 1;
    fprintf(stderr, 
	    " %8ld                                                 %c",
	    number_of_items_in_queue, 13);
    
    assert(GET_ROOT_PTR(test1) == NULL_PTR
	   || cell_check_rec(GET_ROOT_PTR(test1)));
    
  }
}

static void test_delete_one_million(void) 
{
  ptr_t item;
  int i, first_item_in_queue = 0, last_item_in_queue = 0, number_of_items_in_queue; 
  
  fprintf(stderr, "\nFirst inserts one million items to the queue, then removes them...");  

  /* First insert one million items to the queue. */
  for (i = 0; i < 1000000; i++) {
    while (!can_allocate(LQ_INSERT_MAX_ALLOCATION 
			 + 2 /* The CELL_word_vector below */)) { 
      fprintf(stderr, "\nflush_batch();\n");
      flush_batch();
    }
    /* Insert new item. */
    item = allocate(2, CELL_word_vector);
    item[0] |= 1;
    item[1] = last_item_in_queue++;
    SET_ROOT_PTR(test1, lq_insert_last(GET_ROOT_PTR(test1), item));
    number_of_items_in_queue = last_item_in_queue - first_item_in_queue + 1;
    fprintf(stderr, 
	    " %8ld                                                 %c",
	    number_of_items_in_queue, 13);
  }

  fprintf(stderr,"\n");

  /* Remove one million items from the head. */
  for (i = 1000000; i > 0; i--) {
    while (!can_allocate(LQ_REMOVE_MAX_ALLOCATION)) {
      fprintf(stderr, "\nflush_batch();\n");
      flush_batch();
    }
    /* Remove item from head. */
    assert(GET_ROOT_PTR(test1) != NULL_PTR);
    SET_ROOT_PTR(test1, lq_remove_first(GET_ROOT_PTR(test1)));
    fprintf(stderr, 
	    " %8ld                                                 %c",
	    i, 13);
    assert(GET_ROOT_PTR(test1) == NULL_PTR
	   || cell_check_rec(GET_ROOT_PTR(test1)));
  }

  assert(GET_ROOT_PTR(test1) == NULL_PTR);  
}

int main(int argc, char **argv)
{
  double start_time, end_time;
    
  init_shades(argc, argv);
  create_db();

  SET_ROOT_PTR(test1, NULL_PTR);
  start_time = give_time();
  test_delete_one_million();
  end_time = give_time();
  fprintf(stderr, "end. Total duration %.3f\n", end_time - start_time);
  return 0;
}




