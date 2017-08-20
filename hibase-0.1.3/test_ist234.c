/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 *          
 */

/* Test/benchmark program for the 2-3-4 tree routines.
 */

#include "includes.h"
#include "shades.h"
#include "ist234.h"
#include "root.h"
#include "test_aux.h"

static ptr_t allocate_data(word_t data)
{
  ptr_t result = allocate(2, CELL_word_vector);
  result[0] |= 1;
  result[1] = data;
  return result;
}

typedef enum {
  INSERT, DELETE
} op_t;
  
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

static void test_ist234_insert(void)
{
  int ctr = 0, ctr_total = 0;
  word_t key = 0;
  ptr_t data;
  
  SET_ROOT_PTR(test1, NULL_PTR);
  remove_history();
  
  if (!can_allocate(2)) {
    fprintf(stderr, "Can't allocate nest egg.\n");
    exit(1);
  }
  while (1) {
    while (can_allocate(IST234_MAX_ALLOCATION_IN_INSERT + 8)) {
      /* Check if the randomly chosen `key' is in the tree. */
      key = random() % 1000;
      data = ist234_find(GET_ROOT_PTR(test1), 
			 allocate_data(key),
			 item_cmp, 
			 NULL);
      if (data == NULL_PTR)
	assert(!is_in_history(key));
      else {
	assert(is_in_history(key));
	assert(data[1] == key);
      }
      key = random() % 1000;
      /*      fprintf(stderr, "\n key == %d", key);*/
      /* Insert the key. */
      SET_ROOT_PTR(test1,
		   ist234_insert(GET_ROOT_PTR(test1), 
				 allocate_data(key),
				 item_cmp,
				 NULL, 
				 NULL,
				 allocate_data(key)));
      data = ist234_find(GET_ROOT_PTR(test1), 
			 allocate_data(key),
			 item_cmp, 
			 NULL);
      assert(data != NULL_PTR);
      assert(data[1] == key);
      prepend_history(key); 
      ctr++;
      ist234_make_assertions(GET_ROOT_PTR(test1)); 
    }
    if (GET_ROOT_PTR(test1) != NULL_PTR)
      assert(cell_check_rec(GET_ROOT_PTR(test1)));
    else 
      assert(is_empty_history());
    flush_batch();
    ctr_total += ctr;
    fprintf(stderr, "\n[%d] insertions. Total [%d]", ctr, ctr_total);
    ctr = 0;
  }
}


static void test_ist234_delete(void)
{
  int ctr = 0, ctr_total = 0, i;
  word_t key = 0;
  ptr_t data;
  
  SET_ROOT_PTR(test1, NULL_PTR);
  remove_history();
  
  if (!can_allocate(2)) {
    fprintf(stderr, "Can't allocate nest egg.\n");
    exit(1);
  }

  while (1) {
    while (can_allocate(IST234_MAX_ALLOCATION_IN_DELETE + 18)) {
      /* Check if the randomly chosen `key' is in the tree. */
      key = random() % 100000;
      data = ist234_find(GET_ROOT_PTR(test1), 
			 allocate_data(key),
			 item_cmp, 
			 NULL);
      if (data == NULL_PTR)
	assert(!is_in_history(key));
      else {
	assert(is_in_history(key));
	assert(data[1] == key);
      }
      
      /* Assert that minimum item in 2-3-4 tree is same as in history. */ 
      key = minimum_in_history();
      data = ist234_find_min(GET_ROOT_PTR(test1));
      if (key == -1)
	assert(data == NULL_PTR);
      else
	assert(data[1] == key);
      
      key = random() % 100000;
      if (random() % 60 <= 30) {
	SET_ROOT_PTR(test1,
		     ist234_insert(GET_ROOT_PTR(test1), 
				   allocate_data(key),
				   item_cmp,
				   NULL, 
				   NULL,
				   allocate_data(key)));
	ist234_make_assertions(GET_ROOT_PTR(test1)); 
	prepend_history(key);
	ctr++;
      } else {
	SET_ROOT_PTR(test1, ist234_delete(GET_ROOT_PTR(test1),
					  allocate_data(key),
					  item_cmp,
					  NULL, 
					  NULL, NULL_PTR));
	ist234_make_assertions(GET_ROOT_PTR(test1)); 
	withdraw_history(key);
	ctr--;
	if (GET_ROOT_PTR(test1) == NULL_PTR)
	  assert(is_empty_history());
	data = ist234_find(GET_ROOT_PTR(test1), 
			   allocate_data(key),
			   item_cmp, 
			   NULL);
	assert(data == NULL_PTR); 
      }

      key = random() % 100000;
      SET_ROOT_PTR(test1,
		   ist234_insert(GET_ROOT_PTR(test1), 
				 allocate_data(key),
				 item_cmp,
				 NULL, 
				 NULL,
				 allocate_data(key)));
      ist234_make_assertions(GET_ROOT_PTR(test1));
      data = ist234_find(GET_ROOT_PTR(test1), 
			 allocate_data(key),
			 item_cmp, 
			 NULL);
      assert(data != NULL_PTR);
      assert(data[1] == key);
      SET_ROOT_PTR(test1, ist234_delete(GET_ROOT_PTR(test1),
					allocate_data(key),
					item_cmp,
					NULL, 
					NULL, NULL_PTR));
      withdraw_history(key);
      if (GET_ROOT_PTR(test1) == NULL_PTR)
	assert(is_empty_history());
      ist234_make_assertions(GET_ROOT_PTR(test1));
      /* Assert the key is not anymore in the tree */
      data = ist234_find(GET_ROOT_PTR(test1), 
			 allocate_data(key),
			 item_cmp, 
			 NULL);
      assert(data == NULL_PTR);
      /* Assert that minimum item in 2-3-4 tree is same as in history. */ 
      key = minimum_in_history();
      data = ist234_find_min(GET_ROOT_PTR(test1));
      if (key == -1)
	assert(data == NULL_PTR);
      else
	assert(data[1] == key);
    }
    if (GET_ROOT_PTR(test1) != NULL_PTR)
      assert(cell_check_rec(GET_ROOT_PTR(test1)));
    else 
      assert(is_empty_history());
    flush_batch();
    fprintf(stderr, "\n[%d] items in 2-3-4 tree.", ctr);
  }
}



int main(int argc, char **argv)
{
  op_t operation;
  int c;
  fprintf(stderr, "Initializing shades...");
  init_shades(argc, argv);
  fprintf(stderr, "done\n");
  fprintf(stderr, "Creating database...");
  create_db();
  fprintf(stderr, "done\n");
  srandom(getpid()); 
  fprintf(stderr, "seed = %d\n", getpid());  
  fprintf(stderr, "\nChoose 2-3-4 operation that you want test:\n\n");
  fprintf(stderr, "0. `ist234_insert'.\n");
  fprintf(stderr, "1. `ist234_insert' and `ist234_delete'.\n");
  fprintf(stderr, "Your choice is? ");
  c = getchar();
  if  ((c != '0') && (c != '1')) {
    fprintf(stderr, "\nOnly 0 and 1 are possible. ");
    return 0;
  }
  operation = (op_t) c - '0';
  switch (operation) {
  case INSERT:
    fprintf(stderr, "\nThis test function proceeds by inserting randomly chosen keys into 2-3-4 tree. ^C stops the test...\n");
    test_ist234_insert();
  case DELETE:
    fprintf(stderr, "\nThis test function proceeds by randomly inserting and deleting randomly chosen keys into 2-3-4 tree. ^C stops the test...\n");
    test_ist234_delete(); 
  default:
    fprintf(stderr, "\nThat test-function hasn't made yet.\n");
  }
  return 0;
}
