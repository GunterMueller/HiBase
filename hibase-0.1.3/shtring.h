/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Lars Wirzenius <liw@iki.fi>
 */


/* Declarations for Shades's strings, shtrings.

   A shtring is an immutable one-dimensional array of arbitrary
   8-bit bytes. The basic operations are:
   
	   shtring_create(cstr, cstrlen)
		   create a shtring from a C array of characters
		   
	   shtring_length(shtr)
		   return length of a shtring
	   
	   shtring_cat(shtr1, shtr2)
		   create a new shtring that is the concatenation
		   of two existing shtrings
	   
	   shtring_copy(shtr1, start, len)
		   create a new shtring from part of an existing
		   shtring
		   
	   shtring_charat(shtr, pos)
		   return character at a given position in a shtring

	   shtring_strat(cstr, shtr, start, len)
		   copy bytes from a shtr to a C array of
		   characters
		   
	   shtring_cmp(shtr1, shtr2)
	   	   compare two shtrings; return a cmp_result_t
		   (see cells.h)

   Note that a shtring is never modified after it has been created,
   and that new shtrings are created from C byte arrays or existing
   shtrings.
   
   More detailed descriptions of the operations below. 
   
   The basic implementation of shtrings is character set
   insensitive, but they operate only on 8-bit bytes. Interesting
   modern character sets have at least 16 bits per character,
   possibly 32. These character sets can be used with shtrings
   by using a suitable encoding, such as the one used in Plan9
   from Bell Labs. 
   
   However, pattern matching and other operations may not
   work correctly with these character sets. Life is hard and
   then you fly.  */
   

#ifndef INCL_SHTRING_H
#define INCL_SHTRING_H


#include "includes.h"
#include "shades.h"


/* Size of a CELL_shtring, in words. */
#define SHTRING_WORDS 5

/* Maximum number of characters in one chunk. */
#ifndef SHTRING_CHUNK_MAX
#define SHTRING_CHUNK_MAX	32
#endif

/* Note: assumes that sizeof(word_t) == 4, since the preprocessor doesn't
   grok sizeof. */
#if (SHTRING_CHUNK_MAX % 4) != 0
#error Maximum number of characters in chunk not divisible by four (== sizeof(word_t))
#endif


/* Compute the number of words to perform `shtring_create(cstr, len)'. */
word_t shtring_create_max_allocation(size_t len);


/* Create a new shtring and initialize it from a C byte array. Return
   -1 for permanent error (no use re-trying), or 1 (operation successful, 
   pointer to new shtring stored in `*shtr'). Assumes that caller has
   done `can_allocate(shtring_create_max_allocation(len))'. */
int shtring_create(ptr_t *shtr, char *cstr, size_t len);


/* Compute the number of words to perform `shtring_cat(shtr1, shtr2)'. */
word_t shtring_cat_max_allocation(ptr_t shtr1, ptr_t shtr2);


/* Catenate two shtrings. Return pointer to new shtring. Assume
   `can_allocate(shtring_cat_max_allocation(shtr1, shtr2))'. */
ptr_t shtring_cat(ptr_t shtr1, ptr_t shtr2);


/* Make a new shtring that is a copy of some part of an existing shtring. */
ptr_t shtring_copy(ptr_t shtr, word_t start, word_t len);


/* Return the character at a given position in a shtring. */
int shtring_charat(ptr_t shtr, word_t pos);


/* Return the characters at a given range in a shtring in a C array.
   The caller takes care of allocating enough memory for the result
   before the call (i.e., `cstr' should point at an allocated memory
   area of at least `len' characters). */
void shtring_strat(char *cstr, ptr_t shtr, word_t pos, word_t len);


/* Return the length (in characters) of a shtring. */
word_t shtring_length(ptr_t shtr);


/* Output a debugging dump of a shtring to `f'. */
void shtring_fprint(FILE *f, ptr_t shtr);


/* Compare two shtrings. Return a cmp_result_t (see cells.h). */
cmp_result_t shtring_cmp_without_allocating(ptr_t, ptr_t);
word_t shtring_cmp_piecemeal_max_allocation(ptr_t, ptr_t);
cmp_result_t shtring_cmp_piecemeal(ptr_t *cur1, ptr_t *cur2, word_t maxchars);


/* Intern a shtring. XXX better comment. */

#include "triev2.h"	/* need TRIE_MAX_ALLOCATION */
#define SHTRING_INTERN_MAX_ALLOCATION (4 + 2*TRIEV2_MAX_ALLOCATION + 4)

typedef struct shtring_intern_result {
  ptr_t new_intern_root;
  ptr_t interned_shtring;
  word_t interned_shtring_id;
  int was_new;
} shtring_intern_result_t;

word_t shtring_hash(ptr_t shtr);
shtring_intern_result_t shtring_intern(ptr_t root, ptr_t shtr);
ptr_t shtring_lookup_by_intern_id(ptr_t root, word_t id);


/* Converting shtrings to numbers. The functions return one of the
   values SHTRING_OK (`*result' holds result), SHTRING_ABS_TOO_BIG
   (absolute value of number is too big), SHTRING_ABS_TOO_SMALL
   (absolute value of number is too small; shtring_to_double only),
   SHTRING_NOT_A_NUMBER (syntax error in number). 
   
   The shtrings may contain leading whitespace, and trailing
   garbage. On successful return, `end' will give the offset of the first
   character after the number, if there is trailing garbage, or
   shtring_length(shtr) if there is not.
   
   */

typedef enum {
	SHTRING_OK,
	SHTRING_ABS_TOO_BIG,
	SHTRING_ABS_TOO_SMALL,
	SHTRING_NOT_A_NUMBER
} shtring_to_result_t;

shtring_to_result_t shtring_to_long(long *result, word_t *end, ptr_t shtr, 
	int base);
shtring_to_result_t shtring_to_ulong(unsigned long *result, word_t *end, 
	ptr_t shtr, int base);
shtring_to_result_t shtring_to_double(double *result, word_t *end, ptr_t shtr);

/* Remove leading or trailing whitespace from shtring. Whitespace is those
   characters for which isspace returns true. */
ptr_t shtring_trim_leading(ptr_t shtr);
ptr_t shtring_trim_trailing(ptr_t shtr);


/* These cursors are made for walking, and soon they'll walk all over trees... */
word_t shtring_cursor_max_allocation(ptr_t shtr);
ptr_t shtring_cursor_create(ptr_t shtr);
word_t shtring_cursor_get_position(ptr_t cur);
ptr_t shtring_cursor_move_forward(ptr_t cur, word_t nchars);


#endif
