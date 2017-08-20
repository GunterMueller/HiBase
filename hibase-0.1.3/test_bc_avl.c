/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@cs.hut.fi>
 *          
 */

/* Test/benchmark program for the AVL-tree routines.
 */

#include "includes.h"
#include "shades.h"
#include "avl.h"
#include "root.h"
#include "test_aux.h"

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

static void test_avl_bc_insert(void)
{
  int ctr = 0, ctr_total = 0, result;
  word_t key = 0;
  ptr_t data;

  SET_ROOT_PTR(test1, NULL_PTR);
  remove_history();
  
  while (1) {
    while (can_allocate(AVL_MAX_ALLOCATION_IN_INSERT 
			+ 48*AVL_BC_DELVE_MAX_ALLOCATION + 6)) {
      /* Check if the randomly chosen `key' is in the tree. */
      key = random();
      data = avl_find(GET_ROOT_PTR(test1), 
		      allocate_data(key),
		      item_cmp, 
		      NULL);
      if (data == NULL_PTR)
	assert(!is_in_history(key));
      else {
	assert(is_in_history(key));
	assert(data[1] == key);
      }
      
      key = random();
      /* Insert path in history (`test2'). */
      result = 1;
      SET_ROOT_PTR(test2,
		   build_history_for_insert(GET_ROOT_PTR(test1), 
					    allocate_data(key),
					    item_cmp,
					    NULL, 
					    &result));
      if (result > 0) {
	/* Insert the key (`test1'). */
	SET_ROOT_PTR(test1,
		     avl_bc_insert(GET_ROOT_PTR(test2), 
				   allocate_data(key),
				   allocate_data(key)));
	prepend_history(key); 
	ctr++;
	avl_make_assertions(GET_ROOT_PTR(test1));
	/* Check that it really is in the tree. */
	data = avl_find(GET_ROOT_PTR(test1), 
			allocate_data(key),
			item_cmp, 
			NULL);
	assert(data != NULL_PTR);
	assert(is_in_history(key));
	assert(data[1] == key);
      }

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

static void test_avl_bc_delete(void)
{
  int ctr = 0, ctr_total = 0, result;
  word_t key = 0;
  ptr_t data;

  SET_ROOT_PTR(test1, NULL_PTR);
  remove_history();
  while (1) {
    while (can_allocate(3*AVL_MAX_ALLOCATION_IN_DELETE 
			+ 3*48*AVL_BC_DELVE_MAX_ALLOCATION + 24)) {     
      /* Check if the randomly chosen `key' is in the tree. */
      key = random();
      data = avl_find(GET_ROOT_PTR(test1), 
		      allocate_data(key),
		      item_cmp, 
		      NULL);
      if (data == NULL_PTR) {
	/* It isn't. Insert the key. */
	assert(!is_in_history(key));
	result = 1;
	SET_ROOT_PTR(test2,
		     build_history_for_insert(GET_ROOT_PTR(test1), 
					      allocate_data(key),
					      item_cmp,
					      NULL, 
					      &result));
	assert(result > 0); 
	/* Insert the key in the AVL tree. */
	SET_ROOT_PTR(test1,
		     avl_bc_insert(GET_ROOT_PTR(test2), 
				   allocate_data(key),
				   allocate_data(key)));
	prepend_history(key); 
	ctr++;
	/* Check that it really is in the tree. */
	data = avl_find(GET_ROOT_PTR(test1), 
			allocate_data(key),
			item_cmp, 
			NULL);
	assert(data != NULL_PTR);
	assert(is_in_history(key));
	assert(data[1] == key);
      } else {
	/* It is. Delete it. */
	assert(is_in_history(key));
	assert(data[1] == key);
	result = 1;
	SET_ROOT_PTR(test2,
		     build_history_for_delete(GET_ROOT_PTR(test1), 
					      allocate_data(key),
					      item_cmp,
					      NULL, 
					      &result));
	assert(result > 0);
	SET_ROOT_PTR(test1, avl_bc_delete(GET_ROOT_PTR(test2)));
	withdraw_history(key);
	ctr--;
	/* Check that it really isn't anymore the tree. */
	data = avl_find(GET_ROOT_PTR(test1), 
			allocate_data(key),
			item_cmp, 
			NULL);
	assert(data == NULL_PTR);  
      }
      
      key = random();
      /* First insert and then delete `key'. */
      result = 1;
      SET_ROOT_PTR(test2,
		   build_history_for_insert(GET_ROOT_PTR(test1), 
					    allocate_data(key),
					    item_cmp,
					    NULL, 
					    &result));
      if (result > 0) { 
	/* Insert the key in the AVL tree. */
	SET_ROOT_PTR(test1,
		     avl_bc_insert(GET_ROOT_PTR(test2), 
				   allocate_data(key),
				   allocate_data(key)));
      }
      data = avl_find(GET_ROOT_PTR(test1), 
			allocate_data(key),
		      item_cmp, 
		      NULL);
      assert(data != NULL_PTR);
      assert(data[1] == key);
      /* Insert path in history. */
      result = 1;
      SET_ROOT_PTR(test2,
		   build_history_for_delete(GET_ROOT_PTR(test1), 
					    allocate_data(key),
					    item_cmp,
					    NULL, 
					    &result));
      assert(result > 0);
      SET_ROOT_PTR(test1, avl_bc_delete(GET_ROOT_PTR(test2)));
      data = avl_find(GET_ROOT_PTR(test1), 
		      allocate_data(key),
		      item_cmp, 
		      NULL);
      assert(data == NULL_PTR);
      /* Assert that minimum item in AVL tree is same as in history. */ 
      key = minimum_in_history();
      data = avl_find_min(GET_ROOT_PTR(test1));
      if (key == -1)
	assert(data == NULL_PTR);
      else
	  assert(data[1] == key);
      avl_make_assertions(GET_ROOT_PTR(test1)); 
    }
    if (GET_ROOT_PTR(test1) != NULL_PTR)
      assert(cell_check_rec(GET_ROOT_PTR(test1)));
    else 
      assert(is_empty_history());
    flush_batch();
    fprintf(stderr, "\n[%d] items in AVL tree.", ctr);
  }
}

int main(int argc, char **argv)
{
  int c;
  init_shades(argc, argv);
  fprintf(stderr, "done\n");
  create_db();
  srandom(getpid());
  fprintf(stderr, "seed = %d\n", getpid());  
  fprintf(stderr, "\nChoose AVL bc-operation that you want test:\n\n");
  fprintf(stderr, "0. `avl_bc_insert'.\n");
  fprintf(stderr, "1. `avl_bc_insert' and `avl_bc_delete'.\n");
  fprintf(stderr, "Your choice is? ");
  c = getchar();
  if  ((c != '0') && (c != '1')) {
    fprintf(stderr, "\nOnly 0 and 1 are possible. ");
    return 0;
  }
  if (c == '0') {
    fprintf(stderr, "\nTesting avl_bc_insert...\n");
    test_avl_bc_insert();
  }
  fprintf(stderr, "\nTesting avl_bc_delete...\n");
  test_avl_bc_delete();
  return 0;
}

