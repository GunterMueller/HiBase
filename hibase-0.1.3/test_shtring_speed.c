/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Lars Wirzenius <liw@iki.fi>
 *	(based on test_shtring.c)
 */


/* Benchmark program for shtrings.
 *
 * This is best run with very small first generation sizes in order to
 * stress the gc-safety.  E.g.
 *   ./configure --enable-page-size=4
 *   ./test_queue --first-generation-size=8k -v
 */


#include "includes.h"
#include "params.h"
#include "shtring.h"
#include "root.h"
#include "shades.h"
#include "test_aux.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

static char *rev_id = "$Id: test_shtring_speed.c,v 1.2 1997/05/02 12:23:56 liw Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

/* Panic: something went so wrong we can't recover from it. */
static void panic(char *msg)
{
  fprintf(stderr, "PANIC: %s\n", msg);
  abort();
}


/* Call malloc, but exit program if it fails. */
static void *xmalloc(size_t n)
{
  void *p;
  
  p = malloc(n);
  if (p == NULL)
    panic("malloc failed");
  return p;
}


int main(int argc, char **argv)
{
  ptr_t shtr, shtr2;
  int result;
  int test_create, test_charat, test_strat, test_copy, test_cat, test_cmp;
  char *cstr, *cstr2, *s, buf[1024];
  time_t begin, end;
  int i;
  double secs, megabytes;
  
  init_shades(argc, argv);
  create_db();
  
  test_create = test_charat = test_strat = test_copy = 
  test_cat = test_cmp = 0;
  if (strlen(test_shtring) >= sizeof(buf)) {
    fprintf(stderr, "Option value too long (test_shtring)\n");
    exit(1);
  }
  strcpy(buf, test_shtring);
  for (s = strtok(buf, ", "); s != NULL; s = strtok(NULL, ", ")) {
    if (strcmp(s, "create") == 0)
      test_create = 1;
    else if (strcmp(s, "charat") == 0)
      test_charat = 1;
    else if (strcmp(s, "strat") == 0)
      test_strat = 1;
    else if (strcmp(s, "copy") == 0)
      test_copy = 1;
    else if (strcmp(s, "cat") == 0)
      test_cat = 1;
    else if (strcmp(s, "cmp") == 0)
      test_cmp = 1;
    else if (strcmp(s, "all") == 0)
      test_create = test_charat = test_strat = test_copy = 
      test_cat = test_cmp = 1;
    else {
      fprintf(stderr, "Unknown test target %s\n", s);
      exit(1);
    }
  }
  if (test_create + test_charat + test_strat + test_copy + 
      test_cat + test_cmp == 0)
    test_create = test_charat = test_strat = test_copy = 
    test_cat = test_cmp = 1;

  printf("loops %d\n", shtring_loops);
  printf("size %d\n", shtring_size);
  megabytes = (double) shtring_loops * shtring_size / 1024 / 1024;
  printf("megabytes %.3f\n", megabytes);

  if (test_create) {
    cstr = xmalloc(shtring_size);
    time(&begin);
    for (i = 0; i < shtring_loops; ++i) {
      result = shtring_create(&shtr, cstr, shtring_size);
      switch (result) {
      case -1:
	  panic("string is too big!");
	  break;
      case 0:
	flush_batch();
	if (shtring_create(&shtr, cstr, shtring_size) <= 0)
	  panic("shtring_create didn't succeed!");
	break;
      }
    }
    time(&end);
    secs = difftime(end, begin);
    printf("shtring_create %.0f s ", secs);
    if (secs == 0)
      printf("too damn fast\n");
    else
      printf("%.1f MB/s\n", megabytes / secs);
    free(cstr);
  }

#if 0  
  /* Test shtring_charat */
  if (test_charat) {
    prev = 0;
    for (i = 0; i < ELEMS(charatsizes); ++i) {
      make_shtring(1, &shtr, charatsizes[i]);
      for (j = 0; j < charatsizes[i]; ++j) {
	if (shtring_charat(shtr, j) != pattern[j % patsize])
	  panic("shtring_charat failed");
	if (j - prev >= 1000) {
	  STATUS(("shtring_charat for size %lu at offset %lu...\n",
		 (unsigned long) charatsizes[i], (unsigned long) j));
	  prev = j;
	}
      }
      STATUS(("shtring_charat for size %lu OK.\n", 
      		(unsigned long) charatsizes[i]));
      gc();
    }
  }
#endif

  if (test_strat) {
    cstr = xmalloc(shtring_size);
    if (shtring_create(&shtr, cstr, shtring_size) != 1)
      panic("Couldn't create shtring.");
    time(&begin);
    for (i = 0; i < shtring_loops; ++i)
      shtring_strat(cstr, shtr, 0, shtring_size);
    time(&end);
    secs = difftime(end, begin);
    printf("shtring_strat %.0f seconds ", secs);
    if (secs == 0)
      printf("too damn fast\n");
    else
      printf("%.0f megabytes/second\n", megabytes / secs);
    free(cstr);
  }

  if (test_copy) {
    cstr = xmalloc(shtring_size);
    if (shtring_create(&shtr, cstr, shtring_size) != 1)
      panic("couldn't create shtring!");
    time(&begin);
    for (i = 0; i < shtring_loops; ++i) {
      if (!can_allocate(SHTRING_WORDS)) {
        flush_batch();
	if (shtring_create(&shtr, cstr, shtring_size) != 1)
	  panic("couldn't create shtring!");
	if (!can_allocate(SHTRING_WORDS))
	  panic("can't reserve enough memory!");
      }
      (void) shtring_copy(shtr, shtring_size/3, shtring_size/3);
    }
    time(&end);
    secs = difftime(end, begin);
    printf("shtring_copy %.0f seconds\n", secs);
    free(cstr);
  }

#if 0
  /* Test shtring_cat */  
  if (test_cat) {
    for (i = 0; i < ELEMS(sizes); ++i) {
      for (j = 0; j < ELEMS(sizes); ++j) {
	make_shtring(1, &shtr, sizes[i]);
	make_shtring(1, &shtr2, sizes[j]);
	if (!can_allocate(shtring_cat_max_allocation(shtr, shtr2)))
	  panic("not enough room for shtring_cat");
	copy = shtring_cat(shtr, shtr2);
	check_shtring(copy, 0, sizes[i], 0, sizes[j]);
	gc();
      }
      STATUS(("shtring_cat for size %lu OK.\n", (unsigned long) sizes[i]));
    }
  }
#endif

#if 0
  /* Test shtring_cmp */  
  if (test_cmp) {
    for (i = 0; i < ELEMS(cmptab); ++i) {
      for (j = 0; j < ELEMS(cmptab); ++j) {
        while (make_shtring_from_string(&shtr, cmptab[i]) == 0 ||
               make_shtring_from_string(&shtr2, cmptab[j]) == 0) {
	  STATUS(("garbage collecting...\n"));
	  gc();
	}

	result = strcmp(cmptab[i], cmptab[j]);
	shtr_result = shtring_cmp(shtr, shtr2);
	switch (shtr_result) {
	case CMP_EQUAL:
	  if (result != 0) panic("shtring_cmp returned =0"); break;
	case CMP_LESS:
	  if (result >= 0) panic("shtring_cmp returned <0"); break;
	case CMP_GREATER:
	  if (result <= 0) panic("shtring_cmp returned >0"); break;
	default:
	  panic("shtring_cmp returned something curious");
	}
      }
    }
    STATUS(("shtring_cmp ok\n"));
  }
#endif
  
  return 0;
}
