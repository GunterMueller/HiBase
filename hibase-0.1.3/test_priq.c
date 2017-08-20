/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 */

/* Test routines for priority queues.
 */


#include "includes.h"
#include "shades.h"
#include "cells.h"
#include "root.h"
#include "test_aux.h"
#include "priq.h"

static char *rev_id = "$Id: test_priq.c,v 1.20 1998/03/02 15:23:09 sirkku Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

#define DURATION_IN_SECONDS        (double) (2 * 60)

static ptr_t allocate_data(word_t data)
{
  ptr_t result = allocate(2, CELL_word_vector);
  result[0] |= 1;
  result[1] = data;
  return result;
}

static cmp_result_t item_cmp(ptr_t p, ptr_t q, void *context)
{
  int number_of_words, i;

  if (context != NULL)
    return CMP_ERROR;

  assert(p != NULL_PTR && q != NULL_PTR);
  assert(CELL_TYPE(p) == CELL_word_vector);
  assert(CELL_TYPE(q) == CELL_word_vector);
  assert((p[0] & 0xFFFFFF) == (q[0] & 0xFFFFFF));
  assert((p[0] & 0xFFFFFF) > 0);

  number_of_words = p[0] & 0xFFFFFF;
  for (i = 1; i <= number_of_words; i++) {
    if (p[i] < q[i])
      return CMP_LESS;
    else if (p[i] > q[i])
      return CMP_GREATER;
  }
  return CMP_EQUAL;
}

static void test_merge(void)
{
  word_t key;
  int ops = 0;

  SET_ROOT_PTR(test1, NULL_PTR);
  SET_ROOT_PTR(test2, NULL_PTR);
  while (1) {  
    while (can_allocate(3 * PRIQ_MAX_ALLOCATION + 4)) {
      ops++;
      if (random() % 60 <= 34) {
	/* Insert an item. */
	key = random();
	SET_ROOT_PTR(test1, priq_insert(GET_ROOT_PTR(test1),  
					allocate_data(key),
					item_cmp,
					NULL));
      } else {
	/* Delete the minimum. */
	SET_ROOT_PTR(test1, priq_delete(GET_ROOT_PTR(test1),
					item_cmp, 
					NULL));
      }
      if (random() % 60 <= 34) {
	/* Insert an item. */
	key = random();
	SET_ROOT_PTR(test2, priq_insert(GET_ROOT_PTR(test2),  
					allocate_data(key),
					item_cmp,
					NULL));
      } else {
	/* Delete the minimum. */
	SET_ROOT_PTR(test2, priq_delete(GET_ROOT_PTR(test2),
					item_cmp, 
					NULL));
      }      
      priq_make_assertions(GET_ROOT_PTR(test1));
      priq_make_assertions(GET_ROOT_PTR(test2));
      if (GET_ROOT_PTR(test1) != NULL_PTR)
	assert(cell_check_rec(GET_ROOT_PTR(test1)));
      if (GET_ROOT_PTR(test2) != NULL_PTR)
	assert(cell_check_rec(GET_ROOT_PTR(test2)));
      if (random() % 1024 <= 100) {
	if (random() % 2 == 1) {
	  SET_ROOT_PTR(test1, priq_merge(GET_ROOT_PTR(test1),
					 GET_ROOT_PTR(test2),
					 item_cmp, 
					 NULL));
	  SET_ROOT_PTR(test2, NULL_PTR);
	} else {
	  SET_ROOT_PTR(test2, priq_merge(GET_ROOT_PTR(test1),
					 GET_ROOT_PTR(test2),
					 item_cmp, 
					 NULL));
	  SET_ROOT_PTR(test1, NULL_PTR);
	}
      }
    }
    fprintf(stderr, "\n%d", ops);
    flush_batch();
  }
  return;
}
  
		
    
static void test_priq(void)
{
  ptr_t data;
  word_t key;
  cmp_result_t res;
  double start_time = give_time(), end_time;
  int ops = 0, insertions = 0, deletions = 0;

  SET_ROOT_PTR(test1, NULL_PTR);
  while (1) {  
    while (can_allocate(PRIQ_MAX_ALLOCATION + 4)) {
      if (random() % 60 <= 34) {
	/* Insert an item. */
	ops++;
	key = random();
	SET_ROOT_PTR(test1, priq_insert(GET_ROOT_PTR(test1),  
					allocate_data(key),
					item_cmp,
					NULL));
	prepend_history(key);
      } else {
	/* Delete the minimum. */
	if (avl_destr_find_min(insertion_history, &key)) {
	  ops--;
	  SET_ROOT_PTR(test1, priq_delete(GET_ROOT_PTR(test1), 
					  item_cmp, 
					  NULL));
	  withdraw_history(key);
	}
      }
      if (avl_destr_find_min(insertion_history, &key)) {
	data = allocate_data(key);
	res = item_cmp(priq_find_min(GET_ROOT_PTR(test1), item_cmp, NULL), data, NULL);
	assert(res == CMP_EQUAL);
      } else {
	assert(GET_ROOT_PTR(test1) == NULL_PTR);
	assert(priq_find_min(GET_ROOT_PTR(test1), item_cmp, NULL) == NULL_PTR);
      }
      if (GET_ROOT_PTR(test1) != NULL_PTR)
	assert(cell_check_rec(GET_ROOT_PTR(test1)));
      priq_make_assertions(GET_ROOT_PTR(test1));
    }
    /*end_time = give_time();
    fprintf(stderr, "\n%d", end_time);
    if ((end_time - start_time) >= DURATION_IN_SECONDS) {
      fprintf(stderr, "\n%d operations in %d seconds", ops, DURATION_IN_SECONDS);    
      return;
    }*/
    fprintf(stderr, "\n%d items in priority queue", ops);
    if (ops > 20000)
      return;
    else
      flush_batch();
  }
  return;
}

int main(int argc, char **argv)
{
  init_shades(argc, argv);
  create_db();
  srandom(getpid());
  fprintf(stderr, "seed = %d\n", getpid());
  fprintf(stderr, "Testing `priq_insert' and `priq_delete'...");
  test_priq(); 
  fprintf(stderr, "Testing `priq_merge'...");
  test_merge();
  return 0;
}









