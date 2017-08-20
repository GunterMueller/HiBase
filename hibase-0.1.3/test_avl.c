/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 *          
 */

/* Test/benchmark program for the AVL-tree routines.
 */

#include "includes.h"
#include "shades.h"
#include "avl.h"
#include "root.h"
#include "test_aux.h"

static char *rev_id = "$Id: test_avl.c,v 1.58 1998/02/18 08:38:41 sirkku Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

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

static void test_avl_insert()
{
  int ctr = 0, ctr_total = 0;
  word_t key = 0;
  ptr_t data;

  SET_ROOT_PTR(test1, NULL_PTR);
  remove_history();
  
  if (!can_allocate(AVL_MAX_ALLOCATION_IN_INSERT + 4)) {
    fprintf(stderr, "Can't allocate nest egg.\n");
    exit(1);
  }
  while (1) {
    while (can_allocate(AVL_MAX_ALLOCATION_IN_INSERT + 6)) {
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
      /* Insert the key. */
      SET_ROOT_PTR(test1,
		   avl_insert(GET_ROOT_PTR(test1), 
			      allocate_data(key),
			      item_cmp,
			      NULL, 
			      NULL,
			      allocate_data(key)));
      prepend_history(key);
      /* Check that the key really is in AVL-tree. */
      data = avl_find(GET_ROOT_PTR(test1), 
		      allocate_data(key),
		      item_cmp, 
		      NULL);
      assert(data != NULL_PTR);
      assert(is_in_history(key));
      assert(data[1] == key);
      ctr++;
      avl_make_assertions(GET_ROOT_PTR(test1));
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

static void test_avl_delete()
{
  int ctr = 0, ctr_total = 0;
  word_t key = 0;
  ptr_t data;

  SET_ROOT_PTR(test1, NULL_PTR);
  remove_history();
  
  if (!can_allocate(AVL_MAX_ALLOCATION_IN_INSERT + 4)) {
    fprintf(stderr, "Can't allocate nest egg.\n");
    exit(1);
  }
  while (1) {
    while (can_allocate(3 * AVL_MAX_ALLOCATION_IN_DELETE + 20)) {
      /* Check if the randomly chosen `key' is in the tree. */
      key = random();
      data = avl_find(GET_ROOT_PTR(test1), 
		      allocate_data(key),
		      item_cmp, 
		      NULL);
      if (data == NULL_PTR) {
	/* It isn't. Insert the key. */
	assert(!is_in_history(key));
	SET_ROOT_PTR(test1,
		     avl_insert(GET_ROOT_PTR(test1), 
				allocate_data(key),
				item_cmp,
				NULL, 
				NULL,
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
	SET_ROOT_PTR(test1, avl_delete(GET_ROOT_PTR(test1),
				       allocate_data(key),
				       item_cmp,
				       NULL, 
				       NULL,
				       allocate_data(key)));
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
      SET_ROOT_PTR(test1,
		   avl_insert(GET_ROOT_PTR(test1), 
			      allocate_data(key),
			      item_cmp,
			      NULL, 
			      NULL,
			      allocate_data(key)));
      data = avl_find(GET_ROOT_PTR(test1), 
			allocate_data(key),
		      item_cmp, 
		      NULL);
      assert(data != NULL_PTR);
      assert(data[1] == key);
      SET_ROOT_PTR(test1, avl_delete(GET_ROOT_PTR(test1),
				     allocate_data(key),
				     item_cmp,
				     NULL, 
				     NULL,
				     allocate_data(key)));
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
  fprintf(stderr, "\nChoose AVL-operation that you want test:\n\n");
  fprintf(stderr, "0. `avl_insert'.\n");
  fprintf(stderr, "1. `avl_insert' and `avl_delete'.\n");
  fprintf(stderr, "Your choice is? ");
  c = getchar();
  if  ((c != '0') && (c != '1')) {
    fprintf(stderr, "\nOnly 0 and 1 are possible. ");
    return 0;
  }
  if (c == '0') {
    fprintf(stderr, "\nTesting avl_insert...\n");
    test_avl_insert();
  } 
  fprintf(stderr, "\nTesting avl_delete...\n");
  test_avl_delete();
  return 0;
}






