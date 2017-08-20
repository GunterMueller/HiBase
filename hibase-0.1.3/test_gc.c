/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Test/benchmark program the (mature) garbage collector. */

#include "includes.h"
#include "triev2.h"
#include "root.h"
#include "test_aux.h"

static char *rev_id = "$Id: test_gc.c,v 1.17 1997/09/18 12:40:13 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


static void insert(word_t key)
{
  ptr_t data;

  while (!can_allocate(TRIEV2_MAX_ALLOCATION + 2))
    flush_batch();

  data = allocate(2, CELL_word_vector);
  data[0] |= 1;
  data[1] = key;
  SET_ROOT_PTR(test1, 
	       triev2_insert(GET_ROOT_PTR(test1),
			     key, 32, NULL, NULL,
			     PTR_TO_WORD(data)));
}

static void version(word_t key)
{
  assert(can_allocate(TRIEV2_MAX_ALLOCATION));
  SET_ROOT_PTR(test2,
	       triev2_insert(GET_ROOT_PTR(test2), 
			     key,
			     32,
			     NULL, NULL,
			     GET_ROOT_WORD(test1)));
}

static void swap(word_t key1, word_t key2)
{
  ptr_t data1, data2;

  while (!can_allocate(2 * TRIEV2_MAX_ALLOCATION))
    flush_batch();

  data1 = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key1, 32));
  data2 = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test1), key2, 32));
  assert(data1 != NULL_PTR);
  assert(data2 != NULL_PTR);
  assert(data1[1] != data2[1]);
  assert(data1 != data2);
  SET_ROOT_PTR(test1, 
	       triev2_insert(GET_ROOT_PTR(test1),
			     key1, 32, NULL, NULL,
			     PTR_TO_WORD(data2)));
  SET_ROOT_PTR(test1, 
	       triev2_insert(GET_ROOT_PTR(test1), 
			     key2, 32, NULL, NULL,
			     PTR_TO_WORD(data1)));
}

static zipf_t *z;
static int number_of_keys, updates_rotate, updates_swap, updates_dag;
static int updates_scatter;

static void update(void)
{
  static word_t key = 0;
  word_t key1, key2, key3;

  if (updates_rotate) {
    key = (key+1) % number_of_keys;
    insert(key);
  } else if (updates_swap) {
    key1 = random() % number_of_keys;
    do {
      key2 = random() % number_of_keys;
    } while (key2 == key1);
    swap(key1, key2);
  } else if (updates_dag) {
    while (!can_allocate(3 * TRIEV2_MAX_ALLOCATION))
      flush_batch();
    key1 = random() % number_of_keys;
    do {
      key2 = random() % number_of_keys;
    } while (key2 == key1);
    swap(key1, key2);
    key3 = zipf(z);
    version(key3);
  } else if (updates_scatter) {
    key = scatter(random() % number_of_keys);
    insert(key);
  } else
    insert(zipf(z));
}

extern long number_of_written_pages;
extern long number_of_major_gc_written_pages;
#ifdef GC_PROFILING
extern unsigned long max_rem_set_size;
#endif

int main(int argc, char **argv)
{
  double skewedness, start_time;
  int i;
  word_t key;

  /* Parse arguments. */
  if (argc < 3) {
    fprintf(stderr, 
	    "Usage: %s number_of_keys [skewedness | --rotate | --static | --dag | --scatter] [params]*\n", 
	    argv[0]);
    exit(1);
  }
  number_of_keys = atoi(argv[1]);
  argv[1] = NULL;
  updates_rotate = updates_swap = updates_dag = 0;
  if (strcmp(argv[2], "--rotate") == 0)
    updates_rotate = 1;
  else if (strcmp(argv[2], "--static") == 0)
    updates_swap = 1;
  else if (strcmp(argv[2], "--dag") == 0) {
    updates_dag = 1;
    z = zipf_init(256, 0.8);
  } else if (strcmp(argv[2], "--scatter") == 0) {
    updates_scatter = 1;
  } else {
    skewedness = atof(argv[2]);
    z = zipf_init(number_of_keys, skewedness);
  }
  argv[2] = NULL;

  /* Initialize the system. */
  srandom(0);
  init_shades(argc, argv);
  create_db();

  fprintf(stderr, "\t\tPREFILLING\n");
  for (key = 0; key < number_of_keys; key++)
    if (updates_scatter)
      insert(scatter(key));
    else
      insert(key);

  fprintf(stderr, "\t\tHEATING UP\n");
  for (i = 0; i < db_size / 8; i++)
    update();

  start_time = give_time();
  number_of_written_pages = number_of_major_gc_written_pages = 0;
#ifdef GC_PROFILING
  max_rem_set_size = 0;
#endif
  fprintf(stderr, "\t\tBENCHMARKING%c\n", 7);
  for (i = 0; i < db_size / 8; i++)
    update();

  /* Report results. */
#ifdef GC_PROFILING
  fprintf(stderr, "%d\t%d\t%d\t%d\n",
	  number_of_keys, 
	  number_of_written_pages,
	  number_of_major_gc_written_pages,
	  max_rem_set_size);
  printf("%d\t%d\t%d\t%d\n",
	 number_of_keys, 
	 number_of_written_pages,
	 number_of_major_gc_written_pages,
	 max_rem_set_size);
#else
  fprintf(stderr, "%d\t%d\t%d\n",
	  number_of_keys, 
	  number_of_written_pages,
	  number_of_major_gc_written_pages);
  printf("%d\t%d\t%d\n",
	 number_of_keys, 
	 number_of_written_pages,
	 number_of_major_gc_written_pages);
#endif
  fprintf(stderr, "%d updates in %g seconds\n",
	  db_size / 8, give_time() - start_time);
  printf("%d updates in %g seconds\n",
	 db_size / 8, give_time() - start_time);

  return 0;
}
