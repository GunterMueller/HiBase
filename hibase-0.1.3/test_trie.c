/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Sirkku-Liisa Paajanen <sirkku@cs.hut.fi>
 */

/* Test/benchmark program for the trie routines.
 */

#include "includes.h"
#include "shades.h"
#include "trie.h"
#include "root.h"
#include "test_aux.h"

static char *rev_id = "$Id: test_trie.c,v 1.143 1997/05/15 13:33:35 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

#define TRIE_INSERT          '1'
#define TRIE_FIND_AT_LEAST   '2'
#define TRIE_DELETE          '3'
#define TRIE_INSERT_SET      '4'
#define TRIE_DELETE_SET      '5'
#define NUMBER_OF_KEYS       100

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

static void test_trie(int operation)
{
  int ctr = 0, ctr_total = 0, res;
  word_t key = 0, key2;
  ptr_t data, data2;

  SET_ROOT_PTR(test1, NULL_PTR);
  remove_history();
  
  if (!can_allocate(TRIE_MAX_ALLOCATION + 2)) {
    fprintf(stderr, "Can't allocate nest egg.\n");
    exit(1);
  }
  /* Put key + data = <0, 0> as a nest egg. */
  SET_ROOT_PTR(test1,
	       trie_insert(GET_ROOT_PTR(test1), 
			   0, 
			   NULL, 
			   allocate_data(0)));
  prepend_history(0);
  ctr++;
  
  if (operation == TRIE_INSERT) {
    while (1) {
      while (can_allocate(TRIE_MAX_ALLOCATION + 2)) {
	/* Check if the randomly chosen `key' is in the tree. */
	data = trie_find(GET_ROOT_PTR(test1), key);
	if (data == NULL_PTR)
	  assert(!is_in_history(key));
	else {
	  assert(is_in_history(key));
	  assert(data[1] == key);
	}
	
	key = random();
	/* Insert the key. */
	SET_ROOT_PTR(test1,
		     trie_insert(GET_ROOT_PTR(test1), 
				 key, 
				 NULL,
				 allocate_data(key)));
	prepend_history(key); 
	ctr++;
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
  } else if (operation == TRIE_DELETE) {
    while (1) {
      while (can_allocate(TRIE_MAX_ALLOCATION + 2)) {
	/* Check if the randomly chosen `key' is in the tree. */
	data = trie_find(GET_ROOT_PTR(test1), key);
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
		       trie_insert(GET_ROOT_PTR(test1), 
				   key, 
				   NULL,
				   allocate_data(key)));
	  prepend_history(key); 
	} else {
	  SET_ROOT_PTR(test1, trie_delete(GET_ROOT_PTR(test1),
					  key, NULL, NULL_PTR));
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
  } else /* TRIE_FIND_AT_LEAST */ {
    while (1) {
      while (can_allocate(TRIE_MAX_ALLOCATION + 2)) {
	/* Check if the randomly chosen `key' is in the tree. */
	data = trie_find(GET_ROOT_PTR(test1), key);
	if (data == NULL_PTR)
	  assert(!is_in_history(key));
	else {
	  assert(is_in_history(key));
	  assert(data[1] == key);
	}
	
	key = random();
	/* Insert the key. */
	SET_ROOT_PTR(test1,
		     trie_insert(GET_ROOT_PTR(test1), 
				 key, 
				 NULL,
				 allocate_data(key)));
	prepend_history(key);
 
	key = random();
	key2 = key;
	data = trie_find_at_least(GET_ROOT_PTR(test1), &key);
	res = avl_destr_find_at_least(insertion_history, &key2);
	if (data == NULL_PTR)
	  assert(!res);
	else {
	  data2 = trie_find(GET_ROOT_PTR(test1), key2);
	  assert(data2 != NULL_PTR);
	  assert(data2[1] == key2);
	  assert(data[1] == key);
	  assert(key == key2);
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

static void test_set_operations(int operation)
{
  int ctr = 0, i, max;
  word_t key = 0, key_vector[NUMBER_OF_KEYS];   
  ptr_t data, data_table[NUMBER_OF_KEYS];
  
  SET_ROOT_PTR(test1, NULL_PTR);
  remove_history();

  if (operation == TRIE_INSERT_SET) {
    while (1) {
      max = random() % 10000;
      while (can_allocate(2 * (NUMBER_OF_KEYS + max + 1)
			  * (TRIE_MAX_ALLOCATION + 2))) {
	/* Insert `max' + 1 keys to the trie. */
	SET_ROOT_PTR(test1,
		     trie_insert(GET_ROOT_PTR(test1), 
				 0, 
				 NULL, 
				 allocate_data(0)));
	prepend_history(0);
	for (i = 0; i < max; i++) {
	  key = random();
	  /* Check if the randomly chosen `key' is in the tree. */
	  data = trie_find(GET_ROOT_PTR(test1), key);
	  if (data == NULL_PTR)
	    assert(!is_in_history(key));
	  else {
	    assert(is_in_history(key));
	    assert(data[1] == key);
	  }
	  /* Insert the key. */
	  SET_ROOT_PTR(test1,
		       trie_insert(GET_ROOT_PTR(test1), 
				   key, 
				   NULL,
				   allocate_data(key)));
	  prepend_history(key);     
	}

	if (GET_ROOT_PTR(test1) != NULL_PTR)
	  assert(cell_check_rec(GET_ROOT_PTR(test1)));
	else 
	  assert(is_empty_history());

	/* Fill a table with randomly chosen keys. */
	key_vector[0] = random() % 1000;
	data_table[0] = allocate_data(key_vector[0]);
	for (i = 1; i < NUMBER_OF_KEYS; i++) {
	  key_vector[i] = (key_vector[i - 1] + i + random() % 1000) 
	    % 0xFFFFFFFF;
	  data_table[i] = allocate_data(key_vector[i]);
	}
	/* Insert keys in the table in the trie. */
	SET_ROOT_PTR(test1, trie_insert_set(GET_ROOT_PTR(test1), 
					    key_vector,
					    data_table,
					    NUMBER_OF_KEYS));
        for (i = 0; i < NUMBER_OF_KEYS; i++)
	  prepend_history(key_vector[i]);
	
	if (GET_ROOT_PTR(test1) != NULL_PTR)
	  assert(cell_check_rec(GET_ROOT_PTR(test1)));
	else 
	  assert(is_empty_history());
	
	/* Delete all the keys in the trie. */
	while (avl_destr_find_min(insertion_history, &key)) {
	  withdraw_history(key);
	  SET_ROOT_PTR(test1, trie_delete(GET_ROOT_PTR(test1),
					  key, NULL, NULL_PTR));
	}
	ctr++;
	assert(GET_ROOT_PTR(test1) == NULL_PTR);
      }
      flush_batch();
      fprintf(stderr, "\n[%d]", ctr);
      ctr = 0;
    }
  } else {
    while (1) {
      max = random() % 10000;
      while (can_allocate((NUMBER_OF_KEYS + 2 * (max + 1))
			  * (TRIE_MAX_ALLOCATION + 2))) {
	/* Insert `max' + 1 keys to the trie. */
	SET_ROOT_PTR(test1,
		     trie_insert(GET_ROOT_PTR(test1), 
				 0, 
				 NULL, 
				 allocate_data(0)));
	prepend_history(0);
	for (i = 0; i < max; i++) {
	  key = random();
	  /* Check if the randomly chosen `key' is in the tree. */
	  data = trie_find(GET_ROOT_PTR(test1), key);
	  if (data == NULL_PTR)
	    assert(!is_in_history(key));
	  else {
	    assert(is_in_history(key));
	    assert(data[1] == key);
	  }
	  /* Insert the key. */
	  SET_ROOT_PTR(test1,
		       trie_insert(GET_ROOT_PTR(test1), 
				   key, 
				   NULL,
				   allocate_data(key)));
	  prepend_history(key);     
	}
      
	if (GET_ROOT_PTR(test1) != NULL_PTR)
	  assert(cell_check_rec(GET_ROOT_PTR(test1)));
	else 
	  assert(is_empty_history());

	/* Fill a table with randomly chosen keys. */	
	key_vector[0] = random() % 1000;
	for (i = 1; i < NUMBER_OF_KEYS; i++) 
	  key_vector[i] = (key_vector[i - 1] + i + random() % 1000) 
	    % 0xFFFFFFFF;
	/* Delete keys in the `key_vector' from the trie. */
	SET_ROOT_PTR(test1, trie_delete_set(GET_ROOT_PTR(test1), 
					    key_vector,
					    NUMBER_OF_KEYS));
	for (i = 0; i < NUMBER_OF_KEYS; i++)
	  withdraw_history(key_vector[i]);

	if (GET_ROOT_PTR(test1) != NULL_PTR)
	  assert(cell_check_rec(GET_ROOT_PTR(test1)));
	else 
	  assert(is_empty_history());
	
	/* Delete all the keys in the trie. */
	while (avl_destr_find_min(insertion_history, &key)) {
	  withdraw_history(key);
	  assert(GET_ROOT_PTR(test1) != NULL_PTR);
	  SET_ROOT_PTR(test1, trie_delete(GET_ROOT_PTR(test1),
					  key, NULL, NULL_PTR));
	}
	assert(GET_ROOT_PTR(test1) == NULL_PTR);
	ctr++;
      }
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
  fprintf(stderr, "seed = %d\n", getpid());
  fprintf(stderr, "\nChoose the trie-operation that you want test:\n\n");
  fprintf(stderr, "1. `trie_insert'.\n");
  fprintf(stderr, "2. `trie_find_at_least'.\n");
  fprintf(stderr, "3. `trie_insert' and `trie_delete'.\n");
  fprintf(stderr, "4. `trie_insert_set'.\n");
  fprintf(stderr, "5. `trie_delete_set'.\n\n");
  fprintf(stderr, "Your choice is? ");
  operation = getchar();
  while  ((operation < '1') || (operation > '6')) {
    fprintf(stderr, "\nYour choice is (1 - 6)? ");
    operation = getchar();
  }
  switch (operation) {
  case TRIE_INSERT:
    fprintf(stderr, "\nThis test function proceeds by inserting randomly chosen keys into the trie. ^C stops the test...\n");
    test_trie(TRIE_INSERT);
  case TRIE_FIND_AT_LEAST:
    fprintf(stderr, "\nTesting `trie_find_at_least'-operation. ^C stops the test...\n");
    test_trie(TRIE_FIND_AT_LEAST);
  case TRIE_DELETE:
    fprintf(stderr, "\nThis test function proceeds by randomly inserting and deleting randomly chosen keys into the trie. ^C stops the test...\n");
    test_trie(TRIE_DELETE);
  case TRIE_INSERT_SET:
    fprintf(stderr, "\nThis test function proceeds by inserting ordered sets (size %d) of keys. The keys are chosen randomly. ^C stops the test...\n", NUMBER_OF_KEYS);
    test_set_operations(TRIE_INSERT_SET);
  case TRIE_DELETE_SET:
    fprintf(stderr, "\nThis test function proceeds by deleting ordered sets (size %d) of keys. The keys are chosen randomly. ^C stops the test...\n", NUMBER_OF_KEYS);
    test_set_operations(TRIE_DELETE_SET);
  default:
    fprintf(stderr, "\nThat test-function hasn't made yet.\n");
  }
  return 0;
}



