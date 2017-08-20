/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 */

/* Test/benchmark program for dynamic hashing routines.
 */

#include "includes.h"
#include "shades.h"
#include "dh.h"
#include "root.h"
#include "test_aux.h"

#define DH_INSERT            '1'
#define DH_DELETE            '2'

/* A benchmark and a test program.  The test program is thorough, but
   SLOW, and the benchmark program is useful only for benchmarking not
   testing. */

static ptr_t allocate_data(word_t data)
{
  ptr_t result = allocate(2, CELL_word_vector);
  result[0] |= 1;
  result[1] = data;
  return result;
}

extern int scan_debug;

static void test_dh(int operation)
{
  int ctr = 0, ctr_total = 0, i;
  word_t key = 0, key2;
  ptr_t data, data2;

  SET_ROOT_PTR(test1, NULL_PTR);
  remove_history();
  
  if (!can_allocate(DH_MAX_ALLOCATION + 2)) {
    fprintf(stderr, "Can't allocate nest egg.\n");
    exit(1);
  }
  /* Put key + data = <0, 0> as a nest egg. */
  SET_ROOT_PTR(test1,
	       dh_insert(GET_ROOT_PTR(test1), 
			 0, NULL,
			 allocate_data(0)));
  prepend_history(0);
  ctr++;
  
  if (operation == DH_INSERT) {
/*fprintf(stderr, "\nkey == 0"); */
    while (1) {
      while (can_allocate(DH_MAX_ALLOCATION + 2)) {

	for (i = 0; i < 10; i++) {
	  key = random();
	  /* Check if the randomly chosen `key' is in the tree. */
	  data = dh_find(GET_ROOT_PTR(test1), key);
	  if (data == NULL_PTR)
	    assert(!is_in_history(key));
	  else {
	    assert(is_in_history(key));
	    assert(data[1] == key);
	  }
	}

	key = random();
	/* Insert the key. */
/*fprintf(stderr, "\nkey == %d", key);*/
	SET_ROOT_PTR(test1,
		     dh_insert(GET_ROOT_PTR(test1), key, NULL, allocate_data(key)));
	prepend_history(key); 
	ctr++;
	data = dh_find(GET_ROOT_PTR(test1), key);
	assert(data != NULL_PTR);
	assert(is_in_history(key));
	assert(data[1] == key);
	dh_make_assertions(GET_ROOT_PTR(test1), 0, 0); 
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
  } else if (operation == DH_DELETE) {
    while (1) {
      while (can_allocate(DH_MAX_ALLOCATION + 2)) {
	/* Check if the randomly chosen `key' is in the tree. */
	data = dh_find(GET_ROOT_PTR(test1), key);
	if (data == NULL_PTR)
	  assert(!is_in_history(key));
	else {
	  assert(is_in_history(key));
	  assert(data[1] == key);
	}
	key = random();
	if (random() % 60 <= 32) {
	  /* Insert the key. */
	  SET_ROOT_PTR(test1,
		       dh_insert(GET_ROOT_PTR(test1), 
				 key, 
				 NULL,
				 allocate_data(key)));
	  prepend_history(key); 
	} else {
	  SET_ROOT_PTR(test1, dh_delete(GET_ROOT_PTR(test1), key));
	  withdraw_history(key);
	} 
       	ctr++;
      }
      if (GET_ROOT_PTR(test1) != NULL_PTR)
	assert(cell_check_rec(GET_ROOT_PTR(test1)));
      else 
	assert(is_empty_history());
      flush_batch();
      fprintf(stderr, "\n[%d]", ctr);
      ctr = 0;
    }
  }
  return;
}

int main(int argc, char **argv)
{
  int operation;
  fprintf(stderr, "Initializing shades...");
  init_shades(argc, argv);
  fprintf(stderr, "done\n");
  fprintf(stderr, "Creating database...");
  create_db();
  fprintf(stderr, "done\n");
  srandom(getpid()); 
  /*srandom(0);*/
  fprintf(stderr, "seed = %d\n", getpid()); 
  fprintf(stderr, "\nChoose the dynamic hash operation that you want test:\n\n");
  fprintf(stderr, "1. `dh_insert'.\n");
  fprintf(stderr, "2. `dh_insert' and `dh_delete'.\n");
  fprintf(stderr, "Your choice is? ");
  operation = getchar();
  while  ((operation < '1') || (operation > '2')) {
    fprintf(stderr, "\nYour choice is (1 - 2)? ");
    operation = getchar();
  }
  switch (operation) {
  case DH_INSERT:
    fprintf(stderr, "\nThis test function proceeds by inserting randomly chosen keys. ^C stops the test...\n");
    test_dh(DH_INSERT);
  case DH_DELETE:
    fprintf(stderr, "\nThat test-function hasn't made yet.\n");
  }
  return 0;
}



