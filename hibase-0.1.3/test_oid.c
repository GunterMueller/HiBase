/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kengatharan Sivalingam <siva@iki.fi>
 */

/* A benchmark test for object identity (oid) allocations.
 */

#include "includes.h"
#include "root.h"
#include "oid.h"
#include "test_aux.h"


static char *rev_id = "$Id: test_oid.c,v 1.23 1997/07/09 09:51:26 siva Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


static zipf_t *z;
static word_t *oid_table;
static int duration, show_progress;

static void do_oid_allocations(void);

int main(int argc, char *argv[])
{
  long number_of_objects;
  double skewedness;
  int i;

  if (argc < 4) {
    fprintf(stderr,
	    "Usage: %s num-of-objects seconds skewedness [--show-progress] \
[params]\n",
	    argv[0]);
    exit(1);
  }

  number_of_objects = atol(argv[1]);
  duration = atoi(argv[2]);
  skewedness = atof(argv[3]);

  argv[1] = NULL;
  argv[2] = NULL;
  argv[3] = NULL;

  /* Read other arguments given exclusively for `test_oid'. */
  for (i = 3; i < argc; i++) {
    if (argv[i] == NULL)
      continue;
    if (strcmp("--show-progress", argv[i]) == 0) {
      show_progress = 1;
      argv[i] = NULL;
      break;
    }
  }

  srandom(give_time());
  init_shades(argc, argv);
  create_db();
  z = zipf_init(number_of_objects, skewedness);

  /* Allocate and initialize oid table. */
  oid_table = calloc(number_of_objects, sizeof(word_t));
  if (oid_table == NULL) {
    fprintf(stderr, "Fatal: calloc failed.\n");
    exit(1);
  }

  do_oid_allocations();
  return 0;
}


#define OP_SET_SIZE     (4*2048)

static void do_oid_allocations(void)
{
  ptr_t obj_p;
  double start_time, end_time, total_runtime = 0;
  unsigned long number_of_ops = 0;
  unsigned long k, number_of_allocations = 0;
  unsigned long prev_max_oid = 0;
  unsigned int i;

  while (1) {
    start_time = give_time();
    for (i = 0; i < OP_SET_SIZE; i++) {
      /* Get next Zipf number. */
      k = zipf(z);
      number_of_ops++;

      /* Allocate a word long dummy object. The word is initialized 
	 with the number of object/oid allocations made so far. */
      if (oid_table[k] == 0) {
	while (!can_allocate(OID_ALLOCATE_MAX_ALLOCATION + 1 + 1))
	  flush_batch();

	obj_p = allocate(1 + 1, CELL_word_vector);
	obj_p[0] |= 1;
	obj_p[1] = ++number_of_allocations;
	oid_table[k] = oid_allocate(PTR_TO_WORD(obj_p));
#if 0
	if (oid_table[k] > prev_max_oid) {
	  prev_max_oid = oid_table[k];
	  printf("%u\t%u\n", prev_max_oid >> 2, number_of_allocations);
	  fflush(stdout);
	}
	if (number_of_allocations > 2000000000)
	  exit(0);
#endif
      } else {
	while (!can_allocate(OID_DISPOSE_MAX_ALLOCATION))
	  flush_batch();

	oid_dispose(oid_table[k]);
	oid_table[k] = 0;
      }
    }
    end_time = give_time();

    total_runtime += end_time - start_time;
    if (total_runtime > duration)
      break;

    if (show_progress)
      fprintf(stderr, 
	      " %8ld oid_max = %d                                          %c",
	      number_of_allocations, GET_ROOT_WORD(oid_max), 13);
  }

  /* Print out results. APS = oid allocations/second */
  fprintf(stdout, "@@@ Allocations: %ld, APS: %.2f & Oid-max: %ld @@@\n", 
	  number_of_allocations,
	  (float) number_of_allocations / total_runtime,
	  GET_ROOT_WORD(oid_max));

  cell_clear_size_stats();
  cell_compute_size_stats(GET_ROOT_PTR(oid_map));
  cell_fprint_size_stats(stdout);
}
