/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Definitions and basic tools for the low-level cell types and type
 * tags understood by the shades recovery algorithm. 
 */


#ifndef INCL_CELLS
#define INCL_CELLS 1

/* Individual words may be one of these types, declared with
   `DECLARE_WORD/PTR/...' respectively. */
typedef enum {
  WORD, PTR, NONNULL_PTR, TAGGED, VOID,
  NUMBER_OF_WORD_TYPES
} word_type_t;

#define TAGGED_IS_PTR(x)  (((x) & 0x3) == 0)
#define TAGGED_IS_WORD(x)  (((x) & 0x3) == 1)
#define TAGGED_IS_OID(x)  (((x) & 0x3) == 2)


/* Define the enum that contains cell type names.
 */

#undef CELL
#define CELL(name, size, field_def_block)	\
  CELL_ ## name,

typedef enum {
#include "cells-def-prep.h"
  NUMBER_OF_CELLS
} cell_type_t;

#undef CELL


/* Declare the services of `cells.c'. */

/* The uppermost 8 bits of each cell are reserved for the type tag. */
#define CELL_TYPE_BITS  8
#define CELL_TYPE_MASK  0xFF000000
#define CELL_TYPE(p)  ((cell_type_t) (p[0] >> (32 - CELL_TYPE_BITS)))

/* Translation table from C identifiers for cell types to
   corresponding strings.  E.g. `cell_type_name[CELL_list]' evaluates
   to the string "list". */
extern const char *cell_type_name[];

/* Returns the number of words occupied by the referred cell. */
unsigned int cell_get_number_of_words(ptr_t ptr);

/* The following three calls can be used to perform memory profiling
   for a given (sub)tree.  The call sequence is e.g.
     cell_clear_size_stats();
     cell_compute_size_stats(subtree);
     cell_fprint_size_stats(stdout); */
void cell_clear_size_stats(void);
void cell_compute_size_stats(ptr_t);
void cell_fprint_size_stats(FILE *fp);

/* Make a copy of the given cell.  Assumes `can_allocate' has been
   called properly. */
ptr_t cell_copy(ptr_t p);

/* Returns non-zero if the two cells are EQUAL: they are of the same
   type and size, and corresponding word members (DECLARE_WORD) are
   bitwise the same, and corresponding pointer members (DECLARE_PTR)
   point to cells that are EQUAL. As a trivial case, if two pointers
   are EQUAL, the cells they point to are also EQUAL. */
int cell_is_equal(ptr_t p, ptr_t q);

/* Type of a user-defined more general comparison function.  E.g., if
   `a' should be less than `b', then cmp_fun(a, b, ..) returns
   CMP_LESS.  CMP_PLEASE_CONTINUE is returned by shtring_cmp_piecemeal,
   and means no difference was yet found. */
typedef enum {
  CMP_EQUAL, CMP_LESS, CMP_GREATER, CMP_UNCOMPARABLE, CMP_ERROR,
  CMP_PLEASE_CONTINUE
} cmp_result_t;

typedef cmp_result_t cmp_fun_t(ptr_t /* a */,
			       ptr_t /* b */,
			       void * /* context */);

/* Print, hopefully in a human-readable format, to `FILE *fp' the data
   structure referred to by `ptr' following pointers at most to depth
   `max_depth'. */
void cell_fprint(FILE *fp, ptr_t ptr, int max_depth);

#ifndef NDEBUG
/* Performs various low-level consistency checks on the given cell.
   These functions are so slow that we don't to use it in production
   versions by mistake. They are only available when debugging. */
int cell_check(ptr_t p);

/* Same recursively for a cell tree. */
int cell_check_rec(ptr_t p);
#endif

void init_cells(void);


#endif /* INCL_TYPES */
