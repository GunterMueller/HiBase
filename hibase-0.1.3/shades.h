/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Cell allocation, memory management.
 */


#ifndef INCL_SHADES
#define INCL_SHADES 1

#include "includes.h"
#include "params.h"
#include "cells.h"
#include "tagged.h"
#include "bitops.h"


/* Use registers for the three most time-critical global variables.
 */

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__STRICT_ANSI__)
#if defined(sparc) || defined(__sparc)
#define USE_REGS 1
#define REG1 asm("g5")
#define REG2 asm("g6")
#define REG3 asm("g7")
#endif
#if defined(__alpha) || defined(__alpha__)
#define USE_REGS 1
#define REG1 asm("$14")
#define REG2 asm("$13")
#define REG3 asm("$12")
#endif
#endif


/* Recoverable conversions between pointers and words. */


/* Pointer to the beginning of the contiguous memory area reserved for
   the database.  Different runs, a.k.a. recoveries, of Shades may
   have different values of `mem_base', but the memory/disk image is
   equivalent.  For convenience, this is declared as a scalar value,
   not as a pointer.

   (After the memory used by database pages, follows the first
   generation.) */
#ifdef REG1
register ptr_as_scalar_t mem_base REG1;
#else
extern ptr_as_scalar_t mem_base;
#endif

/* Shades-internal counterpart to generic C "NULL". */
#define NULL_PTR  ((ptr_t) mem_base)
#define NULL_WORD  ((word_t) 0)

/* Convert a recovery-independent integer value `x' to a
   recovery-dependent pointer value. */
#define WORD_TO_PTR(x)  ((ptr_t) (mem_base + (ptr_as_scalar_t) (x)))

/* Convert a recovery-dependent pointer value `p' to a
   recovery-independent integer value. */
#define PTR_TO_WORD(x)  ((word_t) ((ptr_as_scalar_t) (x) - mem_base))


/* Memory allocation primitives. */


/* `allocate' allocates the designated number of words from the
   current commit batch.  It requires the cell type tag, thereby
   decreasing the probability that the programmer forgets to set it
   himself.  `raw_allocate' can be used to allocate override this
   precaution in some optimizations.

   `allocate' should NEVER fail.  To make sure that it never does, one
   should first call `can_allocate' which checks whether there is
   sufficient memory available, in which case it returns non-zero.  If
   there is not sufficient memory available, one should either abort
   the transaction or `flush_batch'.  Note that `can_allocate' can,
   and for sake of efficiency should, extend to cover several future
   calls to `allocate'.

   `restore_allocation_point' can be used to retract all allocation
   done since the `get_allocation_point'.  This is a useful trick in
   avoiding excess allocation in some cases.  Suppose that you `ap =
   get_allocation_point()' at the beginning of, e.g., a binary tree
   delete, construct a new path from the root to the point where the
   deleted key resides, and return the new root without issuing
   `restore_allocation_point'.  But if the key to be deleted was not
   in the tree, then we allocated the new path in vain, and by doing
   so uselessly consumed the first generation.  By issuing
   `restore_allocation_point(ap)' at the end of the unsuccessful
   deletion, we avoid this problem.  A `flush_batch' may NOT be issued
   between an `ap = get_allocation_point()' and the respective
   `restore_allocation_point(ap)'. */

#if !defined(__GNUC__) || !defined(NDEBUG) || defined(__STRICT_ANSI__)

int can_allocate(int number_of_words);
ptr_t allocate(int number_of_words, cell_type_t type);
ptr_t raw_allocate(int number_of_words);

ptr_t get_allocation_point(void);
void restore_allocation_point(ptr_t);

#ifdef ENABLE_BCPROF
extern word_t number_of_words_allocated;
#endif

/* Returns non-zero if the given pointer refers to some data item in
   the first generation. */
int is_in_first_generation(ptr_t p);

#else

extern ptr_t first_generation_start;
#ifdef REG2
register ptr_t first_generation_allocation_ptr REG2;
#else
extern ptr_t first_generation_allocation_ptr;
#endif
#ifdef REG3
register ptr_t first_generation_end REG3;
#else
extern ptr_t first_generation_end;
#endif

#ifdef ENABLE_BCPROF
extern word_t number_of_words_allocated;
#endif

extern inline int can_allocate(int number_of_words)
{
  return first_generation_allocation_ptr - number_of_words >= 
    first_generation_end;
}

extern inline ptr_t allocate(int number_of_words, cell_type_t type)
{
#ifdef ENABLE_BCPROF
  number_of_words_allocated += number_of_words;
#endif
  first_generation_allocation_ptr -= number_of_words;
  *first_generation_allocation_ptr = type << (32 - CELL_TYPE_BITS);
  return first_generation_allocation_ptr;
}

extern inline ptr_t raw_allocate(int number_of_words)
{
#ifdef ENABLE_BCPROF
  number_of_words_allocated += number_of_words;
#endif
  first_generation_allocation_ptr -= number_of_words;
  return first_generation_allocation_ptr;
}

extern inline ptr_t get_allocation_point(void)
{
  return first_generation_allocation_ptr;
}

extern inline void restore_allocation_point(ptr_t p)
{
  first_generation_allocation_ptr = p;
}

extern inline int is_in_first_generation(ptr_t p)
{
  return p >= first_generation_allocation_ptr;
}

#endif /* else __GNUC__ && !NDEBUG */


#ifdef ENABLE_RED_ZONES

/* Check the consistency and surrounding red zones of the `heaviness'
   number of newest cells in the first generation.  Called
   automatically also from several places inside `shades.c'. */
void make_first_generation_assertions(unsigned int heaviness);

/* Print debugging information about the first generation. */
void first_generation_fprint(FILE *fp, unsigned int heaviness);

#endif


/* Group commit, initialization, recovery, and asking about
   recovery. */


/* `flush_batch' finishes the current commit batch and starts a new
   one.  Returns non-zero on failure.  It moves data, so all
   C-pointers to Shades' memory need to be updated from the root
   block. */
void flush_batch(void);

/* The initialization sequence should be as follows:

     1. The main program reads its command line arguments, and
        possibly some of its own initializations.  Command line
        arguments that it processes can be assigned to NULL.
     2. The main program calls `init_shades(argc, argv)'.
     3. The main programs calls either `create_db()' or
        `recover_db()'.  The latter case hasn't yet been implemented. */

/* Returns non-zero in case of error. */
int init_shades(int argc, char **argv);

/* Create a new empty database. */
void create_db(void);

/* Recover an existing database. */
void recover_db(void);


#endif /* INCL_SHADES */
