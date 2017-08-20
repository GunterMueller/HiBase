/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* This file contains some tools and checks related to low-level cell
 * types and type tags understood by the shades recovery algorithm.
 */


#include "includes.h"
#include "shades.h"
#include "cells.h"
#include "root.h"
#include "interp.h"
#include "dh.h"
/* Disabled until the KDQ becomes public.
   #include "kdqtrie.h" */
#include "list.h"
#include "shtring.h"

static char *rev_id = "$Id: cells.c,v 1.40 1998/03/31 09:17:31 jiivonen Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


/* Names of types as character strings.  E.g. `type_name[TYPE_list]'
   evaluates to the string "list". */

#undef CELL
#define CELL(name, number_of_words, field_declaration_block)  \
  #name,

const char *cell_type_name[256] = {
#include "cells-def-prep.h"
  NULL
};

#undef CELL


/* Returns the number of words occupied by the referred cell. */

unsigned int cell_get_number_of_words(ptr_t p)
{
  word_t p0 = p[0];

  assert(p != NULL);
  assert(p > NULL_PTR);
  assert(p < (ptr_t) (mem_base + db_size + first_generation_size));
  assert((((ptr_as_scalar_t) p) & 0x3) == 0);

  switch (CELL_TYPE(p)) {

#define CELL(name, number_of_words, field_declaration_block)	\
  case CELL_ ## name:						\
    return (number_of_words);					\
    break;

#include "cells-def-prep.h"

#undef CELL

  default:
    /* Illegal type tag. */
    assert(0);
    return 0;
    break;
  }
}


/* Memory profiling tools. */

static unsigned long number_of_cells[NUMBER_OF_CELLS];
static unsigned long number_of_words_in_cells[NUMBER_OF_CELLS];

void cell_clear_size_stats(void)
{
  int i;

  for (i = 0; i < NUMBER_OF_CELLS; i++)
    number_of_cells[i] = number_of_words_in_cells[i] = 0;
}

void cell_compute_size_stats(ptr_t p)
{
  word_t p0 = p[0];

  assert(p != NULL);
  assert(p > NULL_PTR);
  assert(p < (ptr_t) (mem_base + db_size + first_generation_size));
  assert((((ptr_as_scalar_t) p) & 0x3) == 0);

  switch (CELL_TYPE(p)) {

#define CELL(name, number_of_words, field_declaration_block)		\
  case CELL_ ## name:							\
    number_of_cells[CELL_ ## name]++;					\
    number_of_words_in_cells[CELL_ ## name] += (number_of_words);	\
    field_declaration_block;						\
    break;
#define DECLARE_WORD(x)  /* Do nothing. */
#define DECLARE_PTR(x)					\
    do {						\
      if (x != NULL_WORD)				\
	cell_compute_size_stats(WORD_TO_PTR(x));	\
    } while (0)
#define DECLARE_NONNULL_PTR(x)			\
    do {					\
      assert(x != NULL_WORD);			\
      cell_compute_size_stats(WORD_TO_PTR(x));	\
    } while (0)
#define DECLARE_TAGGED(x)				\
    do {						\
      if (TAGGED_IS_PTR(x) && (x) != NULL_WORD)		\
        cell_compute_size_stats(WORD_TO_PTR(x));	\
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
}

void cell_fprint_size_stats(FILE *fp)
{
  int i;

  for (i = 0; i < NUMBER_OF_CELLS; i++)
    if (number_of_cells[i] != 0) {
      fprintf(fp, "%ld words in %ld %s cells.\n",
	      number_of_words_in_cells[i],
	      number_of_cells[i],
	      cell_type_name[i]);
    }
}


/* Make a copy of the given cell.  Assumes `can_allocate' has been
   called properly. */
ptr_t cell_copy(ptr_t p)
{
  word_t p0 = p[0];
  unsigned int n;
  ptr_t new_p;
  
  assert(p != NULL);
  assert(p > NULL_PTR);
  assert(p < (ptr_t) (mem_base + db_size + first_generation_size));
  assert((((ptr_as_scalar_t) p) & 0x3) == 0);

  switch (CELL_TYPE(p)) {

#define CELL(name, number_of_words, field_declaration_block)	\
  case CELL_ ## name:						\
    n = (number_of_words);					\
    break;

#include "cells-def-prep.h"

#undef CELL

  default:
    /* Illegal type tag. */
    assert(0);
    return NULL_PTR;
    break;
  }
  assert(can_allocate(n));
  new_p = allocate(n, CELL_TYPE(p));
  memcpy(new_p, p, n * sizeof(word_t));
  return new_p;
}


/* Returns non-zero if the two cells are EQUAL: they are of the same
   type and size, and corresponding word members (DECLARE_WORD) are
   bitwise the same, and corresponding pointer members (DECLARE_PTR)
   point to cells that are EQUAL. As a trivial case, if two pointers
   are EQUAL, the cells they point to are also EQUAL. */

int cell_is_equal(ptr_t p, ptr_t q)
{
  word_t p0;

  if (p == q)
    /* The two cells are, in fact, the same cell, so they must be equal. */
    return 1;
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

  switch (CELL_TYPE(p)) {
#define CELL(name, number_of_words, field_declaration_block)	\
  case CELL_ ## name:						\
    field_declaration_block;					\
    break;
#define DECLARE_WORD(x)				\
    do {					\
      if ((x) != q[&(x) - p])			\
        return 0;				\
    } while (0)
#define DECLARE_PTR(x)							\
    do {								\
      /* Check recursively whether they are equal.  */			\
      if (!cell_is_equal(WORD_TO_PTR(x), WORD_TO_PTR(q[&(x) - p])))	\
	/* Return inequality as soon as a difference is found. */	\
	return 0;							\
    } while (0)
#define DECLARE_NONNULL_PTR(x)  DECLARE_PTR(x)
#define DECLARE_TAGGED(x)						\
    do {								\
      if (((x) & 0x3) != (q[&(x) - p] & 0x3))				\
	return 0;							\
      if (TAGGED_IS_PTR(x)) {						\
        if (!cell_is_equal(WORD_TO_PTR(x), WORD_TO_PTR(q[&(x) - p])))	\
	  return 0;							\
      } else {								\
	if ((x) != q[&(x) - p])						\
	  return 0;							\
      }									\
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
  return 0;
}


/* Generic low-level print methods. */

static int cell_indent = 2;

static int the_char(word_t x)
{
  if (isprint(x))
    return x;
  else
    return '.';
}

static void cell_do_fprint(FILE *fp, ptr_t p, int depth, int max_depth)
{
  word_t p0;

  if (depth >= max_depth) {
    fprintf(fp, "%*s", cell_indent * depth, "");
    fprintf(fp, "...\n");
    return;
  }

  p0 = p[0];

  assert(p != NULL);
  assert(p > NULL_PTR);
  assert(p < (ptr_t) (mem_base + db_size + first_generation_size));
  assert((((ptr_as_scalar_t) p) & 0x3) == 0);

  fprintf(fp, "%*s", cell_indent * depth, "");
  fprintf(fp, "type = %s\n", cell_type_name[CELL_TYPE(p)]);
  fprintf(fp, "%*s", cell_indent * depth, "");
  fprintf(fp, "[0]'s non-tag bits = 0x%06lx  (%ld) \n", 
	  (long) (p0 & ~CELL_TYPE_MASK), (long) (p0 & ~CELL_TYPE_MASK));
  /* XXX Should print something about the cell's location in the main
     memory database w.r.t. other pages, commit batching, etc. */

  switch (CELL_TYPE(p)) {

#define CELL(name, number_of_words, field_declaration_block)	\
  case CELL_ ## name:						\
    field_declaration_block;					\
    break;
#define DECLARE_WORD(x)					\
  do {							\
    fprintf(fp, "%*s", cell_indent * depth, "");	\
    fprintf(fp, "[%d] = 0x%08lx  (%d)  \"%c%c%c%c\"\n",	\
            &(x) - p,					\
	    (unsigned long) (x), (int) (x),		\
	    the_char(((x) >> 24) & 0xFF),		\
	    the_char(((x) >> 16) & 0xFF),		\
	    the_char(((x) >> 8) & 0xFF),		\
	    the_char((x) & 0xFF));			\
  } while (0)
#define DECLARE_PTR(x)						\
  do {								\
    fprintf(fp, "%*s", cell_indent * depth, "");                \
    fprintf(fp, "[%d] -> ", &(x) - p);				\
    if ((x) == NULL_WORD)					\
      fprintf(fp, "NULL\n");					\
    else {							\
      fprintf(fp, " (0x%08lx)\n", (unsigned long) (x));		\
      cell_do_fprint(fp, WORD_TO_PTR(x), depth + 1, max_depth);	\
    }								\
  } while (0)
#define DECLARE_NONNULL_PTR(x)					\
  do {								\
    fprintf(fp, "%*s", cell_indent * depth, "");                \
    fprintf(fp, "[%d] -> ", &(x) - p);				\
    assert((x) != NULL_WORD);					\
    fprintf(fp, " (0x%08lx)\n", (unsigned long) (x));		\
    cell_do_fprint(fp, WORD_TO_PTR(x), depth + 1, max_depth);	\
  } while (0)
#define DECLARE_TAGGED(x)						 \
  do {									 \
    fprintf(fp, "%*s", cell_indent * depth, "");			 \
    if (TAGGED_IS_PTR(x)) {						 \
      fprintf(fp, "[%d] -> TAGGED ", &(x) - p);			       	 \
      if ((x) == NULL_WORD)						 \
        fprintf(fp, "NULL\n");						 \
      else {								 \
        fprintf(fp, " (0x%08lx)\n", (unsigned long) (x));		 \
        cell_do_fprint(fp, WORD_TO_PTR(x), depth + 1, max_depth);	 \
      }									 \
    } else {								 \
      fprintf(fp, "[%d] = TAGGED 0x%08lx  (%d)  \"%c%c%c%c\"\n",	 \
              &(x) - p,							 \
  	      (unsigned long) (x), (int) (x),				 \
              the_char(((x) >> 24) & 0xFF),				 \
	      the_char(((x) >> 16) & 0xFF),				 \
	      the_char(((x) >> 8) & 0xFF),				 \
	      the_char((x) & 0xFF));					 \
    }									 \
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
}

void cell_fprint(FILE *fp, ptr_t p, int max_depth)
{
  cell_do_fprint(fp, p, 0, max_depth);
}

/* This is here so that it would be easier to call from gdb. */
static void cell_print(ptr_t ptr, int max_depth)
{
  cell_do_fprint(stdout, ptr, 0, max_depth);
}


/* Make some (in)sanity checks.
 */

#ifndef NDEBUG

int cell_check(ptr_t p)
{
  word_t p0;
  int n;

#ifdef ENABLE_RED_ZONES

#define NUMBER_OF_DEADBEEFABLE_WORDS_PER_CELL  1024
  static unsigned char is_defined[NUMBER_OF_DEADBEEFABLE_WORDS_PER_CELL];
  int i;
  char *nm;
#define DECLARE_POSN(x) 					\
  do {								\
    if (&(x) - p < NUMBER_OF_DEADBEEFABLE_WORDS_PER_CELL) {	\
      assert(!is_defined[&(x) - p]);				\
      is_defined[&(x) - p] = 1;					\
    }								\
  } while (0)

#else

#define DECLARE_POSN(x)  /* Do nothing. */

#endif

  assert(p != NULL);
  assert(p > NULL_PTR);
  assert(p < (ptr_t) (mem_base + db_size + first_generation_size));
  assert((((ptr_as_scalar_t) p) & 0x3) == 0);

  p0 = *p;
  switch (CELL_TYPE(p)) {
#ifdef ENABLE_RED_ZONES
#define CELL(name, number_of_words, field_declaration_block)	\
  case CELL_ ## name:						\
    nm = # name;						\
    n = (number_of_words);					\
    if (CELL_ ## name != CELL_bonk) {				\
      field_declaration_block;					\
    }								\
    break;
#else
#define CELL(name, number_of_words, field_declaration_block)	\
  case CELL_ ## name:						\
    n = (number_of_words);					\
    if (CELL_ ## name != CELL_bonk) {				\
      field_declaration_block;					\
    }								\
    break;
#endif

#define DECLARE_WORD(x)				\
    do {					\
      assert(&(x) >= p);			\
      assert(&(x) < p + n);			\
      DECLARE_POSN(x);				\
    } while (0)
#define DECLARE_PTR(x)						\
    do {							\
      assert(&(x) >= p);					\
      assert(&(x) < p + n);					\
      assert((x) != PTR_TO_WORD(NULL));				\
      /* These shouldn't occur except in HUGE databases. */	\
      assert((((word_t) (x)) & 0x3) == 0);			\
      assert(is_in_first_generation(p)				\
	     || (x) != FIRST_GENERATION_DEADBEEF);		\
      assert((x) != PAGE_DEADBEEF);				\
      DECLARE_POSN(x);						\
    } while (0)
#define DECLARE_NONNULL_PTR(x)					\
    do {							\
      assert(&(x) >= p);					\
      assert(&(x) < p + n);					\
      assert((x) != NULL_WORD);					\
      assert((x) != PTR_TO_WORD(NULL));				\
      /* These shouldn't occur except in HUGE databases. */	\
      assert((((word_t) (x)) & 0x3) == 0);			\
      assert(is_in_first_generation(p)				\
	     || (x) != FIRST_GENERATION_DEADBEEF);		\
      assert((x) != PAGE_DEADBEEF);				\
      DECLARE_POSN(x);						\
    } while (0)
#define DECLARE_TAGGED(x)					\
    do {							\
      assert(&(x) >= p);					\
      assert(&(x) < p + n);					\
      DECLARE_POSN(x);						\
      assert(TAGGED_IS_PTR(x) || TAGGED_IS_WORD(x));		\
      if (TAGGED_IS_PTR(x)) {					\
        assert((x) != PTR_TO_WORD(NULL));			\
        /* These shouldn't occur except in HUGE databases. */	\
        assert((((word_t) (x)) & 0x3) == 0);			\
        assert(is_in_first_generation(p)			\
	       || (x) != FIRST_GENERATION_DEADBEEF);		\
        assert((x) != PAGE_DEADBEEF);				\
      }								\
    } while (0)

#include "cells-def-prep.h"

#undef CELL
#undef DECLARE_TAGGED
#undef DECLARE_NONNULL_PTR
#undef DECLARE_PTR
#undef DECLARE_WORD
#undef DECLARE_POSN

  default:
    /* Illegal type tag. */
    assert(0);
    break;
  }

#ifdef ENABLE_RED_ZONES
  for (i = 1; i < n && i < NUMBER_OF_DEADBEEFABLE_WORDS_PER_CELL; i++)
    if (is_defined[i])
      is_defined[i] = 0;
    else
      p[i] = UNDECL_WORD_CLOBBER_COOKIE;
#endif

  return 1;
}


/* As above, but do it recursively. */
int cell_check_rec(ptr_t p)
{
  word_t p0;
  int n, decl_cnt = 0;

  assert(p != NULL);
  assert(p != NULL_PTR);
  p0 = *p;
  switch (CELL_TYPE(p)) {
#define CELL(name, number_of_words, field_declaration_block)	\
  case CELL_ ## name:						\
    n = (number_of_words);					\
    if (CELL_ ## name != CELL_cont) {				\
      field_declaration_block;					\
    }								\
    break;
#define DECLARE_WORD(x)				\
    do {					\
      assert(&(x) >= p);			\
      assert(&(x) < p + n);			\
      decl_cnt++;				\
    } while (0)
#define DECLARE_PTR(x)				\
    do {					\
      assert(&(x) >= p);			\
      assert(&(x) < p + n);			\
      assert((x) != PTR_TO_WORD(NULL));		\
      assert((x) != FIRST_GENERATION_DEADBEEF);	\
      assert((x) != PAGE_DEADBEEF);		\
      assert((((word_t) (x)) & 0x3) == 0);	\
      if ((x) != NULL_WORD)			\
        cell_check_rec(WORD_TO_PTR(x));		\
      decl_cnt++;				\
    } while (0)
#define DECLARE_NONNULL_PTR(x)			\
    do {					\
      assert(&(x) >= p);			\
      assert(&(x) < p + n);			\
      assert((x) != PTR_TO_WORD(NULL));		\
      assert((x) != FIRST_GENERATION_DEADBEEF);	\
      assert((x) != PAGE_DEADBEEF);		\
      assert((((word_t) (x)) & 0x3) == 0);	\
      assert((x) != NULL_WORD);			\
      cell_check_rec(WORD_TO_PTR(x));		\
      decl_cnt++;				\
    } while (0)
#define DECLARE_TAGGED(x)					\
    do {							\
      assert(&(x) >= p);					\
      assert(&(x) < p + n);					\
      assert(TAGGED_IS_PTR(x) || TAGGED_IS_WORD(x));		\
      if (TAGGED_IS_PTR(x)) {					\
        assert((x) != PTR_TO_WORD(NULL));			\
        /* These shouldn't occur except in HUGE databases. */	\
        assert(is_in_first_generation(p)			\
	       || (x) != FIRST_GENERATION_DEADBEEF);		\
        assert((x) != PAGE_DEADBEEF);				\
        if ((x) != NULL_WORD)					\
          cell_check_rec(WORD_TO_PTR(x));			\
      }								\
      decl_cnt++;						\
    } while (0)

#include "cells-def-prep.h"

#undef CELL
#undef DECLARE_PTR
#undef DECLARE_NONNULL_PTR
#undef DECLARE_WORD

  default:
    assert(0);
    break;
  }
  /* assert(n == decl_cnt - 1); */
  return 1;
}

#endif  /* not NDEBUG */


void init_cells(void)
{
  /* Assert that the bits reserved to store the type tag are enough.  */
  assert(NUMBER_OF_CELLS <= 1 << CELL_TYPE_BITS);

  if (be_verbose)
    printf("%d of max. 256 cell types in use.\n", NUMBER_OF_CELLS);
}
