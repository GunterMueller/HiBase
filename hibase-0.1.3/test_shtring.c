/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Lars Wirzenius <liw@iki.fi>
 *	(based on test_queue.c)
 */


/* Test/benchmark program for shtrings.
 *
 * This is best run with very small first generation sizes in order to
 * stress the gc-safety.  E.g.
 *   ./configure --enable-page-size=4
 *   ./test_queue --first-generation-size=8k -v
 */
 
/* Description of automated testing.

Test cases

	shtring_create

	- empty string
	- 1 character
	- 2, 3, 4 characters (4 chars/word)
	- N-1, N, N+1 characters (N = maximum chars per chunk)
	- i*N characters, where i = 0 to maximum number of chunks + 1

	shtring_charat
	
	- for each test case for shtring_create, each character in range
	
	shtring_strat
	
	- for each test case for shtring_create, all substrings of lengths
	  0, 1, 2, 3, 4, 5, 7, 8, 9, N-1, N, N+1
	- all other test cases use strat implicitly as well
	  
	shtring_copy
	
	- for each test case for shtring_create, copy from offsets
	  0, 1, 2, 3, 4, N-1, N, N+1, and all lengths from 0 to length
	  of rest of shtring.
	  
	shtring_cat
	
	- all cases for shtring_copy on each other (n*n cases)

	shtring_length
	
	- each test case for shtring_create and shtring_cat and
	  shtring_copy: check result length (implicitly)

Coverage

	The above test cases do not get very good coverage (not even all
	lines are tested). A coverage measurement tool would be nice.

Implementation of test cases

	Function make_shtring(n) calls shtring_create to create a
	test shtring of length n with a known pattern (lower case
	letters a-z, repeating enough times).
	
	Function check_shtring(shtr, start1, len1, start2, len2)
	checks that shtr is of length n, and that its contents
	follow the pattern, or have two patterns catted. start1
	and start2 tell the offset into the pattern, len1 and len2
	tell the length of the sub-pattern.

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
#include <limits.h>

static char *rev_id = "$Id: test_shtring.c,v 1.30 1998/03/28 16:57:55 liw Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

/* Maximum number of chunks. */
#if 0
#define MAX_CHUNKS	15000
#else
#define MAX_CHUNKS	((4*15000L) / SHTRING_CHUNK_MAX)
#endif

/* Return number of elements in an array. `arr' must be an array name, it
   may not be a pointer. */
#define ELEMS(arr)	(sizeof(arr) / sizeof(arr[0]))


/* Output a status message. */
#ifndef NDEBUG
#define STATUS(args)	printf args
#else
#define STATUS(args)	((void) 0)
#endif


/* The pattern used to fill shtring values during testing. */
static char pattern[] = "abcdefghijklmnopqrstuvwxyz";
static int patsize = sizeof(pattern) - 1;


/* Panic: something went so wrong we can't recover from it. */
static void panic(char *msg)
{
  fprintf(stderr, "PANIC: %s\n", msg);
  abort();
}


/* Garbage collect. */
static void gc(void)
{
  flush_batch();
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


/* Fill a memory area with a pattern. */
static void fill(void *mem, size_t memsize, void *pat, size_t patsize)
{
  char *p;
  size_t i;

  p = mem;
  for (i = memsize / patsize; i > 0; --i, p += patsize)
    memcpy(p, pat, patsize);
  memcpy(p, pat, memsize % patsize);
}


/* Make a shtring of length `n', and make it contain a known pattern
   (a-z, repeating enough times). Abort program if creation fails. */
static void make_shtring(int desired_result, ptr_t *shtr, word_t n)
{
  char *value;
  int result;
  
  value = xmalloc(n);
  fill(value, n, pattern, patsize);
  if (!can_allocate(shtring_create_max_allocation(n)))
    panic("not enough room to create shtring");
  result = shtring_create(shtr, value, n);
#if 0
  if (result == 0 && desired_result != 0) {
    gc();
    result = shtring_create(shtr, value, n);
  }
#endif
  if (result != desired_result) 
    panic("shtring_create did not return desired result");
  free(value);
}


/* Make a shtring, from a string. */
static int make_shtring_from_string(ptr_t *shtr, char *str)
{
  int result;
  size_t n;

  n = strlen(str);
  if (!can_allocate(shtring_create_max_allocation(n)))
    panic("not enough room to create shtring");
  result = shtring_create(shtr, str, n);
#if 0 /* this can't ever happen anymore */
  if (result == 0)
    return 0;
#endif
  if (result == -1)
    panic("couldn't create a shtring -- out of memory?");
  return 1;
}


/* Check that the first n characters of str follow the pattern starting at
   start. */
#ifdef NDEBUG
#define check_pattern(a,b,c)	((void) 0)
#else
static void check_pattern(char *str, size_t n, size_t start)
{
  size_t i, j;

  start %= patsize;
  for (i = 0, j = start; i < n; ++i, j = (j + 1) % patsize)
    if (str[i] != pattern[j])
      panic("string does not match pattern");
}
#endif


/* Check that the first len1 characters of a shtring contain the pattern
   (starting at offset start1), and likewise for the rest of the shring
   (start2, len2). The total length of the shtring must be len1 + len2. */
#ifdef NDEBUG
#define check_shtring(a,b,c,d,e)	((void) 0)
#else
static void check_shtring(ptr_t shtr, word_t start1, word_t len1,
				word_t start2, word_t len2)
{
  char *value;

  if (shtring_length(shtr) != len1 + len2)
    panic("shtring is of wrong length");
  value = xmalloc(len1 + len2);
  shtring_strat(value, shtr, 0, len1 + len2);
  check_pattern(value, len1, start1);
  check_pattern(value + len1, len2, start2);
  free(value);
}
#endif

static word_t old_string_hash(char *s, int len)
{
  word_t h = 5381;
  int i;

  for (i = 0; i < len; i++)
    h = (h << 5) + h + (h >> 3) + s[i];
  return h;
}

static void call_shtring_to_long(char *s, int *result, long *value, word_t *off)
{
  ptr_t shtr;
  if (!can_allocate(shtring_create_max_allocation(strlen(s)))) {
    gc();
    if (!can_allocate(shtring_create_max_allocation(strlen(s))))
      panic("can't allocate shtring");
  }
  shtring_create(&shtr, s, strlen(s));
  *result = shtring_to_long(value, off, shtr, 0);
}

static void call_shtring_to_ulong(char *s, int *result, unsigned long *value, 
				  word_t *off)
{
  ptr_t shtr;
  if (!can_allocate(shtring_create_max_allocation(strlen(s)))) {
    gc();
    if (!can_allocate(shtring_create_max_allocation(strlen(s))))
      panic("can't allocate shtring");
  }
  shtring_create(&shtr, s, strlen(s));
  *result = shtring_to_ulong(value, off, shtr, 0);
}

static void test_shtring_trimming(char *input, char *wanted, 
                                  ptr_t (*fun)(ptr_t)) {
  ptr_t shtr_input, shtr_result;
  char result[10*1024];
  size_t len;

  make_shtring_from_string(&shtr_input, input);
  shtr_result = fun(shtr_input);
  len = strlen(wanted);
  assert (len < sizeof(result));
  if (shtring_length(shtr_result) != len)
    panic("test_shtring_trimming: result is of wrong length");
  shtring_strat(result, shtr_result, 0, len);
  if (memcmp(result, wanted, len) != 0)
    panic("test_shtring_trimming: result is wrong");
}

static void reverse(char *result, const char *input) {
  size_t i, len;
  
  for (i = 0, len = strlen(input); i < len; ++i)
    result[len - 1 - i] = input[i];
  result[len] = '\0';
}

int main(int argc, char **argv)
{
  ptr_t p, shtr, shtr2, copy;
  word_t i, j, len, prev, step;
  int result, shtr_result;
  int test_create, test_charat, test_strat, test_copy, test_cat, test_cmp,
  	test_hash, test_numbers, test_trim, test_intern, test_print;
  char buf[1024];
  char *s, *value;
  word_t sizes[] = {
    0, 1, 2, 3, 4, SHTRING_CHUNK_MAX-1, SHTRING_CHUNK_MAX, SHTRING_CHUNK_MAX+1,
    (MAX_CHUNKS-1) * SHTRING_CHUNK_MAX, MAX_CHUNKS * SHTRING_CHUNK_MAX,
  };
  word_t charatsizes[] = {
    0, 1, 2, 3, 4, SHTRING_CHUNK_MAX-1, SHTRING_CHUNK_MAX, SHTRING_CHUNK_MAX+1,
    MAX_CHUNKS * SHTRING_CHUNK_MAX,
  };
  word_t offsets[] = {
    0, 1, 2, 3, 4, 5, 7, 8, 9, 
    SHTRING_CHUNK_MAX-1, SHTRING_CHUNK_MAX, SHTRING_CHUNK_MAX+1
  };
  word_t lens[] = {
    0, 1, 2, 3, 4, 5, 7, 8, 9, 
    SHTRING_CHUNK_MAX-1, SHTRING_CHUNK_MAX, SHTRING_CHUNK_MAX+1,
    13*SHTRING_CHUNK_MAX, MAX_CHUNKS/2 * SHTRING_CHUNK_MAX, 
    (MAX_CHUNKS-1) * SHTRING_CHUNK_MAX, MAX_CHUNKS * SHTRING_CHUNK_MAX,
  };
  char *cmptab[] = {
    "", "a", "b", "aa", "ab", "ba", "bb",
    "0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-",
    "0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-foo",
    "Turo", "Erkki", "Vainio"
  };
  
  init_shades(argc, argv);
  create_db();
  
  test_create = test_charat = test_strat = test_copy = 
  test_cat = test_cmp = test_hash = test_numbers = 
  test_trim = test_intern = test_print = 0;
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
    else if (strcmp(s, "hash") == 0)
      test_hash = 1;
    else if (strcmp(s, "numbers") == 0)
      test_numbers = 1;
    else if (strcmp(s, "trim") == 0)
      test_trim = 1;
    else if (strcmp(s, "intern") == 0)
      test_intern = 1;
    else if (strcmp(s, "print") == 0)
      test_print = 1;
    else if (strcmp(s, "all") == 0)
      test_create = test_charat = test_strat = test_copy = 
      test_cat = test_cmp = test_hash = test_numbers = 
      test_trim = test_intern = test_print = 1;
    else {
      fprintf(stderr, "Unknown test target %s\n", s);
      exit(1);
    }
  }
  if (test_create + test_charat + test_strat + test_copy + 
      test_cat + test_cmp + test_hash + test_numbers + test_trim + 
      test_intern + test_print == 0)
    test_create = test_charat = test_strat = test_copy = 
    test_cat = test_cmp = test_hash = test_numbers = 
    test_trim = test_intern = test_print = 1;

  SET_ROOT_PTR(test1, NULL_PTR);
  p = GET_ROOT_PTR(test1);
  
  if (max_shtring_chunks == 0)
    max_shtring_chunks = MAX_CHUNKS;

  sizes[ELEMS(sizes)-2] = (max_shtring_chunks - 1) * SHTRING_CHUNK_MAX;
  sizes[ELEMS(sizes)-1] = max_shtring_chunks * SHTRING_CHUNK_MAX;
  charatsizes[ELEMS(charatsizes)-2] = (max_shtring_chunks - 1) * SHTRING_CHUNK_MAX;
  charatsizes[ELEMS(charatsizes)-1] = max_shtring_chunks * SHTRING_CHUNK_MAX;
  lens[ELEMS(lens)-3] = max_shtring_chunks/2 * SHTRING_CHUNK_MAX;
  lens[ELEMS(lens)-2] = (max_shtring_chunks-1) * SHTRING_CHUNK_MAX;
  lens[ELEMS(lens)-1] = max_shtring_chunks * SHTRING_CHUNK_MAX;

  if (max_shtring_chunks < 500)
    step = 1;
  else if (max_shtring_chunks < 5000)
    step = 19;
  else
    step = 1066;

  /* Test shtring_create */
  if (test_create) {
#if 1
    for (i = 0; i < ELEMS(sizes); ++i) {
      make_shtring(1, &shtr, sizes[i]);
      check_shtring(shtr, 0, sizes[i], 0, 0);
      STATUS(("Created size %lu shtring OK.\n", (unsigned long) sizes[i]));
      gc();
    }
#else
    for (i = 0; i < 10000; ++i) {
#ifndef NDEBUG
      if (i > 0 && (i % 10) == 0) printf("%lu\n", (unsigned long) i);
#endif
      make_shtring(1, &shtr, 10*1024);
    }
    return 0;
#endif
  }
  
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

  /* Test shtring_strat */
  if (test_strat) {
    value = xmalloc(charatsizes[ELEMS(charatsizes) - 1]);
    prev = 0;
    for (i = 0; i < ELEMS(charatsizes); ++i) {
      make_shtring(1, &shtr, charatsizes[i]);
      for (j = 0; j < charatsizes[i]; j += (j < step) ? 1 : step) {
	for (len = 0; len < ELEMS(lens) && lens[len] < charatsizes[i]-j; ++len) {
	  shtring_strat(value, shtr, j, lens[len]);
	  check_pattern(value, lens[len], j);
	}
	if (j - prev >= 100) {
	  STATUS(("shtring_strat for size %lu at offset %lu...\n",
		 (unsigned long) charatsizes[i], (unsigned long) j));
	  prev = j;
	}
      }
      STATUS(("shtring_strat for size %lu OK.\n", 
	      	(unsigned long) charatsizes[i]));
      gc();
    }
    free(value);
  }

  /* Test shtring_copy */
  if (test_copy) {
    prev = 0;
    for (i = 0; i < ELEMS(sizes); ++i) {
      make_shtring(1, &shtr, sizes[i]);
      for (j = 0; j < ELEMS(offsets); ++j) {
	if (offsets[j] > sizes[i])
	  continue;
	for (len = 0; len < sizes[i]-offsets[j]; len += (len<step) ? 1 : step) {
	  if (!can_allocate(SHTRING_WORDS))
	    panic("no room for shtring_cat");
	  copy = shtring_copy(shtr, offsets[j], len);
	  check_shtring(copy, offsets[j], len, 0, 0);
	  if (len - prev > 100) {
	    STATUS(("shtring_copy for size %lu at offset %lu, substring length %lu...\n",
		   (unsigned long) sizes[i], (unsigned long) offsets[j],
		   (unsigned long) len));
	    prev = len;
	  }
	}
      }
      STATUS(("shtring_copy for size %lu OK.\n", (unsigned long) sizes[i]));
      gc();
    }
  }

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
  
  /* Test shtring_cmp */  
  if (test_cmp) {
    ptr_t cur, cur2;
    
    for (i = 0; i < ELEMS(cmptab); ++i) {
      for (j = 0; j < ELEMS(cmptab); ++j) {
        while (make_shtring_from_string(&shtr, cmptab[i]) == 0 ||
               make_shtring_from_string(&shtr2, cmptab[j]) == 0) {
	  STATUS(("garbage collecting...\n"));
	  gc();
	}
	
	result = strcmp(cmptab[i], cmptab[j]);
	shtr_result = shtring_cmp_without_allocating(shtr, shtr2);
	switch (shtr_result) {
	case CMP_EQUAL:
	  if (result != 0) panic("shtring_cmp_without_allocating returned =0"); break;
	case CMP_LESS:
	  if (result >= 0) panic("shtring_cmp_without_allocating returned <0"); break;
	case CMP_GREATER:
	  if (result <= 0) panic("shtring_cmp_without_allocating returned >0"); break;
	default:
	  panic("shtring_cmp_without_allocating returned something curious");
	}

	if (!can_allocate(shtring_cursor_max_allocation(shtr) +
			  shtring_cursor_max_allocation(shtr2)))
	  panic("can't allocate enough for creating cursors for comparison");
	cur = shtring_cursor_create(shtr);
	cur2 = shtring_cursor_create(shtr2);
	do {
	  if (!can_allocate(shtring_cmp_piecemeal_max_allocation(shtr, shtr2)))
	    panic("can't allocate enough for comparison");
	  shtr_result = shtring_cmp_piecemeal(&cur, &cur2, 1);
	} while (shtr_result ==	CMP_PLEASE_CONTINUE);
	switch (shtr_result) {
	case CMP_EQUAL:
	  if (result != 0) panic("shtring_cmp_piecemeal returned =0"); break;
	case CMP_LESS:
	  if (result >= 0) panic("shtring_cmp_piecemeal returned <0"); break;
	case CMP_GREATER:
	  if (result <= 0) panic("shtring_cmp_piecemeal returned >0"); break;
	default:
	  panic("shtring_cmp_without_allocating returned something curious");
	}
	
	gc();
      }
    }
    STATUS(("shtring_cmp_{without_allocating,piecemeal} both ok\n"));
  }

  /* shtring_hash */
  if (test_hash) {
    word_t hash, old_hash;
    char *value;
    
    for (i = 0; i < ELEMS(sizes); ++i) {
      value = xmalloc(sizes[i]);
      fill(value, sizes[i], pattern, patsize);
      if (!can_allocate(shtring_create_max_allocation(sizes[i])))
        panic("not enough room to create shtring");
      if (shtring_create(&shtr, value, sizes[i]) != 1)
        panic("shtring_create failed");
      hash = shtring_hash(shtr);
      old_hash = old_string_hash(value, (int) sizes[i]);
      free(value);
      printf("len=%ld, new=%08lx, old=%08lx, same=%d\n",
        (unsigned long) sizes[i],
      	(unsigned long) hash,
      	(unsigned long) old_hash,
	hash == old_hash);
      gc();
    }
  }

  /* shtring_to_long */
  if (test_numbers) {
    long oktab[] = {
	0, 1, -1, 42, -69, 105, LONG_MAX, LONG_MIN,
    };
    struct {
      int result;
      char *str;
    } *p, badtab[] = {
	{ SHTRING_ABS_TOO_BIG, "999999999999999999999999999999999999" },
	{ SHTRING_ABS_TOO_BIG, "+999999999999999999999999999999999999" },
	{ SHTRING_ABS_TOO_BIG, "-999999999999999999999999999999999999" },
	{ SHTRING_NOT_A_NUMBER, "" },
	{ SHTRING_NOT_A_NUMBER, "x" },
	{ SHTRING_NOT_A_NUMBER, "-" },
	{ SHTRING_NOT_A_NUMBER, "+" },
	{ SHTRING_NOT_A_NUMBER, " " },
    };
    long n;
    int result;
    word_t i, off;
    
    for (i = 0; i < sizeof(oktab)/sizeof(*oktab); ++i) {
      sprintf(buf, "%ld", oktab[i]);
      call_shtring_to_long(buf, &result, &n, &off);
      if (result != SHTRING_OK)
        panic("shtring_to_long failed for known-good value");
      if (n != oktab[i])
        panic("shtring_to_long gave wrong result");
      if (off != strlen(buf))
        panic("shtring_to_long set end offset wrongly");
    }

    for (i = 0; i < sizeof(badtab)/sizeof(*badtab); ++i) {
      p = badtab + i;
      call_shtring_to_long(p->str, &result, &n, &off);
      if (result != p->result)
        panic("shtring_to_long failed to handle bad value correctly");
      if (result == SHTRING_NOT_A_NUMBER && off != 0)
        panic("shtring_to_long set end offset wrongly for bad value");
    }

    printf("shtring_to_long ok\n");
  }

  /* shtring_to_ulong */
  if (test_numbers) {
    unsigned long oktab[] = {
	0, 1, -1, 42, 69, 105, LONG_MAX, ULONG_MAX,
    };
    struct {
      int result;
      char *str;
    } *p, badtab[] = {
	{ SHTRING_ABS_TOO_BIG, "999999999999999999999999999999999999" },
	{ SHTRING_ABS_TOO_BIG, "+999999999999999999999999999999999999" },
	{ SHTRING_ABS_TOO_BIG, "-999999999999999999999999999999999999" },
	{ SHTRING_NOT_A_NUMBER, "" },
	{ SHTRING_NOT_A_NUMBER, "x" },
	{ SHTRING_NOT_A_NUMBER, "-" },
	{ SHTRING_NOT_A_NUMBER, "+" },
	{ SHTRING_NOT_A_NUMBER, " " },
    };
    unsigned long n;
    int result;
    word_t i, off;
    
    for (i = 0; i < sizeof(oktab)/sizeof(*oktab); ++i) {
      sprintf(buf, "%lu", oktab[i]);
      call_shtring_to_ulong(buf, &result, &n, &off);
      if (result != SHTRING_OK)
        panic("shtring_to_ulong failed for known-good value");
      if (n != oktab[i])
        panic("shtring_to_ulong gave wrong result");
      if (off != strlen(buf))
        panic("shtring_to_ulong set end offset wrongly");
    }

    for (i = 0; i < sizeof(badtab)/sizeof(*badtab); ++i) {
      p = badtab + i;
      call_shtring_to_ulong(p->str, &result, &n, &off);
      if (result != p->result)
        panic("shtring_to_ulong failed to handle bad value correctly");
      if (result == SHTRING_NOT_A_NUMBER && off != 0)
        panic("shtring_to_ulong set end offset wrongly for bad value");
    }

    printf("shtring_to_ulong ok\n");
  }
  
  if (test_trim) {
    struct {
      char *str, *wanted;
    } *p, tab[] = {
      { "", "" },
      { "x", "x" },
      { "x ", "x " },
      { " ", "" },
      { " x", "x" },
      { " x ", "x " },
      { "\t", "" },
      { "\tx", "x" },
      { "\n", "" },
      { "\nx", "x" },
      { "\nx ", "x " },
      { "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
        "                                                                    "
	"x", "x" },
    };
    long n;
    int result;
    word_t i, off;
    char reverse_str[10*1024];
    char reverse_wanted[10*1024];
    
    for (i = 0; i < sizeof(tab)/sizeof(*tab); ++i) {
      p = tab + i;
      test_shtring_trimming(p->str, p->wanted, shtring_trim_leading);
    }
    printf("shtring_trim_leading ok\n");
      
    for (i = 0; i < sizeof(tab)/sizeof(*tab); ++i) {
      p = tab + i;
      reverse(reverse_str, p->str);
      reverse(reverse_wanted, p->wanted);
      test_shtring_trimming(reverse_str, reverse_wanted, shtring_trim_trailing);
    }
    printf("shtring_trim_trailing ok\n");
  }

  if (test_intern) {
    word_t hash, old_hash;
    char *value;
    ptr_t intern_root;
    ptr_t shtrings[ELEMS(sizes)];
    word_t ids[ELEMS(sizes)];
    shtring_intern_result_t result;
    
    intern_root = NULL_PTR;
    for (i = 0; i < ELEMS(sizes); ++i) {
      printf("intern: interning shtring %d\n", (int) i);
      value = xmalloc(sizes[i]);
      fill(value, sizes[i], pattern, patsize);
      if (!can_allocate(shtring_create_max_allocation(sizes[i])))
        panic("not enough room to create shtring");
      if (shtring_create(&shtr, value, sizes[i]) != 1)
        panic("shtring_create failed");
      free(value);
      shtrings[i] = shtr;

      if (!can_allocate(SHTRING_INTERN_MAX_ALLOCATION))
        panic("not enough room for shtring_intern");
      result = shtring_intern(intern_root, shtr);
      if (!result.was_new)
        panic("shtring_intern claimed it already knew the shtring, it didn't");
      ids[i] = result.interned_shtring_id;
      intern_root = result.new_intern_root;
    }

    for (i = 0; i < ELEMS(sizes); ++i) {
      printf("intern: looking up shtring %d\n", (int) i);
      if (!can_allocate(SHTRING_INTERN_MAX_ALLOCATION))
        panic("not enough room for shtring_intern");
      result = shtring_intern(intern_root, shtrings[i]);
      if (result.was_new)
        panic("shtring_intern didn't find shtring again");
      if (result.interned_shtring != shtrings[i])
        panic("shtring_intern found another shtring");
      if (result.interned_shtring_id != ids[i])
        panic("shtring_intern found another shtring");

      shtr = shtring_lookup_by_intern_id(intern_root, ids[i]);
      if (shtr == NULL_PTR)
        panic("shtring_lookup_by_shtring_id didn't find shtring");
      if (shtr != shtrings[i])
        panic("shtring_lookup_by_shtring_id found wrong shtring");
    }

    gc();
    printf("shtring_intern and shtring_lookup_by_intern_id ok\n");
  }
  
  /* Test shtring_fprint */
  if (test_print) {
    FILE *f;
    char buf1[MAX_CHUNKS * SHTRING_CHUNK_MAX + 1024];
    char buf2[MAX_CHUNKS * SHTRING_CHUNK_MAX + 1024];

    for (i = 0; i < ELEMS(sizes); ++i) {
      f = tmpfile();
      if (f == NULL)
        panic("couldn't create temporary file");
      make_shtring(1, &shtr, sizes[i]);
      check_shtring(shtr, 0, sizes[i], 0, 0);
      shtring_fprint(f, shtr);
      rewind(f);
      if (fread(buf1, 1, sizes[i] + 1, f) != sizes[i])
        panic("shtring_fprint output too long or I/O error");
      shtring_strat(buf2, shtr, 0, sizes[i]);
      if (memcmp(buf1, buf2, sizes[i]) != 0)
        panic("shtring_fprint corrupts output");
      fclose(f);
      gc();
      STATUS(("shtring_fprint for sizes %lu works\n", (unsigned long) sizes[i]));
    }
  }
  
  return 0;
}
