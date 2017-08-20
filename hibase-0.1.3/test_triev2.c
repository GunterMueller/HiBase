/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Sirkku-Liisa Paajanen <sirkku@iki.fi>
 */

/* Test/benchmark program for the trie routines.
 */

#include "includes.h"
#include "shades.h"
#include "triev2.h"
#include "root.h"
#include "test_aux.h"

static char *rev_id = "$Id: test_triev2.c,v 1.66 1998/02/18 11:03:32 sirkku Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

#define INSERT          '1'
#define DELETE          '2'
#define FIND_NONEXIST   '3'
#define FIND_AT_MOST    '4'

#define NUMBER_OF_KEYS  100

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


static void test_triev2(int operation)
{
  int ctr = 0, ctr_total = 0, i, n;
  word_t key = 0, max_key, key_in_range, check_key = 0;
  ptr_t data, check_data;

  SET_ROOT_PTR(test1, NULL_PTR);
  remove_history();
  
  if (!can_allocate(TRIEV2_MAX_ALLOCATION + 2)) {
    fprintf(stderr, "Can't allocate nest egg.\n");
    exit(1);
  }
  if (operation == INSERT) {
    while (1) {
      while (can_allocate(TRIEV2_MAX_ALLOCATION + 2)) {
	/* Check if the randomly chosen `key' is in the tree. */
	data = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key, 32));
	if (data == NULL_PTR)
	  assert(!is_in_history(key));
	else {
	  assert(is_in_history(key));
	  assert(data[1] == key);
	}
	
	/* Test `triev2_find_max' */
	key = maximum_in_history();
	max_key = triev2_find_max(GET_ROOT_PTR(test1), 32);
	if (key == -1)
	  assert(max_key == NULL_WORD);
	else
	  assert(max_key == key);
      
	key = random();
	/* Insert the key. */
	SET_ROOT_PTR(test1,
		     triev2_insert(GET_ROOT_PTR(test1), 
				   key, 
				   32,
				   NULL, NULL,
				   PTR_TO_WORD(allocate_data(key))));
	prepend_history(key); 
	ctr++;
	data = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key, 32));
	assert(data != NULL_PTR);
	assert(is_in_history(key));
	assert(data[1] == key);	
	triev2_make_assertions(GET_ROOT_PTR(test1), 32);
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
  } else if (operation == DELETE) {
    while (1) {
      while (can_allocate(TRIEV2_MAX_ALLOCATION + 2)) {
	/* Check if the randomly chosen `key' is in the tree. */
	key = random() % 100000;
	data = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key, 32));
	if (data == NULL_PTR)
	  assert(!is_in_history(key));
	else {
	  assert(is_in_history(key));
	  assert(data[1] == key);
	}

	/* Test `triev2_find_max' */
	key = maximum_in_history();
	max_key = triev2_find_max(GET_ROOT_PTR(test1), 32);
	if (key == -1)
	  assert(max_key == NULL_WORD);
	else
	  assert(max_key == key);
      
	key = random() % 100000;
	if (random() % 60 <= 30) {
	  /* Insert the key. */
	  SET_ROOT_PTR(test1,
		       triev2_insert(GET_ROOT_PTR(test1), 
				     key,
				     32,
				     NULL, NULL,
				     PTR_TO_WORD(allocate_data(key))));
	  prepend_history(key); 
	  data = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key, 32));
	  assert(data != NULL_PTR);
	  assert(is_in_history(key));
	  assert(data[1] == key);	
	} else {
	  /*	  fprintf(stderr, "\nDelete %12d 0x%x", key, key);*/
	  SET_ROOT_PTR(test1, triev2_delete(GET_ROOT_PTR(test1),
					    key,
					    32,
					    NULL, NULL,
					    NULL_WORD));
	  withdraw_history(key);
	  data = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key, 32));
	  assert(data == NULL_PTR);
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
  } else if (operation == FIND_NONEXIST) {     
    while (1) {
      while (can_allocate(TRIEV2_MAX_ALLOCATION + 2)) {

	key = 16 * random();
	key_in_range = 
	  triev2_find_nonexistent_key(GET_ROOT_PTR(test1), key, 32);
	if (key_in_range == 0xFFFFFFFFL) {
	  for (i = 0; i < 16; i++) {
	    data = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key + i, 32));
	    assert(data != NULL_PTR); 
	  }
	} else {
	  assert((key_in_range >= key) && (key_in_range <= key + 15));
	  data = 
	    WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key_in_range, 32));
	  assert(data == NULL_PTR);
	}
	
	key = random();
	/* Insert the key. */
	SET_ROOT_PTR(test1,
		     triev2_insert(GET_ROOT_PTR(test1), 
				   key,
				   32,
				   NULL, NULL,
				   PTR_TO_WORD(allocate_data(key))));
	prepend_history(key); 
	data = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key, 32));
	assert(data != NULL_PTR);
	assert(is_in_history(key));
	assert(data[1] == key);	
	triev2_make_assertions(GET_ROOT_PTR(test1), 32);
	ctr++;
      }
      flush_batch();
      ctr_total += ctr;
      fprintf(stderr, "\n[%d] insertions. Total [%d]", ctr, ctr_total);
      ctr = 0;
    }
  } else /* TRIE_FIND_AT_MOST */ {
    while (1) {
      while (can_allocate(TRIEV2_MAX_ALLOCATION + 2)) {
	/* Check if the randomly chosen `key' is in the tree. */
	data = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key, 32));
	if (data == NULL_PTR)
	  assert(!is_in_history(key));
	else {
	  assert(is_in_history(key));
	  assert(data[1] == key);
	}
	
	key = random();
	/* Insert the key. */
	SET_ROOT_PTR(test1,
		     triev2_insert(GET_ROOT_PTR(test1), 
				   key, 
				   32,
				   NULL, NULL,
				   PTR_TO_WORD(allocate_data(key))));
	prepend_history(key);
	triev2_make_assertions(GET_ROOT_PTR(test1), 32);
	ctr++;	
	
	n = random() % 10 + 1;
	for (i = 0; i < n; i++) {

	  key = random();
	  check_key = key;
	  data = WORD_TO_PTR(triev2_find_at_most(GET_ROOT_PTR(test1), 
						 &key, 32));
	  if (data == NULL_PTR) {
	    assert(triev2_find(GET_ROOT_PTR(test1), check_key, 32) 
		   == NULL_WORD);
	    assert(!is_in_history(check_key));
	    assert(avl_destr_find_at_most(insertion_history, &check_key) 
		   == 0);
	  } else {
	    assert(data[1] == key);
	    check_data = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), 
						 check_key, 
						 32));     
	    if (check_data == NULL_PTR) {
	      assert(!is_in_history(check_key));
	      assert(key != check_key);
	      assert(avl_destr_find_at_most(insertion_history, &check_key));
	      assert(key == check_key);
	    } else {
	      assert(check_data[1] == check_key);
	      assert(is_in_history(check_key));
	      assert(key == check_key);
	      assert(avl_destr_find_at_most(insertion_history, &check_key));
	      assert(key = check_key);
	    }
	  }
	}
      }
      if (GET_ROOT_PTR(test1) != NULL_PTR)
	assert(cell_check_rec(GET_ROOT_PTR(test1)));
      else 
	assert(is_empty_history());
      flush_batch();
      ctr_total += ctr;
      fprintf(stderr, "\n[%d] insertions. Total [%d].", ctr, ctr_total);
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

  srandom(0); 
  fprintf(stderr, "\nSeed of the random number generator is 0. Do you want to change it? Answer Y/N.");
  operation = getchar();
  if (operation == 'y' || operation == 'Y') {
    srandom(getpid()); 
    fprintf(stderr, "\nSeed is pid.\n");  
  }
  fprintf(stderr, "\nChoose the trie-operation that you want test:\n\n");
  fprintf(stderr, "1. `triev2_insert' and `triev2_find_max'.\n");
  fprintf(stderr, "2. `triev2_insert', `triev2_delete', and `triev2_find_max'.\n");
  fprintf(stderr, "3. `triev2_find_nonexistent_key'.\n");
  fprintf(stderr, "4. `triev2_find_at_most'.\n");  
  operation = getchar();
  while  ((operation < '1') || (operation > '4')) {
    operation = getchar();
  }
  switch (operation) {
  case INSERT:
    fprintf(stderr, "\nThis test function proceeds by inserting randomly "
	    "chosen keys into the trie. ^C stops the test...\n");
    test_triev2(INSERT);
  case DELETE:
    fprintf(stderr, "\nThis test function proceeds by randomly inserting and "
	    "deleting randomly chosen keys into the trie. "
	    "^C stops the test...\n");
    test_triev2(DELETE);  
  case FIND_NONEXIST:
    fprintf(stderr, "This test function tests `triev2_find_nonexistent_key'."
	    "^C stops the test...\n");
    test_triev2(FIND_NONEXIST);
  case FIND_AT_MOST:
    fprintf(stderr, "This test function tests `triev2_find_at_most'."
	    "^C stops the test...\n");
    test_triev2(FIND_AT_MOST);
  default:
    fprintf(stderr, "\nThat test-function hasn't made yet.\n");
  }
  return 0;
}



