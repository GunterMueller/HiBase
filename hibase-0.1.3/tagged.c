/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1998 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Common routines for tagged data types.
 */

#include "includes.h"
#include "shades.h"
#include "root.h"
#include "tagged.h"
#include "shtring.h"
#include "cells.h"
#include "interp.h"
#include "dh.h"
/* Disabled until the KDQ becomes public.
   #include "kdqtrie.h" */
#include "list.h"


void tagged_fprint(FILE *fp, word_t word)
{
  ptr_t p;
  int i;
  int is_first = 1;

  if (TAGGED_IS_WORD(word))
    fprintf(fp, "%ld", TAGGED_TO_SIGNED_WORD(word));
  else if (TAGGED_IS_PTR(word)) {
    if (word == NULL_WORD)
      fprintf(fp, "NIL");
    else {
      p = WORD_TO_PTR(word);
      switch (CELL_TYPE(p)) {
      case CELL_list:
	/* Print a list. */
	fprintf(fp, "[");
	for (;;) {
	  tagged_fprint(fp, car(p));
	  p = WORD_TO_PTR(cdr(p));
	  if (p != NULL_PTR)
	    fprintf(fp, ", ");
	  else
	    break;
	}
	fprintf(fp, "]");
	break;
      case CELL_shtring:
	shtring_fprint(fp, p);
	break;
      case CELL_tuple:
	fprintf(fp, "<");
	for (i = 1; i <= (p[0] & 0xFFF); i++) {
	  if (is_first)
	    is_first = 0;
	  else
	    fprintf(fp, ", ");
	  tagged_fprint(fp, p[i]);
	}
	for (i = 1; i <= ((p[0] >> 12) & 0xFFF); i++) {
	  if (is_first)
	    is_first = 0;
	  else
	    fprintf(fp, ", ");
	  fprintf(fp, "%ld", p[(p[0] & 0xFFF) + i]);
	}
	fprintf(fp, ">");
	break;
      case CELL_object:
	{
	  int i, first = 1;
	  ptr_t fs = WORD_TO_PTR(p[1]);

	  fprintf(fp, "Object.<");
	  for (i = 1; i < triev2_find_max(fs, 32); i++)
	    if (triev2_contains(fs, i, 32)) {
	      if (first)
		first = 0;
	      else
		fprintf(fp, ",\n  ");
	      shtring_fprint(fp, 
                shtring_lookup_by_intern_id(GET_ROOT_PTR(interned_shtrings), 
					    i));
	      fprintf(fp, " = ");
	      tagged_fprint(fp, triev2_find_quick(fs, i, 32));
	    }
	  fprintf(fp, "\n>");
	}
	break;
      default:
	cell_fprint(fp, p, 10);
	break;
      }
    }
  } else
    fprintf(fp, "XXX%ld\n", word);
}


int tagged_is_equal(ptr_t p, ptr_t q)
{
  word_t p0;
  cell_type_t t;

  assert(p != q);		/* Should have been checked already. */
  if (p == NULL_PTR || q == NULL_PTR)
    /* Since either cell is NULL_PTR, no other cell is equal to
       NULL_PTR, and because they were not the same cell, they must be
       different. */
    return 0;
  p0 = *p;
  if (p0 != *q)
    /* The first words didn't match, so the cells must be
       different. */
    return 0;
  assert(CELL_TYPE(p) == CELL_TYPE(q));

  switch (t = CELL_TYPE(p)) {
#define CELL(name, number_of_words, field_declaration_block)	\
  case CELL_ ## name:						\
    if (t == CELL_shtring)					\
      return shtring_cmp_without_allocating(p, q) == CMP_EQUAL;	\
    else							\
      field_declaration_block;					\
    break;

#define DECLARE_WORD(x)				\
    do {					\
      if ((x) != q[&(x) - p])			\
        return 0;				\
    } while (0)
#define DECLARE_PTR(x)							 \
    do {								 \
      /* Check recursively whether they are equal.  */			 \
      if ((x) != q[&(x) - p]						 \
          && !tagged_is_equal(WORD_TO_PTR(x), WORD_TO_PTR(q[&(x) - p]))) \
	/* Return inequality as soon as a difference is found. */	 \
	return 0;							 \
    } while (0)
#define DECLARE_NONNULL_PTR(x)  DECLARE_PTR(x)
#define DECLARE_TAGGED(x)			\
    do {					\
      if (!TAGGED_IS_EQUAL(x, q[&(x) - p]))	\
        return 0;				\
    } while (0)

#include "cells-def-prep.h"

#undef CELL
#undef DECLARE_TAGGED
#undef DECLARE_NONNULL_PTR
#undef DECLARE_PTR
#undef DECLARE_WORD

  default:
    /* Illegal type tag. */
    assert(0);
    break;
  }
  return 1;
}
