/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Kengatharan Sivalingam <siva@iki.fi>
 */

/* Performance test for the access methods of Shades.
 */

#include "includes.h"
#include "root.h"
#include "test_aux.h"
#include <time.h>

/* Header files for the access methods */
#include "trie.h"
#include "triev2.h"
/* Disabled until the KDQ becomes public.
   #include "kdqtrie.h" */
#include "queue.h"
#include "lq.h"
#include "avl.h"
#include "ist234.h"


static char *rev_id = "$Id: test_access_method.c,v 1.58 1998/02/25 13:36:47 sirkku Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

typedef enum access_method_name_t {
  TRIE, TRIEV2, KDQTRIE, AVL, IST234, QUEUE, LQ
} access_method_name_t;

typedef enum test_type_t {
  INSERT, DELETE, PERFORM
} test_type_t;

#define REC_SIZE                1
#define KDQT_ARITY              1

long number_of_keys;
long test_max_allocation;

int warming_in_seconds;
int duration_in_seconds;

int show_progress;
int scatter_key;
int last_trie_was_test2;

access_method_name_t test_name;
test_type_t test_type;

static void make_db(void);
static void do_test(void);
static void do_insert();
static void do_delete();
static void do_perform(long, long);


int main(int argc, char *argv[])
{
  int i;
  
  if (argc < 3) {
  usage:
    fprintf(stderr, 
	    "Usage: %s number_of_keys [db_warming_time+]duration \
[--show-progress] [--scatter-key] [params]\n", 
	    argv[0]);
    exit(1);
  }
  
  number_of_keys = atol(argv[1]);

  if ((strchr(argv[2], '+'))) {
    if (sscanf(argv[2], "%d+%d", &warming_in_seconds, 
	       &duration_in_seconds) != 2)
      goto usage;
  }
  else
    duration_in_seconds = atoi(argv[2]);

  argv[1] = NULL;
  argv[2] = NULL;
  
  /* Read other arguments given exclusively for `test_access_method'. */
  for (i = 3; i < argc; i++) {
    if (argv[i] == NULL)
      continue;
    if (strcmp("--show-progress", argv[i]) == 0) {
      show_progress = 1;
      argv[i] = NULL;
    } else if (strcmp("--scatter-key", argv[i]) == 0) {
      scatter_key = 1;
      argv[i] = NULL;
    }
  }

  /* This should be called before using any Shades parameters. */
  init_shades(argc, argv);

  /* Get the test type. */
  if (strcmp(test_am_type, "insert") == 0)
    test_type = INSERT;
  else if (strcmp(test_am_type, "delete") == 0)
    test_type = DELETE;
  else if (strcmp(test_am_type, "perform") == 0)
    test_type = PERFORM;
  else {
    fprintf(stderr, "### Unknown test type \"%s\" ###\n", 
	    test_am_type);
    exit(1);
  }

  /* Get the access method name. */
  if (strcmp(test_am_name, "trie") == 0) {
    test_name = TRIE;
    test_max_allocation = TRIE_MAX_ALLOCATION;
  }
  else if (strcmp(test_am_name, "triev2") == 0) {
    test_name = TRIEV2;
    test_max_allocation = TRIEV2_MAX_ALLOCATION;
  }
#if 0
  /* Disabled until the KDQ becomes public. */
  else if (strcmp(test_am_name, "kdqtrie") == 0) {
    test_name = KDQTRIE;
    test_max_allocation = KDQTRIE_MAX_ALLOCATION(KDQT_ARITY);
  }
#endif
  else if (strcmp(test_am_name, "queue") == 0) {
    test_name = QUEUE;
    test_max_allocation = QUEUE_MAX_ALLOCATION;
  }
  else if (strcmp(test_am_name, "lq") == 0) {
    test_name = LQ;
    test_max_allocation = 
  LQ_INSERT_MAX_ALLOCATION > LQ_REMOVE_MAX_ALLOCATION ?
  LQ_INSERT_MAX_ALLOCATION : LQ_REMOVE_MAX_ALLOCATION;
  }
  else if (strcmp(test_am_name, "avl") == 0) {
    test_name = AVL;
    test_max_allocation = 
  AVL_MAX_ALLOCATION_IN_INSERT > AVL_MAX_ALLOCATION_IN_DELETE ?
  AVL_MAX_ALLOCATION_IN_INSERT : AVL_MAX_ALLOCATION_IN_DELETE;
  }
  else if (strcmp(test_am_name, "ist234") == 0) {
    test_name = IST234;
    test_max_allocation = 
  IST234_MAX_ALLOCATION_IN_INSERT > IST234_MAX_ALLOCATION_IN_DELETE ?
  IST234_MAX_ALLOCATION_IN_INSERT : IST234_MAX_ALLOCATION_IN_DELETE;
  }
  else {
    fprintf(stderr, "### Unknown access method \"%s\" ###\n", 
	    test_am_name);
    exit(1);
  }

  make_db();

  if (duration_in_seconds > 0) {
    do_test();
    flush_batch();
  }
  return 0;
}

/* Comparison function for `avl' and `ist234'. */
static cmp_result_t item_cmp(ptr_t p, ptr_t q, void *context)
{
  if (context != NULL)
    return CMP_ERROR;

  assert(p != NULL_PTR && q != NULL_PTR);
  assert(CELL_TYPE(p) == CELL_word_vector);
  assert(CELL_TYPE(q) == CELL_word_vector);
  assert((p[0] & 0xFFFFFF) == (q[0] & 0xFFFFFF));
  assert((p[0] & 0xFFFFFF) > 0);

  if (p[1] < q[1])
    return CMP_LESS;
  else if (p[1] > q[1])
    return CMP_GREATER;
  return CMP_EQUAL;
}

/* Create database and insert keys. */ 
static void make_db()
{
  ptr_t rec;
  word_t i, key, total_keys;

  create_db();

  if (test_type == DELETE)
    total_keys = number_of_keys + test_am_key_set_size;
  else 
    total_keys = number_of_keys;

  for (i = 0; i < total_keys; i++) {
    while (!can_allocate(test_max_allocation + REC_SIZE + 1))
      flush_batch();
    
    key = scatter_key ? scatter(i) : i;
    
    rec = allocate(1 + REC_SIZE, CELL_word_vector);
    rec[0] |= REC_SIZE;
    rec[1] = key;
    
    switch (test_name) {
    case TRIE:
      SET_ROOT_PTR(test1,
		   trie_insert(GET_ROOT_PTR(test1), key, NULL, rec));
      break;

    case TRIEV2:
      SET_ROOT_PTR(test1,
		   triev2_insert(GET_ROOT_PTR(test1), key, 32, NULL, NULL,
				 PTR_TO_WORD(rec)));
      break;

#if 0
      /* Disabled until KDQ becomes public */
    case KDQTRIE:
      SET_ROOT_PTR(test1,
		   kdqtrie_insert(GET_ROOT_PTR(test1), rec, NULL, 
				  KDQT_ARITY));
      break;
#endif
    
    case QUEUE:
      SET_ROOT_PTR(test1,
		   queue_insert_last(GET_ROOT_PTR(test1), rec));
      break;     
    case LQ:
      SET_ROOT_PTR(test1,
		   lq_insert_last(GET_ROOT_PTR(test1), rec));
      break;
    case AVL:
      SET_ROOT_PTR(test1,
		   avl_insert(GET_ROOT_PTR(test1), rec, item_cmp, 
			      NULL, NULL, rec));
      break;

    case IST234:
      SET_ROOT_PTR(test1,
		   ist234_insert(GET_ROOT_PTR(test1), rec, item_cmp, 
				 NULL, NULL, rec));
      break;

    default:
      assert(0);
    }
  }

  flush_batch();
}

/* Test driver */
static void do_test()
{
  double start_time, end_time, total_runtime = 0;
  long tested_keys = 0, insert_key, delete_key, dummy = 0;

  insert_key = number_of_keys;
  delete_key = 0;

  while(1) {
    start_time = give_time();

    switch (test_type) {
    case INSERT:
      do_insert();
      break;

    case DELETE:
      do_delete();
      break;

    case PERFORM:
      do_perform(insert_key, delete_key);
      break;

    default:
      assert(0);
    }

    end_time = give_time();
    total_runtime += end_time - start_time;

    if (test_type == PERFORM) {
      insert_key += test_am_key_set_size;
      delete_key += test_am_key_set_size;
    }

    if (warming_in_seconds > 0) {
      if (total_runtime < warming_in_seconds) {
	if (be_verbose || show_progress)
	  fprintf(stderr,
		  " Warming-up...  %.0f seconds                       %c",
		  total_runtime, 13);
	
	continue;
      }
      else {
	warming_in_seconds = 0;
	total_runtime = 0;
	continue;
      }
    }

    tested_keys += test_am_key_set_size;
    if (be_verbose || show_progress)
      fprintf(stderr, 
	      " %8ld                                                 %c",
	      tested_keys, 13);
    
    if (total_runtime >= duration_in_seconds)
      break;
  }

  if (total_runtime > 0)
  fprintf(stdout, "@@@ Total transactions: %ld & TPS: %.2f @@@\n", 
	  dummy, (float) tested_keys / total_runtime);
}

/* Insertion performance */
static void do_insert()
{
  ptr_t rec;
  word_t i, key;
  
  if (last_trie_was_test2)
    SET_ROOT_PTR(test3, GET_ROOT_PTR(test1));
  else
    SET_ROOT_PTR(test2, GET_ROOT_PTR(test1));
  
  for (i = 0; i < test_am_key_set_size; i++) {
    while (!can_allocate(test_max_allocation + REC_SIZE + 1))
      flush_batch();

    key = scatter_key ? scatter(i + number_of_keys) : i + number_of_keys;
    
    rec = allocate(1 + REC_SIZE, CELL_word_vector);
    rec[0] |= REC_SIZE;
    rec[1] = key;
    
    switch (test_name) {
    case TRIE:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     trie_insert(GET_ROOT_PTR(test3), key, NULL, rec));
      else 
	SET_ROOT_PTR(test2,
		     trie_insert(GET_ROOT_PTR(test2), key, NULL, rec));
      break;

    case TRIEV2:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		   triev2_insert(GET_ROOT_PTR(test3),
				 key, 32, NULL, NULL, PTR_TO_WORD(rec)));
      else 
	SET_ROOT_PTR(test2,
		     triev2_insert(GET_ROOT_PTR(test2),
				   key, 32, NULL, NULL, PTR_TO_WORD(rec)));
      break;

#if 0
      /* Disabled until KDQ becomes public. */
    case KDQTRIE:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     kdqtrie_insert(GET_ROOT_PTR(test3), 
				    rec, NULL, KDQT_ARITY));
      else 
	SET_ROOT_PTR(test2,
		     kdqtrie_insert(GET_ROOT_PTR(test2), 
				    rec, NULL, KDQT_ARITY));
      break;
#endif
    
    case QUEUE:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3, 
		     queue_insert_last(GET_ROOT_PTR(test3), rec));
      else 
	SET_ROOT_PTR(test2, 
		     queue_insert_last(GET_ROOT_PTR(test2), rec));
      break;
#if 0      
    case LQ:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3, 
		     lq_insert_last(GET_ROOT_PTR(test3), rec));
      else 
	SET_ROOT_PTR(test2, 
		     lq_insert_last(GET_ROOT_PTR(test2), rec));
      break;
#endif
    case AVL:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     avl_insert(GET_ROOT_PTR(test3),
				rec, item_cmp, NULL, NULL, rec));
      else 
	SET_ROOT_PTR(test2,
		     avl_insert(GET_ROOT_PTR(test2),
				rec, item_cmp, NULL, NULL, rec));
      break;

    case IST234:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     ist234_insert(GET_ROOT_PTR(test3),
				   rec, item_cmp, NULL, NULL, rec));
      else 
	SET_ROOT_PTR(test2,
		     ist234_insert(GET_ROOT_PTR(test2),
				   rec, item_cmp, NULL, NULL, rec));
      break;

    default:
      assert(0);
    }
  }

  if (last_trie_was_test2)
    last_trie_was_test2 = 0;
  else
    last_trie_was_test2 = 1;
}

/* Deletion performance */
static void do_delete()
{
  ptr_t rec;
  word_t i, key;
  
  if (last_trie_was_test2)
    SET_ROOT_PTR(test3, GET_ROOT_PTR(test1));
  else
    SET_ROOT_PTR(test2, GET_ROOT_PTR(test1));

  for (i = 0; i < test_am_key_set_size; i++) {
    while (!can_allocate(test_max_allocation + REC_SIZE + 1))
      flush_batch();

    key = scatter_key ? scatter(i + number_of_keys) : i + number_of_keys;
    
    switch (test_name) {
    case TRIE:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     trie_delete(GET_ROOT_PTR(test3), key, NULL, NULL_PTR));
      else 
	SET_ROOT_PTR(test2,
		     trie_delete(GET_ROOT_PTR(test2), key, NULL, NULL_PTR));
      break;

    case TRIEV2:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     triev2_delete(GET_ROOT_PTR(test3), key, 32, NULL, NULL,
				   NULL_WORD));
      else 
	SET_ROOT_PTR(test2,
		     triev2_delete(GET_ROOT_PTR(test2), key, 32, NULL, NULL,
				   NULL_WORD));
      break;

#if 0
      /* Disabled until KDQ becomes public */
    case KDQTRIE:
      /* Not sure whether this is correct, ask JP! XXX */
      rec = allocate(1 + REC_SIZE, CELL_word_vector);
      rec[0] |= REC_SIZE;
      rec[1] = key;

      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     kdqtrie_delete(GET_ROOT_PTR(test3), rec, KDQT_ARITY));
      else 
	SET_ROOT_PTR(test2,
		   kdqtrie_delete(GET_ROOT_PTR(test2), rec, KDQT_ARITY));
      break;
#endif
    
    case QUEUE:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     queue_remove_first(GET_ROOT_PTR(test3)));
      else 
	SET_ROOT_PTR(test2,
		     queue_remove_first(GET_ROOT_PTR(test2)));

      break;
#if 0      
    case LQ:
      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     lq_remove_first(GET_ROOT_PTR(test3)));
      else 
	SET_ROOT_PTR(test2,
		     lq_remove_first(GET_ROOT_PTR(test2)));

      break;
#endif
    case AVL:
      rec = allocate(1 + REC_SIZE, CELL_word_vector);
      rec[0] |= REC_SIZE;
      rec[1] = key;

      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     avl_delete(GET_ROOT_PTR(test3), rec, item_cmp, 
				NULL, NULL, rec));
      else 
	SET_ROOT_PTR(test2,
		     avl_delete(GET_ROOT_PTR(test2), rec, item_cmp, 
				NULL, NULL, rec));
      break;

    case IST234:
      rec = allocate(1 + REC_SIZE, CELL_word_vector);
      rec[0] |= REC_SIZE;
      rec[1] = key;

      if (last_trie_was_test2)
	SET_ROOT_PTR(test3,
		     ist234_delete(GET_ROOT_PTR(test3), rec, item_cmp, 
				   NULL, NULL, rec));
      else
	SET_ROOT_PTR(test2,
		     ist234_delete(GET_ROOT_PTR(test2), rec, item_cmp, 
				   NULL, NULL, rec));
      break;

    default:
      assert(0);
    }
  }

  if (last_trie_was_test2)
    last_trie_was_test2 = 0;
  else
    last_trie_was_test2 = 1;
}

/* Performance test: Insert one and delete one. */
static void do_perform(long first_insert_key, long first_delete_key)
{
  ptr_t rec_insert, rec_delete;
  word_t i, key_insert, key_delete;
  
  for (i = 0; i < test_am_key_set_size; i++) {
    while (!can_allocate( 2 * (test_max_allocation + REC_SIZE + 1)))
      flush_batch();

    key_insert = scatter_key ? scatter(i + first_insert_key) : 
      i + first_insert_key;
    key_delete = scatter_key ? scatter(i + first_delete_key) : 
      i + first_delete_key;
    
    rec_insert = allocate(1 + REC_SIZE, CELL_word_vector);
    rec_insert[0] |= REC_SIZE;
    rec_insert[1] = key_insert;
    
    switch (test_name) {
    case TRIE:
      SET_ROOT_PTR(test1,
		   trie_insert(GET_ROOT_PTR(test1), key_insert, NULL, 
			       rec_insert));
      SET_ROOT_PTR(test1,
		   trie_delete(GET_ROOT_PTR(test1), key_delete, NULL, 
			       NULL_PTR));
      break;

    case TRIEV2:
      SET_ROOT_PTR(test1,
		   triev2_insert(GET_ROOT_PTR(test1), key_insert, 32, NULL, 
				 NULL, PTR_TO_WORD(rec_insert)));
      SET_ROOT_PTR(test1,
		   triev2_delete(GET_ROOT_PTR(test1), key_insert, 32, NULL, 
				 NULL, NULL_WORD));
      break;

#if 0
      /* Disabled until KDQ becomes public. */
    case KDQTRIE:
      SET_ROOT_PTR(test1,
		   kdqtrie_insert(GET_ROOT_PTR(test1), rec_insert, NULL, 
				  KDQT_ARITY));

      /* Not sure whether this is correct, ask JP! XXX */
      rec_delete = allocate(1 + REC_SIZE, CELL_word_vector);
      rec_delete[0] |= REC_SIZE;
      rec_delete[1] = key_delete;

      SET_ROOT_PTR(test1,
		   kdqtrie_delete(GET_ROOT_PTR(test1), rec_delete, 
				  KDQT_ARITY));
      break;
#endif
    
    case QUEUE:
      SET_ROOT_PTR(test1,
		   queue_insert_last(GET_ROOT_PTR(test1), rec_insert));
      SET_ROOT_PTR(test1,
		   queue_remove_first(GET_ROOT_PTR(test1)));
      break;
#if 0      
    case LQ:
      SET_ROOT_PTR(test1,
		   lq_insert_last(GET_ROOT_PTR(test1), rec_insert));
      SET_ROOT_PTR(test1,
		   lq_remove_first(GET_ROOT_PTR(test1)));
      break;
#endif
    case AVL:
      SET_ROOT_PTR(test1,
		   avl_insert(GET_ROOT_PTR(test1), rec_insert, item_cmp, 
			      NULL, NULL, rec_insert));

      rec_delete = allocate(1 + REC_SIZE, CELL_word_vector);
      rec_delete[0] |= REC_SIZE;
      rec_delete[1] = key_delete;
      SET_ROOT_PTR(test1,
		   avl_delete(GET_ROOT_PTR(test1), rec_delete, item_cmp, 
			      NULL, NULL, rec_delete));
      break;

    case IST234:
      SET_ROOT_PTR(test1,
		   ist234_insert(GET_ROOT_PTR(test1), rec_insert, item_cmp, 
				 NULL, NULL, rec_insert));

      rec_delete = allocate(1 + REC_SIZE, CELL_word_vector);
      rec_delete[0] |= REC_SIZE;
      rec_delete[1] = key_delete;
      SET_ROOT_PTR(test1,
		   ist234_delete(GET_ROOT_PTR(test1), rec_delete, item_cmp, 
				 NULL, NULL, rec_delete));
      break;

    default:
      assert(0);
    }
  }
}
