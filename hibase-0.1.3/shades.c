/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Cell allocation and memory management. 

   This rather large files is loosely divided into the following
   sections:
     1) first generation management,
     2) remembered set management,
     3) page management,
     4) generation management,
     5) cell copying (for the copying collector),
     6) mature garbage collection,
     7) commit grouping,
     8) initialization and recovery routines. */


#include "includes.h"
#include "cookies.h"
#include "shades.h"
#include "root.h"
#include "io.h"
#include "cells.h"
#include "interp.h"
#include "dh.h"
/* Disabled until the KDQ becomes public.
   #include "kdqtrie.h" */
#include "smartptr.h"


static char *rev_id = "$Id: shades.c,v 1.130 1998/03/30 18:43:49 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


/* Should we show commit groups and their statistics?  Set in
   `init_shades' by command-line arguments `-v', `--verbose', and
   `--show-groups'. */
static int must_show_groups = 0;

/* Is recovery currently in progress? */
static int is_recovering = 0;

#ifdef GC_PROFILING
/* Counter for the number of bytes written to disk. Cleared in various
   places for different measures. */
static unsigned long number_of_written_bytes = 0;
#endif

/* For statistics in `test_gc.c'. */
unsigned long number_of_major_gc_written_pages = 0;
unsigned long number_of_written_pages = 0;

#define NUMBER_OF_WORDS_PER_PAGE  (PAGE_SIZE / sizeof(word_t))



/* The first generation and routines for managing it.
   
   The first generation is located after the actual database pages,
   The first generation grows starting from `first_generation_start'
   DOWNWARDS to `first_generation_end'.  The allocation is done by
   decrementing the `first_generation_allocation_ptr', and memory
   exhausts if it reaches `first_generation_end'.

   During debugging, i.e. if `NDEBUG' is not defined, then some
   features are enabled to help detecting and debugging programming
   errors.  First, additional tests and bookkeeping is performed to
   ensure all `allocate'-requests are prechecked with an appropriate
   `can_allocate'.  Second, all allocated cells are surrounded by
   small "red zone".  The red zones and the cells between them are
   occasionally checked, thereby helping to detect possible memory
   overwrites by the application code.  The highest 16 bits of the red
   zone are initialized to `FIRST_GENERATION_RED_ZONE', the lower 16
   bits designate the size of the cell allocated before the red
   zone. */

#define NUMBER_OF_WORDS_IN_FIRST_GENERATION  	\
  (first_generation_size / sizeof(word_t))

ptr_t first_generation_start;
#ifndef REG2
ptr_t first_generation_allocation_ptr;
#endif
#ifndef REG3
ptr_t first_generation_end;
#endif


#ifndef NDEBUG
/* This is used to assert that `can_allocate' is called properly. */
ptr_t first_generation_can_allocate_ptr;
#endif


#ifdef ENABLE_BCPROF
word_t number_of_words_allocated;
#endif


int can_allocate(int number_of_words)
{
#ifndef NDEBUG
  ptr_t p = first_generation_allocation_ptr - number_of_words;

#ifdef ENABLE_RED_ZONES
  make_first_generation_assertions(first_generation_assertion_heaviness);
#endif
  assert(number_of_words >= 2);
  if (p < first_generation_can_allocate_ptr)
    first_generation_can_allocate_ptr = p;
#ifdef ENABLE_RED_ZONES
  /* The factor `2' below consists of memory reservation for the red
     zones.  The `first_generation_can_allocate_ptr' is decremented by
     the amount used by the red zone later so that it maintains a more
     strict bound. */
  return first_generation_allocation_ptr - 2 * number_of_words >= 
    first_generation_end;
#else
  return first_generation_allocation_ptr - number_of_words >= 
    first_generation_end;
#endif
#else
  return first_generation_allocation_ptr - number_of_words >= 
    first_generation_end;
#endif
}


ptr_t allocate(int number_of_words, cell_type_t type)
{
#ifndef NDEBUG
  int i;
#endif
#ifdef ENABLE_RED_ZONES
  ptr_t p;
#endif

  assert(number_of_words >= 2);
  assert(number_of_words <= (signed) NUMBER_OF_WORDS_PER_PAGE - 2);

#ifdef ENABLE_BCPROF
  number_of_words_allocated += number_of_words;
#endif
  first_generation_allocation_ptr -= number_of_words;
#ifndef NDEBUG
  for (i = 0; i < number_of_words; i++)
    assert(first_generation_allocation_ptr[i] == FIRST_GENERATION_DEADBEEF);
#endif
  *first_generation_allocation_ptr = type << (32 - CELL_TYPE_BITS);
#ifdef ENABLE_RED_ZONES
  /* Make a red zone in the first generation immediately below the
     allocated cell. */
  p = first_generation_allocation_ptr;
  first_generation_allocation_ptr--;
  first_generation_can_allocate_ptr--;
  *first_generation_allocation_ptr = 
    (FIRST_GENERATION_RED_ZONE << 16) | number_of_words;
  return p;
#else
  return first_generation_allocation_ptr;
#endif
}


ptr_t raw_allocate(int number_of_words)
{
#ifndef NDEBUG
  int i;
#endif
#ifdef ENABLE_RED_ZONES
  ptr_t p;
#endif

  assert(number_of_words >= 2);
  assert(number_of_words <= (signed) NUMBER_OF_WORDS_PER_PAGE - 2);

#ifdef ENABLE_BCPROF
  number_of_words_allocated += number_of_words;
#endif
  first_generation_allocation_ptr -= number_of_words;
#ifndef NDEBUG
  for (i = 0; i < number_of_words; i++)
    assert(first_generation_allocation_ptr[i] == FIRST_GENERATION_DEADBEEF);
#endif
#ifdef ENABLE_RED_ZONES
  /* Make a red zone in the first generation immediately below the
     allocated cell. */
  p = first_generation_allocation_ptr;
  first_generation_allocation_ptr--;
  first_generation_can_allocate_ptr--;
  *first_generation_allocation_ptr = 
    (FIRST_GENERATION_RED_ZONE << 16) | number_of_words;
  return p;
#else
  return first_generation_allocation_ptr;
#endif
}


ptr_t get_allocation_point(void)
{
  return first_generation_allocation_ptr;
}


void restore_allocation_point(ptr_t p)
{
#ifndef NDEBUG
  ptr_t q;
  assert(p >= first_generation_allocation_ptr);
  for (q = first_generation_allocation_ptr; q < p; q++)
    *q = FIRST_GENERATION_DEADBEEF;
#endif
  first_generation_allocation_ptr = p;
}


int is_in_first_generation(ptr_t p)
{
  /* No pointer should ever refer to something not yet allocated or
     collected away. */
  assert(!(p >= first_generation_end && p < first_generation_allocation_ptr));

  return p >= first_generation_allocation_ptr;
}


static void clear_first_generation(void)
{
#ifndef NDEBUG
  ptr_t p;

  for (p = first_generation_end; p < first_generation_start; p++)
    *p = FIRST_GENERATION_DEADBEEF;
#endif
  first_generation_allocation_ptr = first_generation_start;
#ifndef NDEBUG
  first_generation_can_allocate_ptr = first_generation_start;
#endif
}


#ifdef ENABLE_RED_ZONES

/* These functions are so slow that we don't to use it in production
   versions by mistake. They are only available during heavy debugging. */

void make_first_generation_assertions(unsigned int heaviness)
{
  ptr_t p;
  unsigned int i;

  /* General sanity of the allocation pointers. */
  assert(first_generation_allocation_ptr >= first_generation_can_allocate_ptr);
  assert(first_generation_allocation_ptr >= first_generation_end);

  /* Check that the unallocated part of the first generation is
     virgin. */
  if (first_generation_end + heaviness > first_generation_allocation_ptr)
    p = first_generation_end;
  else
    p = first_generation_allocation_ptr - heaviness;
  for (; p < first_generation_allocation_ptr; p++)
    assert(*p == FIRST_GENERATION_DEADBEEF);
  
  /* Check the linked list of red zones. */
  assert(p == first_generation_allocation_ptr);
  for (i = 0; i < heaviness && p < first_generation_start - 1; i++) {
    unsigned int s;
    assert(*p >> 16 == FIRST_GENERATION_RED_ZONE);
    s = cell_get_number_of_words(p + 1);
    assert((*p & 0xFFFF) >= s);
    assert(cell_check(p + 1));
    p += 1 + (*p & 0xFFFF);
  }
}


void first_generation_fprint(FILE *fp, unsigned int heaviness)
{
  ptr_t p;
  unsigned int i, dp;

  if (first_generation_allocation_ptr < first_generation_can_allocate_ptr)
    fprintf(fp, "fg_allocation_ptr < fg_can_allocate_ptr    ERROR\n");
  if (first_generation_allocation_ptr < first_generation_end)
    fprintf(fp, "fg_allocation_ptr < fg_end    ERROR\n");

  /* Check that the unallocated part of the first generation is
     virgin. */
  if (first_generation_end + heaviness > first_generation_allocation_ptr)
    p = first_generation_end;
  else
    p = first_generation_allocation_ptr - heaviness;
  for (; p < first_generation_allocation_ptr; p++) {
    fprintf(fp, "0x%08lX    0x%08lX", (unsigned long) p, (unsigned long) *p);
    if (*p == FIRST_GENERATION_DEADBEEF)
      fprintf(fp, "\n");
    else
      fprintf(fp, "    ERROR\n");
  }

  /* Check the linked list of red zones. */
  assert(p == first_generation_allocation_ptr);
  for (i = 0; i < heaviness && p < first_generation_start - 1; i++) {
    unsigned int s;
    fprintf(fp, "0x%08lX    0x%08lX (%d)\n",
	    (unsigned long) p,
	    (unsigned long) *p,
	    (unsigned int) (*p & 0xFFFF));
    if (*p >> 16 != FIRST_GENERATION_RED_ZONE)
      fprintf(fp, "ERROR  (No red zone cookie, probably overwritten!)\n");
    s = cell_get_number_of_words(p + 1);
    if ((*p & 0xFFFF) < s) {
      fprintf(fp, "ERROR  (Too small red zone jump!)\n");
      dp = s;
    } else
      dp = *p & 0xFFFF;
    cell_fprint(fp, p + 1, 1);
    p += 1 + dp;
  }
}

#endif


/* Remembered sets.

   The remembered set is a transient structure that stores the
   addresses of memory locations that store pointers that refer to the
   area that the remembered set is dedicated for.  The remembered set
   is implemented as a linked list, but so that several consecutive
   data items are combined to a single node.  This decreases memory
   consumption by the pointers.

   Unconventionally the list does not end with `NULL', but the
   `REM_SET_TAIL_COOKIE'.  Because additionally the
   `rem_set_allocation_ptr' of an empty remembered set is initialized
   to `NULL', we can test for both for rem set being an empty list and
   chunk exhaustion with `rem_set_allocation_ptr < rem_set' in
   `PREPEND_TO_REM_SET'.

   The unused remembered set nodes are organized in a freelist, and
   new nodes are allocated in larger chunks, not individually. */


#define REM_SET_SIZE 40


#ifdef GC_PROFILING
unsigned long current_rem_set_size = 0;
unsigned long max_rem_set_size = 0;
#endif


typedef struct rem_set_t {
  word_t referrer[REM_SET_SIZE];
  struct rem_set_t *next;
} rem_set_t;


static rem_set_t *rem_set_free_list = NULL;

static rem_set_t *allocate_rem_set(void)
{
  rem_set_t *p;
  int i;

  if (rem_set_free_list == NULL) {
    rem_set_free_list =
      (rem_set_t *) malloc(rem_sets_per_malloc * sizeof(rem_set_t));
    if (rem_set_free_list == NULL) {
      fprintf(stderr, "allocate_rem_set: malloc failed.\n");
      exit(1);
    }
    /* Construct a free list. */
    for (i = 0; i < rem_sets_per_malloc - 1; i++)
      rem_set_free_list[i].next = &rem_set_free_list[i+1];
    rem_set_free_list[rem_sets_per_malloc - 1].next = NULL;
  }

  /* Pop an item from the remembered set. */
  p = rem_set_free_list;
  rem_set_free_list = rem_set_free_list->next;
  return p;
}


#ifdef GC_PROFILING

#define PREPEND_TO_REM_SET(rem_set, ref, rem_set_allocation_ptr)	\
  do {									\
    current_rem_set_size++;						\
    if (current_rem_set_size >= max_rem_set_size)			\
      max_rem_set_size = current_rem_set_size;				\
    if ((rem_set_allocation_ptr) >= &(rem_set)->referrer[0])		\
      *(rem_set_allocation_ptr)-- = PTR_TO_WORD(ref);			\
    else {								\
      rem_set_t *p = allocate_rem_set();				\
      p->next = (rem_set);						\
      (rem_set) = p;							\
      p->referrer[REM_SET_SIZE - 1] = PTR_TO_WORD(ref);			\
      (rem_set_allocation_ptr) = &p->referrer[REM_SET_SIZE - 2];	\
    }									\
  } while (0)

#else

#define PREPEND_TO_REM_SET(rem_set, ref, rem_set_allocation_ptr)	\
  do {									\
    if ((rem_set_allocation_ptr) >= &(rem_set)->referrer[0])		\
      *(rem_set_allocation_ptr)-- = PTR_TO_WORD(ref);			\
    else {								\
      rem_set_t *p = allocate_rem_set();				\
      p->next = (rem_set);						\
      (rem_set) = p;							\
      p->referrer[REM_SET_SIZE - 1] = PTR_TO_WORD(ref);			\
      (rem_set_allocation_ptr) = &p->referrer[REM_SET_SIZE - 2];	\
    }									\
  } while (0)

#endif


static void free_rem_set(rem_set_t *rem_set, ptr_t rem_set_allocation_ptr)
{
  rem_set_t *p;

  if (rem_set == REM_SET_TAIL_COOKIE)
    return;
#ifdef GC_PROFILING
  current_rem_set_size -=
    &rem_set->referrer[REM_SET_SIZE - 1] - rem_set_allocation_ptr;
  for (p = rem_set->next; p != REM_SET_TAIL_COOKIE; p = p->next)
    current_rem_set_size -= REM_SET_SIZE;
#endif
  /* Compute to `p' to the last item of the rem_set list. */
  for (p = rem_set; p->next != REM_SET_TAIL_COOKIE; p = p->next)
    ;
  /* Prepend the list `rem_set' to `p' to the `rem_set_free_list'. */
  p->next = rem_set_free_list;
  rem_set_free_list = rem_set;
}


/* Page management.

   For each page we have three distinct data items:
     1. The actual memory image of the raw data of the page.  In
        various conversion macro names `PAGE_PTR' denotes the pointer
        to the beginning of the raw data of the page.  The first word
        of the page contains a `PAGE_MAGIC_COOKIE' and the second word
        denotes the number of words used in that page.  Main memory
        pages are identified by the integer type `page_number_t'.
     2. The disk image of the page.  See the file `io.h' for the
        interfaces available to manipulating disk pages.  Disk pages
        are identified by `disk_page_number_t's.  Note that although
        `io.c' is able to choose a disk page number for you,
        `shades.c' will have to tell `io.c' during recovery which disk
        pages are free and which are used.
     3. A transient descriptor of various meta-information related to
        the page.  These are stored in the array `page_info' indexed by
        page numbers.
  In this section we implement some basic tools for manipulating the
  first and third items.

  Also pages are treated slightly differently if the system is
  compiled for debugging (without defining `NDEBUG').  In particular,
  the freed pages are "deadbeefed" with `PAGE_DEADBEEF' in hope to
  catch any possible references to freed pages.  When the page is
  allocated the deadbeefs are checked in hope to find incorrect
  writing to them. */

/* Pointer to the beginning of the contiguous memory area reserved for
   Shades.  Equals to the first word (its magic cookie) of the first
   main memory page. */
#ifndef REG1
/* Otherwise defined to REG1 in `shades.h'. */
ptr_as_scalar_t mem_base = 0;
#endif

/* The number of pages in the main memory image of the database.
   A frequently used parameter-dependent value.  Initialized in
   `init_shades' to `db_size / PAGE_SIZE'. */
static unsigned long number_of_pages = 0;

/* The page at `mem_base' has the page number 0, the next page has
   page number 1, etc. up to the last page which has page number
   `number_of_pages - 1'. */
typedef word_t page_number_t;

#define INVALID_PAGE_NUMBER ((page_number_t) ~0L)


/* Declarations for the transient page information. */

/* Forward declaration. */
typedef struct generation_info_t generation_info_t;

/* A transient descriptor of various control information related to
   the main memory data pages. */
typedef struct page_info_t {
  /* Pointer to the generation which that this page is part of. */
  generation_info_t *generation;
  int is_allocated;
  /* Used as a link in the linked list of free pages, though not
     during recovery. */
  page_number_t next_free_page;
#if SIZEOF_INT_P == 4
  /* Padding that makes the size of the `page_info' to 16.  This is
     useful in some time-critical routines as in `copy_cell' and
     `drain_copy_stack' so that indexing and de-indexing the
     `page_info' array can be done as efficiently as possible. */
  word_t pad[1];
#endif
} page_info_t;

/* An array indexed by the page number.  Allocated in `init_shades'. */
static page_info_t *page_info = NULL;


/* Some conversion and accessor macros. */

/* Convert the page number into the pointer to the first word on the
   page. */
#define PAGE_NUMBER_TO_PAGE_PTR(pn)		\
  ((ptr_t) WORD_TO_PTR(PAGE_SIZE * (pn)))

/* Convert a pointer to data on the page to the page number.  This
   requires `mem_base' to be page-aligned. */
#define PTR_TO_PAGE_NUMBER(p)			\
  (PTR_TO_WORD(p) >> (ILOG2(PAGE_SIZE) - 1))

/* Convert a pointer to a cell to a pointer to its page and generation
   infos respectively. */
#define PTR_TO_PAGE_INFO(p)			\
  (&page_info[PTR_TO_PAGE_NUMBER(p)])
#define PTR_TO_GENERATION_INFO(p)		\
  (PTR_TO_PAGE_INFO(p)->generation)

/* Macros for getting and setting the number of words in use. */
#define PAGE_SET_NUMBER_OF_WORDS_IN_USE(pn, n)				     \
  do {									     \
    assert((unsigned long) (n) <= (unsigned long) NUMBER_OF_WORDS_PER_PAGE); \
    PAGE_NUMBER_TO_PAGE_PTR(pn)[1] = (n);				     \
  } while (0)
#define PAGE_GET_NUMBER_OF_WORDS_IN_USE(pn)		\
  ((unsigned long) (PAGE_NUMBER_TO_PAGE_PTR(pn)[1]))


/* Free page management. */

static page_number_t list_of_free_pages = INVALID_PAGE_NUMBER;

static unsigned long number_of_free_pages = 0;


/* Free the given page.  If the system is no longer recovering, then
   prepend the page to the global list of free pages. */
static void free_page(page_number_t pn)
{
#ifndef NDEBUG
  unsigned long i;

  assert(PAGE_NUMBER_TO_PAGE_PTR(pn)[0] == PAGE_MAGIC_COOKIE);
  assert(page_info[pn].is_allocated);
  for (i = 1; i < NUMBER_OF_WORDS_PER_PAGE; i++)
    PAGE_NUMBER_TO_PAGE_PTR(pn)[i] = PAGE_DEADBEEF;
#endif
  page_info[pn].is_allocated = 0;
  page_info[pn].generation = NULL;
  if (!is_recovering) {
    page_info[pn].next_free_page = list_of_free_pages;
    list_of_free_pages = pn;
  }
  number_of_free_pages++;
}


static page_number_t allocate_page(void)
{
  page_number_t pn;
#ifndef NDEBUG
  unsigned long i;
#endif

  if (list_of_free_pages == INVALID_PAGE_NUMBER) {
    /* The list of free pages is exhausted. */
    fprintf(stderr, "allocate_page: Out of main memory\n");
    exit(1);
  }
  pn = list_of_free_pages;
  list_of_free_pages = page_info[list_of_free_pages].next_free_page;
#ifndef NDEBUG
  assert(!page_info[pn].is_allocated);
  assert(PAGE_NUMBER_TO_PAGE_PTR(pn)[0] == PAGE_MAGIC_COOKIE);
  for (i = 1; i < NUMBER_OF_WORDS_PER_PAGE; i++)
    assert(PAGE_NUMBER_TO_PAGE_PTR(pn)[i] == PAGE_DEADBEEF);
  assert(!is_recovering);	/* Use `rvy_allocate_page' during recovery. */
#endif
  page_info[pn].is_allocated = 1;
  number_of_free_pages--;
  return pn;
}


static void rvy_allocate_page(page_number_t pn)
{
  assert(PAGE_NUMBER_TO_PAGE_PTR(pn)[0] == PAGE_MAGIC_COOKIE);
  /* Note that the same memory page can quite legally be allocated
     several times during recovery for different major gc rounds.
     Therefore the deadbeefs may no longer be intact. */
  if (!page_info[pn].is_allocated)
    number_of_free_pages--;
  page_info[pn].is_allocated = 1;
}


/* `construct_page_freelist' is called at the end of recovery to
   explicitely free all pages that were not taken to use, and
   construct a free list of them. */
static void construct_page_freelist(void)
{
  page_number_t pn;

  for (pn = 0; pn < number_of_pages; pn++)
    if (!page_info[pn].is_allocated) {
      page_info[pn].generation = NULL;
      page_info[pn].next_free_page = list_of_free_pages;
      list_of_free_pages = pn;
    }
}


/* Generation management. 

   Generations are sets of pages that contain cells of roughly the
   same age.  Each generation consists of a varying number of pages,
   possibly none, but at most of `first_generation_size / PAGE_SIZE'
   pages. */

/* Generation numbers are arbitrary unique numbers.  They don't
   reflect the generation's age. */
typedef word_t generation_number_t;

#define INVALID_GENERATION_NUMBER  ((generation_number_t) ~0L)


/* A transient descriptor of various control information related to
   generations.  The status of a generation is `COLLECTED_ONCE' if it
   has been collected and no longer exists in main memory, but still
   exists on disk. */
struct generation_info_t {
  enum {
    NONEXISTENT, 
    NORMAL, TO_BE_COLLECTED, BEING_COLLECTED, 
    COLLECTED_ONCE, COLLECTED_TWICE
  } status;
  rem_set_t *rem_set;
  ptr_t rem_set_allocation_ptr;
  /* The set of pages that collectively form the generation.  Use
     `npages' instead of `number_of_pages' consistently in order to
     avoid name clash with the global variable. */
  int npages;
  page_number_t *page;
  disk_page_number_t *disk_page;
  /* Generations are organized in various list-like structures, all
     treated below.  Some of them are mutually redundant, but
     currently kept distinct for sake of code clarity. */
  /* Generations that are either NORMAL, TO_BE_COLLECTED or
     BEING_COLLECTED belong in a doubly linked list according to their
     age.  The linked list is chained together by the integer fields
     below.  (This was a little more handier during debugging than
     using pointer values.) */
  generation_number_t younger, older;
  /* Generations that have been COLLECTED_ONCE or COLLECTED_TWICE are
     from-generations for a to-generation in the sense that the live
     cells have been copied from the from-generation to the
     to-generation.  Each from-generation belongs to a linked list
     which starts from the child generations `from'-field and is
     linked together with `next_from'-fields. */
  generation_number_t from, next_from;
  /* Used to link together generations that have been
     COLLECTED_TWICE. */
  generation_number_t next_collected_twice;

  /* Statistics for heuristics that control additional generationality.

     XXX Tentative, can't recover these. */
  double shrinkage;
};

/* An array indexed by the generation number.  Allocated and extended
   by `generation_info_grow'. */
static generation_info_t *generation_info = NULL;

/* The youngest recoverable generation (not the first generation). */
static generation_number_t youngest_gn = INVALID_GENERATION_NUMBER;

/* The generation being currently constructred.  This is an implicit
   argument and return value in many functions. */
static generation_number_t to_gn = INVALID_GENERATION_NUMBER;

/* List of generations COLLECTED_TWICE.  After the root block has been
   successfully written, the generations in this list are made
   NONEXISTENT and their disk pages freed. */
static generation_number_t 
  generations_collected_twice = INVALID_GENERATION_NUMBER;

/* These are currently used for keeping `generation_info' in
   suitable size. */
static unsigned long number_of_generations = 0;
static unsigned long number_of_nonexistent_generations = 0;

/* How much do mature generations on average shrink when collected? 

   XXX Not recovered. Is this the right place for defining this? */
static double avg_generation_shrinkage = 0;


/* Allocate or extend `generation_info' to contain at least
   `new_number_of_generations' generations. */
static void generation_info_grow(unsigned long new_number_of_generations)
{
  generation_number_t gn;
  generation_info_t *old_generation_info;
  page_number_t pn;

  if (new_number_of_generations <= number_of_generations)
    return;
  old_generation_info = generation_info;
  generation_info =
    realloc(old_generation_info,
	    new_number_of_generations * sizeof(generation_info_t));
  if (generation_info == NULL) {
    fprintf(stderr, "allocate_generation: `realloc' failed for %lu gens.\n",
	    new_number_of_generations);
    exit(1);
  }
  /* Mark all new generations NONEXISTENT. */
  for (gn = number_of_generations; gn < new_number_of_generations; gn++) {
    number_of_nonexistent_generations++;
    generation_info[gn].status = NONEXISTENT;
  }
  number_of_generations = new_number_of_generations;
  /* Redirect the `generation'-pointers of pages to refer to the new
     `generation_info'. */
  for (pn = 0; pn < number_of_pages; pn++)
    if (page_info[pn].is_allocated) {
      gn = page_info[pn].generation - old_generation_info;
      page_info[pn].generation = &generation_info[gn];
    }
}


/* Put `to_gn' after `younger_gn' in the doubly linked list of
   generations.  If `younger_gn' is `INVALID_GENERATION_NUMBER', then
   make `to_gn' the `youngest_gn'. */
static void insert_generation_after(generation_number_t younger_gn)
{
  generation_number_t older_gn;

  generation_info[to_gn].younger = younger_gn;
  if (younger_gn == INVALID_GENERATION_NUMBER) {
    generation_info[to_gn].older = youngest_gn;
    if (youngest_gn != INVALID_GENERATION_NUMBER)
      generation_info[youngest_gn].younger = to_gn;
    youngest_gn = to_gn;
  } else {
    older_gn = generation_info[younger_gn].older;
    generation_info[to_gn].older = older_gn;
    if (older_gn != INVALID_GENERATION_NUMBER)
      generation_info[older_gn].younger = to_gn;
    generation_info[younger_gn].older = to_gn;
  }
}


/* Allocate a new generation number to `to_gn'. */
static void allocate_generation(void)
{
  static generation_number_t gn = 0;

  /* If there are too few free `generation_info' slots left, then
     double the size of the `generation_info' table. */
  if (number_of_nonexistent_generations * 8 < number_of_generations)
    generation_info_grow(2 * number_of_generations);
  while (generation_info[gn].status != NONEXISTENT)
    gn = (gn + 1) % number_of_generations;
  to_gn = gn;
  number_of_nonexistent_generations--;
  generation_info[to_gn].status = NORMAL;
  generation_info[to_gn].rem_set = REM_SET_TAIL_COOKIE;
  generation_info[to_gn].rem_set_allocation_ptr = NULL;
  generation_info[to_gn].npages = 0;
  generation_info[to_gn].page = NULL;
  generation_info[to_gn].disk_page = NULL;
  generation_info[to_gn].from = INVALID_GENERATION_NUMBER;
  generation_info[to_gn].next_from = INVALID_GENERATION_NUMBER;
  generation_info[to_gn].next_collected_twice = INVALID_GENERATION_NUMBER;
  generation_info[to_gn].younger = INVALID_GENERATION_NUMBER;
  generation_info[to_gn].older = INVALID_GENERATION_NUMBER;
  generation_info[to_gn].shrinkage = 1;
}


/* Similar to above, but called during recovery when we know the
   number of pages, i.e. `to_gn' has already been set. */
static void rvy_allocate_generation(int npages)
{
  generation_number_t older_gn;

  generation_info_grow(to_gn + 1);
  assert(generation_info[to_gn].status != NORMAL
	 && generation_info[to_gn].status != TO_BE_COLLECTED
	 && generation_info[to_gn].status != BEING_COLLECTED);
  number_of_nonexistent_generations--;
  generation_info[to_gn].status = NORMAL;
  generation_info[to_gn].rem_set = REM_SET_TAIL_COOKIE;
  generation_info[to_gn].rem_set_allocation_ptr = NULL;
  generation_info[to_gn].npages = npages;
  if (npages != 0) {
    generation_info[to_gn].page = 
      malloc(npages * sizeof(page_number_t));
    if (generation_info[to_gn].page == NULL) {
      fprintf(stderr, 
	      "rvy_allocate_generation: malloc failed for %d pages.\n",
	      npages);
      exit(1);
    }
    generation_info[to_gn].disk_page = 
      malloc(npages * sizeof(disk_page_number_t));
    if (generation_info[to_gn].disk_page == NULL) {
      fprintf(stderr, 
	      "rvy_allocate_generation: malloc failed for %d disk pages.\n",
	      npages);
      exit(1);
    }
  }
  generation_info[to_gn].from = INVALID_GENERATION_NUMBER;
  generation_info[to_gn].next_from = INVALID_GENERATION_NUMBER;
  generation_info[to_gn].next_collected_twice = INVALID_GENERATION_NUMBER;
  generation_info[to_gn].younger = INVALID_GENERATION_NUMBER;
  generation_info[to_gn].older = INVALID_GENERATION_NUMBER;
  generation_info[to_gn].shrinkage = 1;
}


/* Called after successful commit groups to finally free the
   generation number, its `disk_pages' array, and particularly the
   disk pages that have until now been reserved in order to insure
   recoverability. */
static void mark_twice_collected_generations_nonexistent(void)
{
  int i;
  generation_number_t gn = generations_collected_twice;

  while (gn != INVALID_GENERATION_NUMBER) {
    generation_info[gn].status = NONEXISTENT;
    number_of_nonexistent_generations++;
    for (i = 0; i < generation_info[gn].npages; i++)
      io_free_disk_page(generation_info[gn].disk_page[i]);
    if (generation_info[gn].npages != 0)
      free(generation_info[gn].disk_page);
    gn = generation_info[gn].next_collected_twice;
  }
  generations_collected_twice = INVALID_GENERATION_NUMBER;
}


static void mark_generation_collected_twice(generation_number_t gn)
{
  generation_info[gn].status = COLLECTED_TWICE;
  generation_info[gn].next_collected_twice = generations_collected_twice;
  generations_collected_twice = gn;
}


static void mark_generation_collected_once(generation_number_t gn)
{
  int i;

  generation_info[gn].status = COLLECTED_ONCE;
  /* Free all memory pages in `gn'. */
  for (i = 0; i < generation_info[gn].npages; i++)
    free_page(generation_info[gn].page[i]);
  if (generation_info[gn].npages != 0)
    free(generation_info[gn].page);
  /* Unlink `gn' from the doubly linked list of generations. */
  if (generation_info[gn].older != INVALID_GENERATION_NUMBER)
    generation_info[generation_info[gn].older].younger =
      generation_info[gn].younger;
  if (generation_info[gn].younger != INVALID_GENERATION_NUMBER)
    generation_info[generation_info[gn].younger].older =
      generation_info[gn].older;
  else {
    assert(gn == youngest_gn);
    youngest_gn = generation_info[gn].older;
  }
  /* Mark all from-generations collected twice. */
  for (gn = generation_info[gn].from;
       gn != INVALID_GENERATION_NUMBER;
       gn = generation_info[gn].next_from)
    mark_generation_collected_twice(gn);
}


#ifndef NDEBUG

static int check_generations(void)
{
  generation_number_t gn, older_gn;

  /* Check that the valid generations indeed form a double linked
     list. */
  gn = youngest_gn;
  assert(generation_info[gn].younger == INVALID_GENERATION_NUMBER);
  do {
    older_gn = generation_info[gn].older;
    if (older_gn != INVALID_GENERATION_NUMBER)
      assert(generation_info[older_gn].younger == gn);
    if (generation_info[gn].status == TO_BE_COLLECTED)
      assert(generation_info[gn].status != BEING_COLLECTED);
    gn = older_gn;
  } while (gn != INVALID_GENERATION_NUMBER);

  return 1;
}

#endif


/* Create in first generation a new `generation_pinfo' that
   corresponds to the transient data of `youngest_gn' and prepend it
   to the `generation_pinfo_list' addressed from root pointer. */
static void 
  log_to_generation_pinfo_list(unsigned int number_of_from_generations,
			       unsigned long number_of_referring_ptrs)
{
  unsigned long i, npages;
  ptr_t p, prev_p;

  /* Allocate the generation_pinfo and initialize the first two fields
     in it and the corresponding root block info. */
  npages = generation_info[to_gn].npages;
  assert(npages <= MAX_GENERATION_SIZE);
  /* The first generation is empty, we must be able to allocate. */
  assert(can_allocate(4 + 2*npages));
  p = allocate(4 + 2*npages, CELL_generation_pinfo);
  p[0] |= npages | (number_of_from_generations << 12);
  /* Find out where the previous generation info was written on disk,
     and store that info in the root block. */
  p[1] = PTR_TO_WORD(GET_ROOT_PTR(generation_pinfo_list));
  p[2] = to_gn;
  p[3] = number_of_referring_ptrs;
  /* Copy the individual page numbers and their disk page numbers. */
  for (i = 0; i < npages; i++) {
    p[4 + 2*i] = generation_info[to_gn].page[i];
    p[4 + 2*i + 1] = generation_info[to_gn].disk_page[i];
  }
  SET_ROOT_PTR(generation_pinfo_list, p);
}


/* Similarly to above, update the `youngest_generation_...'-fields in
   the root block to reflect the current `youngest_gn' */
static void
  cache_generation_pinfo_to_root(unsigned long number_of_referring_ptrs)
{
  unsigned long i, npages;
  ptr_t p, prev_p;

  /* Allocate the generation_pinfo and initialize the first two fields
     in it and the corresponding root block info. */
  npages = generation_info[youngest_gn].npages;
  assert(npages <= MAX_GENERATION_SIZE);
  /* The first generation is empty, we must be able to allocate. */
  SET_ROOT_WORD(youngest_generation_number_of_pages, npages);
  SET_ROOT_WORD(youngest_generation_number, youngest_gn);
  SET_ROOT_WORD(youngest_generation_number_of_referring_ptrs,
		number_of_referring_ptrs);
  /* Copy the individual page numbers and their disk page numbers. */
  for (i = 0; i < npages; i++) {
    SET_ROOT_WORD_VECTOR(youngest_generation_page_number0,
			 2*i,
			 generation_info[youngest_gn].page[i]);
    SET_ROOT_WORD_VECTOR(youngest_generation_disk_page_number0, 
			 2*i,
			 generation_info[youngest_gn].disk_page[i]);
  }
}


/* Routines for the stop & copy collector.
 */

/* The page number of the page which cells are being copied to. */
static page_number_t to_pn;

/* Pointer to where the next cell will be copied to. */
static ptr_t to_ptr;

/* Pointer to the end of the page, i.e. the last legal place we can
   copy to without switching to the next page. */
static ptr_t to_end;


static void start_gc(void)
{
  to_ptr = to_end = NULL;
  to_pn = INVALID_PAGE_NUMBER;
}


static void new_to_page(void)
{
  int npages;

  /* Wrap up possible previous copy page. */
  if (to_pn != INVALID_PAGE_NUMBER)
    PAGE_SET_NUMBER_OF_WORDS_IN_USE(to_pn,
				    to_ptr - PAGE_NUMBER_TO_PAGE_PTR(to_pn));
  /* Allocate a new page and insert it into the `to_gn'. */
  to_pn = allocate_page();
  npages = ++generation_info[to_gn].npages;
  generation_info[to_gn].page =
    realloc(generation_info[to_gn].page, npages * sizeof(page_number_t));
  if (generation_info[to_gn].page == NULL) {
    fprintf(stderr, "new_to_page: `realloc' failed for %dth page.\n", npages);
    exit(1);
  }
  generation_info[to_gn].page[npages - 1] = to_pn;
  generation_info[to_gn].disk_page =
    realloc(generation_info[to_gn].disk_page,
	    npages * sizeof(disk_page_number_t));
  if (generation_info[to_gn].disk_page == NULL) {
    fprintf(stderr, "new_to_page: `realloc' failed for %dth disk page.\n",
	    npages);
    exit(1);
  }
  page_info[to_pn].generation = &generation_info[to_gn];
  /* As described in the page management section, the first word is
     reserved for the PAGE_MAGIC_COOKIE and to dedicate `NULL_PTR' as
     an invalid data.  The second word to store the number of words in
     use. */
  to_ptr = PAGE_NUMBER_TO_PAGE_PTR(to_pn) + 2;
  to_end = PAGE_NUMBER_TO_PAGE_PTR(to_pn) + NUMBER_OF_WORDS_PER_PAGE;
}


static void finish_gc(void)
{
  int i;
  page_number_t pn;

  if (to_pn == INVALID_PAGE_NUMBER)
    /* No gc actually commenced, nothing to finish. */
    return;
  /* Wrap up the previous to-page. */
  PAGE_SET_NUMBER_OF_WORDS_IN_USE(to_pn,
				  to_ptr - PAGE_NUMBER_TO_PAGE_PTR(to_pn));
  /* Write the memory pages to disk. */
  for (i = 0; i < generation_info[to_gn].npages; i++) {
    pn = generation_info[to_gn].page[i];
    generation_info[to_gn].disk_page[i] =
      io_write_page(PAGE_NUMBER_TO_PAGE_PTR(pn),
		    PAGE_GET_NUMBER_OF_WORDS_IN_USE(pn) * sizeof(word_t));
    /* Statistics. */
    number_of_written_pages++;
    number_of_major_gc_written_pages++;
#ifdef GC_PROFILING
    number_of_written_bytes +=
      PAGE_GET_NUMBER_OF_WORDS_IN_USE(pn) * sizeof(word_t);
#endif
  }
}


/* The recursion in `copy_cell' and `drain_copy_stack' has been
   replaced by iteration and an auxiliary stack which is defined
   below.  (This was found experimentally to be more CPU-efficient
   than the alternative Cheney-style breadth-first copier, probably
   because of less dispatching.) */

static ptr_t *copy_stack = NULL;
static ptr_t *copy_stack_ptr;

#define CLEAR_COPY_STACK			\
  do {						\
    copy_stack_ptr = copy_stack;		\
  } while (0)
#define COPY_STACK_IS_EMPTY  (copy_stack_ptr == copy_stack)
#define COPY_STACK_DEPTH  (copy_stack_ptr - copy_stack)
#define PUSH_TO_COPY_STACK(pp)  (*copy_stack_ptr++ = (pp))
#define POP_FROM_COPY_STACK  (*--copy_stack_ptr)


/* Copy the cell referred to by `*pp', redirect `*pp' to refer to the
   copied cell, and put all references from `*pp' to the copy
   stack.  

   The functions `copy_cell' and `drain_copy_stack' are the single
   most time-critical routines in the whole system.  Considerable
   effort has been put in making them as efficient as possible. */
static void copy_cell(ptr_t pp)
{
  word_t p0, new_x;
  ptr_t p, new_p, new_px;

  assert(pp != NULL_PTR);
  assert(*pp != NULL_WORD);
  p = WORD_TO_PTR(*pp);

  p0 = p[0];
  switch (CELL_TYPE(p)) {
    /* Note: The outermost if-statement for a forward-pointer
       should be constant-folded away by the compiler. */
#define CELL(name, number_of_words, field_definition_block)	\
  case CELL_ ## name:						\
    if (CELL_ ## name == CELL_forward_pointer) {		\
      *pp = p[1];						\
      return;							\
    } else {							\
      new_p = to_ptr;						\
      to_ptr += (number_of_words);				\
      if (to_ptr > to_end) {					\
        to_ptr = new_p;						\
        new_to_page();						\
        new_p = to_ptr;						\
        to_ptr += (number_of_words);				\
      }								\
      new_p[0] = p0;						\
      field_definition_block;					\
    }								\
    break;
#define DECLARE_WORD(x)				\
    new_p[&(x) - p] = (x)
#define DECLARE_PTR(x)				\
    do {					\
      assert((x) != FIRST_GENERATION_DEADBEEF);	\
      new_x = (x);				\
      new_px = new_p + (&(x) - p);		\
      *new_px = new_x;				\
      if (new_x != NULL_WORD)			\
        PUSH_TO_COPY_STACK(new_px);		\
    } while (0)
#define DECLARE_NONNULL_PTR(x)			\
    do {					\
      assert((x) != FIRST_GENERATION_DEADBEEF);	\
      new_x = (x);				\
      new_px = new_p + (&(x) - p);		\
      *new_px = new_x;				\
      assert(new_x != NULL_WORD);		\
      PUSH_TO_COPY_STACK(new_px);		\
    } while (0)
#define DECLARE_TAGGED(x)				\
    do {						\
      new_x = (x);					\
      new_px = new_p + (&(x) - p);			\
      *new_px = new_x;					\
      if (TAGGED_IS_PTR(new_x) && new_x != NULL_WORD)	\
        PUSH_TO_COPY_STACK(new_px);			\
    } while (0)

#include "cells-def-prep.h"

#undef CELL
#undef DECLARE_PTR
#undef DECLARE_NONNULL_PTR
#undef DECLARE_WORD
#undef DECLARE_TAGGED

#ifndef NDEBUG
  default:
    abort();
#endif
  }
  /* Set up a forward pointer. */
  p[0] = CELL_forward_pointer << (32 - CELL_TYPE_BITS);
  *pp = p[1] = PTR_TO_WORD(new_p);
  assert(copy_stack_ptr < copy_stack + NUMBER_OF_WORDS_IN_FIRST_GENERATION);
}


static void drain_copy_stack(void)
{
  word_t p0, new_x;
  ptr_t pp, p, new_p, new_px, reg_to_ptr = to_ptr, reg_to_end = to_end;
  generation_info_t *gni;

  while (!COPY_STACK_IS_EMPTY) {
    pp = POP_FROM_COPY_STACK;
    assert(*pp != NULL_WORD);
    p = WORD_TO_PTR(*pp);

    if (!is_in_first_generation(p)) {
      gni = PTR_TO_GENERATION_INFO(p);
      if (gni->status == NORMAL)
	continue;
      if (gni->status == TO_BE_COLLECTED) {
	PREPEND_TO_REM_SET(gni->rem_set, pp, gni->rem_set_allocation_ptr);
	continue;
      }
      assert(gni->status == BEING_COLLECTED);
    }

    p0 = p[0];
    switch (CELL_TYPE(p)) {
#define CELL(name, number_of_words, field_definition_block)	\
    case CELL_ ## name:						\
      if (CELL_ ## name == CELL_forward_pointer) {		\
        *pp = p[1];						\
        continue;						\
      } else {							\
        new_p = reg_to_ptr;					\
        reg_to_ptr += (number_of_words);			\
        if (reg_to_ptr > reg_to_end) {				\
          to_ptr = new_p;					\
          new_to_page();					\
	  new_p = to_ptr;					\
          reg_to_end = to_end;					\
          reg_to_ptr = to_ptr + (number_of_words);		\
        }							\
        new_p[0] = p0;						\
        field_definition_block;					\
      }								\
      break;
#define DECLARE_WORD(x)				\
      new_p[&(x) - p] = (x)
#define DECLARE_PTR(x)					\
      do {						\
        assert((x) != FIRST_GENERATION_DEADBEEF);	\
        new_x = (x);					\
        new_px = new_p + (&(x) - p);			\
        *new_px = new_x;				\
        if (new_x != NULL_WORD)				\
          PUSH_TO_COPY_STACK(new_px);			\
      } while (0)
#define DECLARE_NONNULL_PTR(x)				\
      do {						\
        assert((x) != FIRST_GENERATION_DEADBEEF);	\
        new_x = (x);					\
        new_px = new_p + (&(x) - p);			\
        *new_px = new_x;				\
        assert(new_x != NULL_WORD);			\
        PUSH_TO_COPY_STACK(new_px);			\
      } while (0)
#define DECLARE_TAGGED(x)				\
      do {						\
        new_x = (x);					\
        new_px = new_p + (&(x) - p);			\
        *new_px = new_x;				\
        if (TAGGED_IS_PTR(new_x) && new_x != NULL_WORD)	\
          PUSH_TO_COPY_STACK(new_px);			\
      } while (0)

#include "cells-def-prep.h"

#undef CELL
#undef DECLARE_TAGGED
#undef DECLARE_PTR
#undef DECLARE_NONNULL_PTR
#undef DECLARE_WORD

#ifndef NDEBUG
    default:
      abort();
#endif
    }
    /* Set up a forward pointer. */
    p[0] = CELL_forward_pointer << (32 - CELL_TYPE_BITS);
    *pp = p[1] = PTR_TO_WORD(new_p);
    assert(copy_stack_ptr < copy_stack + NUMBER_OF_WORDS_IN_FIRST_GENERATION);
  }
  to_ptr = reg_to_ptr;
}


/* Recovery of cells in to_gn.  These routines assume `page_info's and
   `generation_info's are all set, and all data pages are read into
   memory.  Additionally the actual cell copying routines assume the
   data has actually already been copied to the to generation.  But
   otherwise these routines mimic the ones defined above.  */

/* The index of the `to_pn' in the `generation_info[to_gn].page'-array. */
static int rvy_to_pn_index;

/* The pointer to the first word on the next page. */
static ptr_t rvy_next_to_ptr;

static void rvy_start_gc(void)
{
  to_ptr = to_end = NULL;
  to_pn = INVALID_PAGE_NUMBER;
  rvy_to_pn_index = 0;
  if (generation_info[to_gn].npages != 0)
    rvy_next_to_ptr =
      PAGE_NUMBER_TO_PAGE_PTR(generation_info[to_gn].page[0]) + 2;
}


/* This is analogous to `new_to_page'. */
static void rvy_next_to_page(void)
{
  if (to_pn != INVALID_PAGE_NUMBER) {
    assert(PAGE_GET_NUMBER_OF_WORDS_IN_USE(to_pn)
	   == (unsigned long) (to_ptr - PAGE_NUMBER_TO_PAGE_PTR(to_pn)));
    /* Pick the next to page in the to generation. */
    rvy_to_pn_index++;
  }
  to_pn = generation_info[to_gn].page[rvy_to_pn_index];
  to_ptr = rvy_next_to_ptr;
  to_end = 
    PAGE_NUMBER_TO_PAGE_PTR(to_pn) + PAGE_GET_NUMBER_OF_WORDS_IN_USE(to_pn);
  if (rvy_to_pn_index != generation_info[to_gn].npages - 1)
    rvy_next_to_ptr =
      PAGE_NUMBER_TO_PAGE_PTR(generation_info[to_gn].page[rvy_to_pn_index + 1])
      + 2;
  else
    rvy_next_to_ptr = NULL;
}


static void rvy_finish_gc(void)
{
  if (to_pn != INVALID_PAGE_NUMBER) {
    /* There's nothing particular to wrap up, but perform some
       assertions. */
    assert(PAGE_GET_NUMBER_OF_WORDS_IN_USE(to_pn)
	   == (unsigned long) (to_ptr - PAGE_NUMBER_TO_PAGE_PTR(to_pn)));
    assert(rvy_to_pn_index == generation_info[to_gn].npages - 1);
  }
}


static void rvy_copy_cell(ptr_t pp)
{
  word_t p0;
  ptr_t p, old_p;		/* `old_p' is used only for asserts. */
  generation_info_t *gni;

  assert(pp != NULL_PTR);
  assert(*pp != NULL_WORD);
  p = WORD_TO_PTR(*pp);

  p0 = p[0];
  switch (CELL_TYPE(p)) {
#define CELL(name, number_of_words, field_definition_block)	\
  case CELL_ ## name:						\
    if (CELL_ ## name == CELL_forward_pointer) {		\
      *pp = p[1];						\
      return;							\
    } else {							\
      if (to_ptr + (number_of_words) > to_end)			\
        rvy_next_to_page();					\
      old_p = p;						\
      p = to_ptr;						\
      to_ptr += (number_of_words);				\
      assert(p[0] == p0);					\
      field_definition_block;					\
    }								\
    break;
#define DECLARE_WORD(x)				\
    do {					\
      assert(old_p[&(x) - p] == (x)); 	\
    } while (0)
#define DECLARE_PTR(x)				\
    do {					\
      if ((x) != NULL_WORD)			\
	PUSH_TO_COPY_STACK(&(x));		\
    } while (0)
#define DECLARE_NONNULL_PTR(x)			\
    do {					\
      assert((x) != NULL_WORD);			\
      PUSH_TO_COPY_STACK(&(x));			\
    } while (0)
#define DECLARE_TAGGED(x)			\
    do {					\
      if (TAGGED_IS_PTR(x)) {			\
	if ((x) != NULL_WORD)			\
          PUSH_TO_COPY_STACK(&(x));		\
      } else {					\
        assert(old_p[&(x) - p] == (x));		\
      }						\
    } while (0)

#include "cells-def-prep.h"

#undef CELL
#undef DECLARE_TAGGED
#undef DECLARE_PTR
#undef DECLARE_NONNULL_PTR
#undef DECLARE_WORD

#ifndef NDEBUG
  default:
    abort();
#endif
  }
  if (PTR_TO_GENERATION_INFO(p) != PTR_TO_GENERATION_INFO(old_p)) {
    /* Set up a forward pointer.  Since `rvy_copy_cell' may be called
       to copy a cell on itself (in order to fill the copy stack as
       not during recovery), we have to check we're not writing a
       forward pointer on the cell itself. */
    old_p[0] = CELL_forward_pointer << (32 - CELL_TYPE_BITS);
    *pp = old_p[1] = PTR_TO_WORD(p);
  }
}


static void rvy_drain_copy_stack(void)
{
  word_t p0;
  ptr_t pp, p;
  generation_info_t *gni;

  while (!COPY_STACK_IS_EMPTY) {
    pp = POP_FROM_COPY_STACK;
    assert(*pp != NULL_WORD);
    p = WORD_TO_PTR(*pp);

    assert(!is_in_first_generation(p));
    gni = PTR_TO_GENERATION_INFO(p);
    if (gni->status == NORMAL)
      continue;
    if (gni->status == TO_BE_COLLECTED) {
      PREPEND_TO_REM_SET(gni->rem_set, pp, gni->rem_set_allocation_ptr);
      continue;
    }
    assert(gni->status == BEING_COLLECTED);

    p0 = p[0];
    switch (CELL_TYPE(p)) {
#define CELL(name, number_of_words, field_definition_block)		\
    case CELL_ ## name:							\
      if (CELL_ ## name == CELL_forward_pointer) {			\
        assert(0);							\
      } else {								\
	if (to_ptr + (number_of_words) <= to_end) {			\
	  /* The cell referred to by `p' can still fit on this */	\
          /* page. */							\
	  if (to_ptr != p)						\
	    /* This means the `p' was already copied, and we can use */	\
            /* the pointer value currently at `pp'. */			\
            continue;							\
	} else {							\
	  /* This page can't host the cell referred to by `p'. */	\
	  if (rvy_next_to_ptr != p)					\
	    continue;							\
          rvy_next_to_page();						\
	}								\
	p = to_ptr;							\
	to_ptr += (number_of_words);					\
        field_definition_block;						\
      }									\
      break;
#define DECLARE_WORD(x)				\
      /* Do nothing. */
#define DECLARE_PTR(x)				\
      do {					\
        if ((x) != NULL_WORD)			\
	  PUSH_TO_COPY_STACK(&(x));		\
      } while (0)
#define DECLARE_NONNULL_PTR(x)			\
      do {					\
        assert((x) != NULL_WORD);		\
	PUSH_TO_COPY_STACK(&(x));		\
      } while (0)
#define DECLARE_TAGGED(x)				\
      do {						\
        if (TAGGED_IS_PTR(x) && (x) != NULL_WORD)	\
	  PUSH_TO_COPY_STACK(&(x));			\
      } while (0)

#include "cells-def-prep.h"

#undef CELL
#undef DECLARE_TAGGED
#undef DECLARE_PTR
#undef DECLARE_NONNULL_PTR
#undef DECLARE_WORD

#ifndef NDEBUG
    default:
      abort();
#endif
    }
    assert(copy_stack_ptr < copy_stack + NUMBER_OF_WORDS_IN_FIRST_GENERATION);
  }
}


/* Mature generation garbage collection. */

static int major_gc_was_started = 0;

/* Mark the generations to be collected by the imminent major gc. */
static void mark_major_gc_generations(void)
{
  int number_of_generations_to_collect = 0;
  unsigned long number_of_marked_pages = 0;
  unsigned long number_of_pages_to_mark = 
    /* XXX A crude heuristic! */
    (sqrt(db_size) * sqrt(first_generation_size)) / PAGE_SIZE;
  generation_number_t gn;

  for (gn = youngest_gn;
       gn != INVALID_GENERATION_NUMBER;
       gn = generation_info[gn].older) {
    if (allow_additional_generationality
	&& number_of_marked_pages > number_of_pages_to_mark
	&& generation_info[gn].shrinkage <
	   avg_generation_shrinkage + generation_shrinkage_margin)
      break;
    assert(generation_info[gn].status == NORMAL);
    generation_info[gn].status = TO_BE_COLLECTED;
    number_of_generations_to_collect++;
    number_of_marked_pages += generation_info[gn].npages;
  }

  if (must_show_groups)
    fprintf(stderr, "\n{Initiating major collection of %d generations}",
	    number_of_generations_to_collect);

  major_gc_was_started = 1;
}


static void rvy_mark_major_gc_generations(generation_number_t start_gn)
{
  generation_number_t gn;

  for (gn = start_gn; 
       gn != INVALID_GENERATION_NUMBER; 
       gn = generation_info[gn].older) {
    assert(generation_info[gn].status == NORMAL);
    generation_info[gn].status = TO_BE_COLLECTED;
  }
}


/* The generation the have previously been copied to by
   *major_gc_step*. */
static generation_number_t prev_to_gn = INVALID_GENERATION_NUMBER;

/* Do a unit of mature generation garbage collection.  Return 0 if the
   major collection was finished, and otherwise an estimate of how
   much gc effort was used. */
static int major_gc_step(void)
{
  int i;
  int number_of_from_pages, number_of_from_gns;
  unsigned long number_of_referring_ptrs;
  generation_number_t first_from_gn, gn, tmp_gn;
  rem_set_t *rem_set;
  ptr_t p;
  smart_ptr_t *sp;

  if (must_show_groups)
    fprintf(stderr, "{");

  /* Choose the first generation to collect. */
  if (prev_to_gn == INVALID_GENERATION_NUMBER) {
    /* We have just started a new mature collection. */
    first_from_gn = youngest_gn;
    /* Skip generations that may have emerged since we marked the
       generations we intend to collect. */
    while (generation_info[first_from_gn].status != TO_BE_COLLECTED)
      first_from_gn = generation_info[first_from_gn].older;
  } else
    first_from_gn = generation_info[prev_to_gn].older;
  /* Start the collection. */
  allocate_generation();	/* Sets `to_gn'. */
  insert_generation_after(generation_info[first_from_gn].younger);
  start_gc();
  /* Choose the generations to collect in this step.  Also put them on
     the from-list of `to_gn'. */
  gn = first_from_gn;
  number_of_from_pages = 0;
  number_of_from_gns = 0;
  do {
    assert(generation_info[gn].status == TO_BE_COLLECTED);
    generation_info[gn].status = BEING_COLLECTED;
    generation_info[gn].next_from = generation_info[to_gn].from;
    generation_info[to_gn].from = gn;
    if (must_show_groups)
      if (number_of_from_gns == 0)
	fprintf(stderr, "%d", generation_info[gn].npages);
      else
	fprintf(stderr, "+%d", generation_info[gn].npages);
    number_of_from_pages += generation_info[gn].npages;
    number_of_from_gns++;
    gn = generation_info[gn].older;
  } while (gn != INVALID_GENERATION_NUMBER
	   && generation_info[gn].status == TO_BE_COLLECTED
	   && number_of_from_gns < 0xFFF
	   && (generation_info[gn].npages +
	       number_of_from_pages) * PAGE_SIZE
	      < first_generation_size * relative_mature_generation_size);
  /* Scan the remembered sets of the collected generations. */
  CLEAR_COPY_STACK;
  for (i = 0, gn = first_from_gn;
       i < number_of_from_gns;
       i++, gn = generation_info[gn].older) {
    rem_set = generation_info[gn].rem_set;
    if (rem_set != REM_SET_TAIL_COOKIE) {
      for (p = generation_info[gn].rem_set_allocation_ptr + 1;
	   p < &rem_set->referrer[REM_SET_SIZE];
	   p++) {
	/* Since `*p' is in `gn's remembered set, it the value in
	   `**p' necessarily has to be a pointer to a cell in `gn'. */
	assert(PTR_TO_GENERATION_INFO(WORD_TO_PTR(*WORD_TO_PTR(*p))) ==
	       &generation_info[gn]);
	copy_cell(WORD_TO_PTR(*p));
      }
      for (rem_set = rem_set->next;
	   rem_set != REM_SET_TAIL_COOKIE;
	   rem_set = rem_set->next)
	for (p = &rem_set->referrer[0];
	     p < &rem_set->referrer[REM_SET_SIZE];
	     p++) {
	  assert(PTR_TO_GENERATION_INFO(WORD_TO_PTR(*WORD_TO_PTR(*p))) ==
		 &generation_info[gn]);
	  copy_cell(WORD_TO_PTR(*p));
	}
      free_rem_set(generation_info[gn].rem_set,
		   generation_info[gn].rem_set_allocation_ptr);
      generation_info[gn].rem_set = REM_SET_TAIL_COOKIE;
      generation_info[gn].rem_set_allocation_ptr = NULL;
    }
  }
  /* Scan the root set, first accu then others. */
  if ((GET_ROOT_WORD(suspended_accu_type) == PTR
       || GET_ROOT_WORD(suspended_accu_type) == NONNULL_PTR
       || (GET_ROOT_WORD(suspended_accu_type) == TAGGED
	   && TAGGED_IS_PTR(GET_ROOT_WORD(suspended_accu))))
      && GET_ROOT_WORD(suspended_accu) != NULL_WORD
      && !is_in_first_generation(GET_ROOT_PTR(suspended_accu))
      && PTR_TO_GENERATION_INFO(GET_ROOT_PTR(suspended_accu))->status 
           == BEING_COLLECTED)
    copy_cell(&root[ROOT_IX_suspended_accu]);
  for (i = 0; i < NUMBER_OF_ROOT_IXS; i++)
    if (root_ix_is_ptr(i)
	&& root[i] != NULL_WORD
	&& !is_in_first_generation(WORD_TO_PTR(root[i]))
	&& PTR_TO_GENERATION_INFO(WORD_TO_PTR(root[i]))->status
	     == BEING_COLLECTED)
      copy_cell(&root[i]);
  /* Scan the smart pointers. */
  for (sp = first_smart_ptr.next; sp != &first_smart_ptr; sp = sp->next) {
    assert(sp->next->prev == sp);
    if (sp->ptr != NULL_WORD
	&& !is_in_first_generation(WORD_TO_PTR(sp->ptr))
	&& PTR_TO_GENERATION_INFO(WORD_TO_PTR(sp->ptr))->status
	     == BEING_COLLECTED)
      copy_cell(&sp->ptr);
  }
  /* Memorize the number of immediately copied cells.  It will later
     be logged in the generation description. */
  number_of_referring_ptrs = COPY_STACK_DEPTH;
  /* Finally drain the copy stack. */
  drain_copy_stack();
  /* Finishing liturgy of garbage collections. */
  finish_gc();
  log_to_generation_pinfo_list(number_of_from_gns, number_of_referring_ptrs);
  /* Maintain statistics. */
  {
    double ngens = db_size / (double) first_generation_size;
    avg_generation_shrinkage =
      ((number_of_from_pages / (generation_info[to_gn].npages + 0.5))
       + avg_generation_shrinkage * ngens) / (ngens + 1);
  }
  /* Free the old generations and their pages.  We couldn't do this
     earlier because it could have resulted in a non-recoverable page
     reuse pattern. */
  for (i = 0, gn = first_from_gn; i < number_of_from_gns; i++, gn = tmp_gn) {
    tmp_gn = generation_info[gn].older;
    mark_generation_collected_once(gn);
  }
  /* Statistics. */
  if (must_show_groups) {
    fprintf(stderr, "»%d", generation_info[to_gn].npages);
#if 0
#ifdef GC_PROFILING
    fprintf(stderr, ",\n  rem_set size is %d of top %d", 
	    current_rem_set_size, max_rem_set_size);
#endif
#endif
    fprintf(stderr, "}");
  }
  /* Finally peek a little ahead to see whether we finished the major
     collection. */
  gn = generation_info[to_gn].older;
  if (gn == INVALID_GENERATION_NUMBER
      || generation_info[gn].status != TO_BE_COLLECTED) {
    /* Major collection is finished. */
    prev_to_gn = INVALID_GENERATION_NUMBER;
    if (must_show_groups)
      fprintf(stderr, "{major gc done}");
    return 0;
  } else {
    /* Major collection will continue, memorize where should continue
       from and return an guesstimate of how much work we did. */
    prev_to_gn = to_gn;
    return generation_info[to_gn].npages + 1;
  }
}


static int rvy_major_gc_step(int number_of_from_generations,
			     unsigned long number_of_referring_ptrs)
{
  int i;
  ptr_t p;
  word_t w;
  rem_set_t *rem_set;
  generation_number_t first_from_gn, gn, tmp_gn;

  /* Choose first generation to collect. */
  if (prev_to_gn == INVALID_GENERATION_NUMBER) {
    first_from_gn = youngest_gn;
    while (generation_info[first_from_gn].status != TO_BE_COLLECTED)
      first_from_gn = generation_info[first_from_gn].older;
  } else
    first_from_gn = generation_info[prev_to_gn].older;
  /* Start rvy-collection.  `to_gn' has already been set. */
  rvy_start_gc();
  insert_generation_after(generation_info[first_from_gn].younger);
  /* Mark the from-generations to be collected. */
  gn = first_from_gn;
  for (i = 0; i < number_of_from_generations; i++) {
    assert(generation_info[gn].status == TO_BE_COLLECTED);
    generation_info[gn].status = BEING_COLLECTED;
    generation_info[gn].next_from = generation_info[to_gn].from;
    generation_info[to_gn].from = gn;
    gn = generation_info[gn].older;
  }
  /* `to_gn' actually behaves also as if it were being collected. */
  generation_info[to_gn].status = BEING_COLLECTED;
  /* Scan the remembered sets of the collected generations. */
  CLEAR_COPY_STACK;
  for (i = 0, gn = first_from_gn;
       i < number_of_from_generations;
       i++, gn = generation_info[gn].older) {
    rem_set = generation_info[gn].rem_set;
    if (rem_set != REM_SET_TAIL_COOKIE) {
      for (p = generation_info[gn].rem_set_allocation_ptr + 1;
	   p < &rem_set->referrer[REM_SET_SIZE];
	   p++) {
	assert(PTR_TO_GENERATION_INFO(WORD_TO_PTR(*WORD_TO_PTR(*p))) ==
	       &generation_info[gn]);
	rvy_copy_cell(WORD_TO_PTR(*p));
      }
      for (rem_set = rem_set->next;
	   rem_set != REM_SET_TAIL_COOKIE;
	   rem_set = rem_set->next)
	for (p = &rem_set->referrer[0];
	     p < &rem_set->referrer[REM_SET_SIZE];
	     p++) {
	  assert(PTR_TO_GENERATION_INFO(WORD_TO_PTR(*WORD_TO_PTR(*p))) ==
		 &generation_info[gn]);
	  rvy_copy_cell(WORD_TO_PTR(*p));
	}
      free_rem_set(generation_info[gn].rem_set,
		   generation_info[gn].rem_set_allocation_ptr);
      generation_info[gn].rem_set = REM_SET_TAIL_COOKIE;
      generation_info[gn].rem_set_allocation_ptr = NULL;
    }
  }
  assert(COPY_STACK_DEPTH <= (signed) number_of_referring_ptrs);
  /* During recovery, this is equivalent to scanning the root block
     and the smart pointers. */
  while (COPY_STACK_DEPTH < (signed) number_of_referring_ptrs) {
    assert(to_ptr <= to_end);
    if (to_ptr == to_end)
      rvy_next_to_page();
    w = PTR_TO_WORD(to_ptr);
    rvy_copy_cell(&w);
  }
  /* This has the effect of reconstructing all older remembered
     sets. */
  rvy_drain_copy_stack();
  /* Finish the gc. */
  rvy_finish_gc();
  generation_info[to_gn].status = NORMAL;
  /* Free old generations.  Identical to `major_gc_step'. */
  for (i = 0, gn = first_from_gn;
       i < number_of_from_generations; 
       i++, gn = tmp_gn) {
    tmp_gn = generation_info[gn].older;
    mark_generation_collected_once(gn);
  }
  /* As in `major_gc_step', peek a little forward and return non-zero
     if there's still major collection to do. */
  gn = generation_info[to_gn].older;
  if (gn == INVALID_GENERATION_NUMBER
      || generation_info[gn].status != TO_BE_COLLECTED) {
    prev_to_gn = INVALID_GENERATION_NUMBER;
    return 0;
  } else {
    prev_to_gn = to_gn;
    return 1;
  }
}


/* Is the major collection currently active? */
static int is_collecting = 0;

/* A driver-loop for the major collector. */
static void major_gc(int is_idle)
{
  unsigned long free_mem;
  int effort = 0, effort_step, already_major_gc_stepped = 0;

  if (!is_idle 
      && !is_collecting
      && number_of_free_pages * PAGE_SIZE > (unsigned) start_gc_limit)
    return;

  do {
    if (!is_collecting) {
      mark_major_gc_generations();
      is_collecting = 1;
      /* We just started the major collection.  Wait a little before
	 actually starting the work.  (The newest generation was just
	 collected, collecting it immediately again would be a wasted
	 effort.) */
      return;
    }
    /* If doing a second `major_gc_step' during the same commit batch,
       we must somehow stabilize or copy for queueing those pages that
       we wrote in the previous `major_gc_step' because we are going
       to redirect some pointers (mutate) in those pages. */
    if (already_major_gc_stepped)
      io_allow_page_changes();
    already_major_gc_stepped = 1;
    /* Now perform the `major_gc_step'. */
    effort_step = major_gc_step();
    if (effort_step == 0)
      /* Finished major gc. */
      is_collecting = 0;
    /* The six lines below implement a heuristic for scheduling major
       gc work.  These may be subject to frequent changes and tweaking. */
    effort += PAGE_SIZE * effort_step;
    free_mem = number_of_free_pages * PAGE_SIZE;
  } while (effort < max_gc_effort
	   && free_mem < (unsigned long) start_gc_limit
	   && (effort * (double) (start_gc_limit - max_gc_limit) 
	         < max_gc_effort * (double) (start_gc_limit - free_mem)));

  if (effort >= max_gc_effort)
    fprintf(stderr, "\n{Maximum GC speed reached.  Memory about to exhaust!}");
}


/* Collecting and committing the first generation; recovering new
   generations. */

/* Collect the first generation and put the surviving cells in a new
   mature generation, which is naturally assiged to `youngest_gn'.
   Returns the number of pointers in the root block that refer to the
   new generation. */
static unsigned long collect_first_generation(void)
{
  int i;
  unsigned long number_of_referring_ptrs;
  smart_ptr_t *sp;

  start_gc();
  allocate_generation();
  insert_generation_after(INVALID_GENERATION_NUMBER);
  /* Copy everything reachable from the root set.  Naturally, there's
     no remembered set for the first generation. */
  CLEAR_COPY_STACK;
  if ((GET_ROOT_WORD(suspended_accu_type) == PTR
       || GET_ROOT_WORD(suspended_accu_type) == NONNULL_PTR
       || (GET_ROOT_WORD(suspended_accu_type) == TAGGED
	   && TAGGED_IS_PTR(GET_ROOT_WORD(suspended_accu))))
      && GET_ROOT_WORD(suspended_accu) != NULL_WORD
      && is_in_first_generation(GET_ROOT_PTR(suspended_accu)))
    copy_cell(&root[ROOT_IX_suspended_accu]);
  for (i = 0; i < NUMBER_OF_ROOT_IXS; i++)
    if (root_ix_is_ptr(i)
	&& root[i] != NULL_WORD
	&& is_in_first_generation(WORD_TO_PTR(root[i])))
      copy_cell(&root[i]);
  /* Scan the smart ptr ring. */
  for (sp = first_smart_ptr.next; sp != &first_smart_ptr; sp = sp->next) {
    assert(sp->next->prev == sp);
    if (sp->ptr != NULL_WORD
	&& is_in_first_generation(WORD_TO_PTR(sp->ptr)))
      copy_cell(&sp->ptr);
  }
  number_of_referring_ptrs = COPY_STACK_DEPTH;
  drain_copy_stack();
  finish_gc();
  return number_of_referring_ptrs;
}


/* This is called for new mature generations (collected from first
   generations instead of other mature generations) during recovery.  Its
   main purpose is to construct the remembered sets of older mature
   generations. */
static void rvy_new_generation(unsigned long number_of_referring_ptrs)
{
  word_t w;

  rvy_start_gc();
  /* Distinct from above, `to_gn' has already been allocated. */
  generation_info[to_gn].status = BEING_COLLECTED;
  CLEAR_COPY_STACK;
  while (COPY_STACK_DEPTH < (signed) number_of_referring_ptrs) {
    assert(to_ptr <= to_end);
    if (to_ptr == to_end)
      rvy_next_to_page();
    w = PTR_TO_WORD(to_ptr);
    rvy_copy_cell(&w);
  }
  rvy_drain_copy_stack();
  rvy_finish_gc();
  generation_info[to_gn].status = NORMAL;
}


/* Group commit.  In addition to collecting and clearing the first
   generation this contains creating some metadata and performing some
   mature garbage collection. */
void flush_batch(void)
{
  int i;
  ptr_t p;
  disk_page_number_t last_dpn;
  unsigned long number_of_referring_ptrs;
#ifdef GC_PROFILING
  /* Initialize to 1 instead of 0 to prevent division by zero. */
  static unsigned long data_kbytes = 1;
  static unsigned long total_kbytes = 1;

  number_of_written_bytes = 0;
#endif
  if (first_generation_allocation_ptr == first_generation_start)
    /* Nothing has been allocated; no commit processing needed. */
    return;
  /* Clear the oid freelist, see `oid.c'. */
  SET_ROOT_WORD(oid_freelist, NULL_WORD);
  number_of_referring_ptrs = collect_first_generation();
  /* Wrap up some metadata. */
  cache_generation_pinfo_to_root(number_of_referring_ptrs);
  /* Finish the commit group in writing the root block. */
  io_write_root();
  /* Start a new commit group by clearing the root block and copying
     the metadata that was cached in the root block into the actual
     database image. */
  clear_first_generation();
  if (major_gc_was_started) {
    major_gc_was_started = 0;
    /* Now that the commit group that finalized the previous major gc
       round has been successfully finished we can free some old disk
       pages. */
    mark_twice_collected_generations_nonexistent();
    /* Start a new pinfo list. */
    SET_ROOT_PTR(prev_prev_generation_pinfo_list,
		 GET_ROOT_PTR(prev_generation_pinfo_list));
    SET_ROOT_PTR(prev_generation_pinfo_list, 
		 GET_ROOT_PTR(generation_pinfo_list));
    SET_ROOT_PTR(generation_pinfo_list, NULL_PTR);
  }
  log_to_generation_pinfo_list(0, number_of_referring_ptrs);
#ifdef GC_PROFILING
  /* Statistics. */
  data_kbytes += number_of_written_bytes / 1024;
  if (must_show_groups)
    fprintf(stderr, 
	    "[%d free pages, %.3f gc-written bytes, %d rem_set bytes] ", 
	    number_of_free_pages,
	    (total_kbytes - data_kbytes) / (double) total_kbytes,
	    current_rem_set_size);
#else
  if (must_show_groups)
    fprintf(stderr, "[%lu free pages] ", number_of_free_pages);
#endif
  /* Do some mature garbage collection. */
  major_gc(0);
#ifdef GC_PROFILING
  /* More statistics. */
  total_kbytes += number_of_written_bytes / 1024;
#endif
  if (must_show_groups)
    fprintf(stderr, "\n");
}


/* Recovery and initialization routines. */

int init_shades(int argc, char **argv)
{
  int i;
  page_number_t pn;

  /* Read the arguments that control this exclusively `shades.c';
     params.c takes care of the rest. */
  for (i = 1; i < argc; i++) {
    if (argv[i] == NULL)
      continue;
    if (strcmp("--show-groups", argv[i]) == 0) {
      must_show_groups = 1;
      argv[i] = NULL;
    }
  }
  init_params(argc, argv);
  if (be_verbose)
    must_show_groups = 1;

  /* Do some sanity checks on the params we just read. */
  if ((sizeof(page_info_t) & (sizeof(page_info_t) - 1)) != 0)
    fprintf(stderr, 
	    "Warning: size of `page_info_t', %d, is not a power of two.\n", 
	    sizeof(page_info_t));
  if ((PAGE_SIZE & (PAGE_SIZE - 1)) != 0) {
    fprintf(stderr, "Fatal: Page size must be a power of 2.  It was %d.\n",
	    PAGE_SIZE);
    exit(2);
  }
  if (PAGE_SIZE > first_generation_size / 6) {
    fprintf(stderr,
	    "Warning: Page size (%d kB) is unproportionally large\n"
	    "  compared to the first generation size (%d kB).\n"
	    "  This will waste memory.\n",
	    PAGE_SIZE / 1024, first_generation_size / 1024);
  }
  if ((unsigned long) (first_generation_size + PAGE_SIZE - 1) / PAGE_SIZE 
      > MAX_GENERATION_SIZE) {
    fprintf(stderr, 
	    "Fatal: Page size (%d kB) so small and first generation so large\n"
	    "  (%d kB) that the root block might not be able to store the\n"
	    "  generation-wise persistent info of a maximal generation.\n",
	    PAGE_SIZE / 1024, first_generation_size / 1024);
    exit(2);
  }

  if (relative_mature_generation_size > 1.0) {
    /* This could lead to overflowing `copy_stack'. */
    fprintf(stderr, 
	    "Fatal: Parameter relative_mature_generation_size has value %f.\n"
	    "  Its value should be <= 1.0\n",
	    relative_mature_generation_size);
    exit(2);
  }
  
  if (disk_skip_nbytes % DISK_BLOCK_SIZE != 0) {
    fprintf(stderr,
	    "Fatal: disk_skip_nbytes (%d) must be a multiple of\n"
	    "  disk block size (%d)\n",
	    disk_skip_nbytes, DISK_BLOCK_SIZE);
    exit(2);
  }
  if (PAGE_SIZE <= ROOT_BLOCK_SIZE) {
    fprintf(stderr,
	    "Fatal: root block size (%d B) must be less or equal to\n"
	    "  page size (%d B).\n",
	    ROOT_BLOCK_SIZE, PAGE_SIZE);
    exit(2);
  }

  number_of_pages = db_size / PAGE_SIZE;

  init_root();
  init_cells();
  io_init();

  /* Allocate page-aligned memory for the main memory image of the
     database. */
#ifdef HAVE_MEMALIGN
  mem_base = (ptr_as_scalar_t)
    memalign(PAGE_SIZE, db_size + first_generation_size);
  assert(mem_base % PAGE_SIZE == 0);
#else
  mem_base = (ptr_as_scalar_t)
    malloc(PAGE_SIZE * (number_of_pages + 1) + first_generation_size);
  if (mem_base % PAGE_SIZE != 0)
    mem_base += PAGE_SIZE - (mem_base % PAGE_SIZE);
#endif
  if (mem_base == 0) {
    fprintf(stderr, "shades_init: Failed to malloc the database.\n");
    exit(1);
  }

  /* Initialize the magic cookie of each page. */
  for (pn = 0; pn < number_of_pages; pn++)
    PAGE_NUMBER_TO_PAGE_PTR(pn)[0] = PAGE_MAGIC_COOKIE;

  /* Cut out the first generation's part of the memory area, and
     initialize it. */
  first_generation_end = (ptr_t) (mem_base + db_size);
  first_generation_start = 
    first_generation_end + NUMBER_OF_WORDS_IN_FIRST_GENERATION;
  clear_first_generation();

  /* Allocate various structures used by other routines. */
  assert(copy_stack == NULL);
  copy_stack = malloc(NUMBER_OF_WORDS_IN_FIRST_GENERATION * sizeof(ptr_t));
  if (copy_stack == NULL) {
    fprintf(stderr, "shades_init: Failed to malloc `copy_stack'.\n");
    exit(1);
  }

  assert(page_info == NULL);
  page_info = malloc(number_of_pages * sizeof(page_info_t));
  if (page_info == NULL) {
    fprintf(stderr, "shades_init: Failed to malloc `page_info'.\n");
    exit(1);
  }

  assert(generation_info == NULL);
  /* Maximum number of generations equals to the maximum number of
     pages. */
  generation_info_grow(32 + number_of_pages / 4);

  return 0;
}


/* Create a new empty database. */

void create_db(void)
{
  page_number_t pn;

  /* `shades_init' should already have been called. */
  assert(mem_base != 0);

  io_create_file();

  for (pn = 0; pn < number_of_pages; pn++) {
    page_info[pn].is_allocated = 1;
    free_page(pn);
  }

  flush_batch();		/* XXX Is this needed? */
}


/* This array keeps in memory which disk page is in the given main
   memory page. */
static disk_page_number_t *rvy_disk_page;


/* Based on the existsing meta-data, read the data pages of the
   specified generation into memory.  `read_generation' might be
   called several times for the same generation, subsequent calls read
   only those pages which really do need to be reread. */
static void rvy_read_generation(generation_number_t gn)
{
  int i;
  page_number_t pn;
  disk_page_number_t dpn;
  
  for (i = 0; i < generation_info[gn].npages; i++) {
    pn = generation_info[gn].page[i];
    dpn = generation_info[gn].disk_page[i];
    if (rvy_disk_page[pn] != dpn) {
      rvy_allocate_page(pn);
      page_info[pn].generation = &generation_info[gn];
      io_declare_disk_page_allocated(dpn);
#ifdef ASYNC_IO
      io_read_page_start(PAGE_NUMBER_TO_PAGE_PTR(pn), PAGE_SIZE, dpn);
#else
      io_read_page(PAGE_NUMBER_TO_PAGE_PTR(pn), PAGE_SIZE, dpn);
#endif
      rvy_disk_page[pn] = dpn;
    }
  }
#ifdef ASYNC_IO
  io_read_page_wait();
#endif
}


/* Read in the generations listed in `generations' and tuck them as
   older generations of `younger_gn'.  This routune performs no
   pointer reconstruction - that has to be done by subsequent recovery
   operations. */
static void rvy_base_major_gc_round(generation_number_t younger_gn,
				    ptr_t generation_pinfo_list)
{
  ptr_t p;
  int i;
  generation_number_t gn, oldest_gn;
  int npages, number_of_from_generations;

  oldest_gn = INVALID_GENERATION_NUMBER;
  for (p = generation_pinfo_list; p != NULL_PTR; p = WORD_TO_PTR(p[1])) {
    assert(page_info[PTR_TO_PAGE_NUMBER(p)].is_allocated);
    to_gn = gn = p[2];
    npages = p[0] & 0xFFF;
    number_of_from_generations = (p[0] >> 12) & 0xFFF;
    rvy_allocate_generation(npages);
    if (number_of_from_generations == 0) {
      /* A new generation. */
      insert_generation_after(younger_gn);
      younger_gn = gn;
    } else {
      /* A collected generation. */
      if (oldest_gn == INVALID_GENERATION_NUMBER) {
	/* The youngest collected generation. */
	oldest_gn = younger_gn;
	/* We can encounter collected generations only after at least
           one new generation. */
	assert(younger_gn != INVALID_GENERATION_NUMBER);
	insert_generation_after(oldest_gn);
	oldest_gn = gn;
      } else
	insert_generation_after(younger_gn);
    }
    for (i = 0; i < npages; i++) {
      generation_info[gn].page[i] = p[4 + 2*i];
      generation_info[gn].disk_page[i] = p[4 + 2*i + 1];
    }
    rvy_read_generation(gn);
  }
}


/* Recover one commit batchful of the database.  This is a recursive
   routine.  It reads new generations before the recursive step and
   constructs all the meta data from the `generation_pinfo's in the
   new generations so that the disk locations of the older generations
   will be known.  After the recursive step the both new and collected
   generations are read and pseudo-collected as they were before
   crash. */
static void rvy_batch(generation_number_t younger_gn,
		      ptr_t generation_pinfo_list,
		      ptr_t prev_generation_pinfo_list,
		      ptr_t prev_prev_generation_pinfo_list)
{
  int i;
  ptr_t p, next_p;
  generation_number_t gn;
  int npages, number_of_from_generations;
  unsigned long number_of_referring_ptrs;

  if (prev_generation_pinfo_list == NULL_PTR) {
    assert(prev_prev_generation_pinfo_list == NULL_PTR);
    rvy_base_major_gc_round(younger_gn, generation_pinfo_list);
    return;
  }
  assert(generation_pinfo_list != NULL_PTR);

  p = generation_pinfo_list;
  assert(CELL_TYPE(p) == CELL_generation_pinfo);
  npages = p[0] & 0xFFF;
  number_of_from_generations = (p[0] >> 12) & 0xFFF;
  next_p = WORD_TO_PTR(p[1]);
  to_gn = gn = p[2];
  number_of_referring_ptrs = p[3];

  rvy_allocate_generation(npages);
  if (number_of_from_generations == 0)
    insert_generation_after(younger_gn);
    /* Otherwise the generation takes its place in the generation list
       later in `rvy_major_gc_step/rvy_start_gc'. */
  for (i = 0; i < npages; i++) {
    generation_info[gn].page[i] = p[4 + 2*i];
    generation_info[gn].disk_page[i] = p[4 + 2*i + 1];
  }
  if (number_of_from_generations == 0) {
    rvy_read_generation(gn);
    if (next_p == NULL_PTR) {
      rvy_batch(gn,
		prev_generation_pinfo_list,
		prev_prev_generation_pinfo_list,
		NULL_PTR);
      to_gn = gn;
      rvy_mark_major_gc_generations(generation_info[gn].older);
      is_collecting = 1;
      mark_twice_collected_generations_nonexistent();
    } else {
      rvy_batch(gn,
		next_p, 
		prev_generation_pinfo_list, 
		prev_prev_generation_pinfo_list);
      to_gn = gn;
    }
    youngest_gn = gn;
    /* The recursive call may have replaced some main memory pages
       with other disk pages.  Reread if needed. */
    rvy_read_generation(gn);
    if (is_collecting)
      rvy_new_generation(number_of_referring_ptrs);
  } else {
    assert(next_p != NULL_PTR);
    rvy_batch(younger_gn,
	      next_p,
	      prev_generation_pinfo_list,
	      prev_prev_generation_pinfo_list);
    to_gn = gn;
    rvy_read_generation(gn);
    if (!rvy_major_gc_step(number_of_from_generations,
			   number_of_referring_ptrs))
      is_collecting = 0;
  }
}


/* Recover an existing database. */
void recover_db(void)
{
  generation_number_t gn, prev_gn;
  page_number_t pn, *ppn;
  disk_page_number_t dpn;
  unsigned long i, npages, number_of_referring_ptrs;
  ptr_t p;

  /* `shades_init' should already have been called. */
  assert(mem_base != 0);

  is_recovering = 1;
  rvy_disk_page = malloc(number_of_pages * sizeof(disk_page_number_t));
  if (rvy_disk_page == NULL) {
    fprintf(stderr, "Fatal: malloc failed for `rvy_disk_page'.\n");
    exit(2);
  }
  for (pn = 0; pn < number_of_pages; pn++) {
    rvy_disk_page[pn] = INVALID_DISK_PAGE_NUMBER;
    page_info[pn].is_allocated = 1;
    free_page(pn);
  }

  io_open_file();
  io_read_root();

  /* Read in the very youngest generation using the information in the
     root block. */
  youngest_gn = INVALID_GENERATION_NUMBER;
  to_gn = gn = GET_ROOT_WORD(youngest_generation_number);
  npages = GET_ROOT_WORD(youngest_generation_number_of_pages);
  number_of_referring_ptrs = 
    GET_ROOT_WORD(youngest_generation_number_of_referring_ptrs);
  rvy_allocate_generation(npages);
  insert_generation_after(INVALID_GENERATION_NUMBER);
  assert(youngest_gn == gn);
  for (i = 0; i < npages; i++) {
    generation_info[gn].page[i] =
      GET_ROOT_WORD_VECTOR(youngest_generation_page_number0, 2*i);
    generation_info[gn].disk_page[i] =
      GET_ROOT_WORD_VECTOR(youngest_generation_disk_page_number0, 2*i);
  }
  rvy_read_generation(gn);
  rvy_batch(youngest_gn,
	    GET_ROOT_PTR(generation_pinfo_list),
	    GET_ROOT_PTR(prev_generation_pinfo_list),
	    GET_ROOT_PTR(prev_prev_generation_pinfo_list));
  to_gn = youngest_gn = gn;
  rvy_read_generation(gn);
  if (is_collecting)
    rvy_new_generation(number_of_referring_ptrs);

  assert(check_generations());

  construct_page_freelist();
  /* Mark free all those disk pages not known to be in use. */
  io_declare_unallocated_pages_free();
  free(rvy_disk_page);
  is_recovering = 0;

  /* Now do what we would have done at the end of a normal commit
     group.  See `flush_batch' for comparison. */
  clear_first_generation();
  /* XXX A mere `mark_major_gc_generations' is not recovered
     identically.  Perhaps we should keep `major_gc_was_started' in
     the root block and conditionally do 

	mark_twice_collected_generations_nonexistent();
	SET_ROOT_PTR(prev_prev_generation_pinfo_list,
	             GET_ROOT_PTR(prev_generation_pinfo_list));
	SET_ROOT_PTR(prev_generation_pinfo_list,
                     GET_ROOT_PTR(generation_pinfo_list));
        SET_ROOT_PTR(generation_pinfo_list, NULL_PTR);

     here?  As it is, we might postpone major gc critically. */
  log_to_generation_pinfo_list(0, number_of_referring_ptrs);
  major_gc(0);
}


/* Tools for debugging.  Included partly because calling preprocessor
   macros from a debugger is somewhat cumbersome. */

#ifndef NDEBUG

static ptr_t word_to_ptr(word_t x)
{
  return WORD_TO_PTR(x);
}

static word_t ptr_to_word(ptr_t p)
{
  return PTR_TO_WORD(p);
}

static ptr_t page_number_to_page_ptr(page_number_t pn)
{
  return PAGE_NUMBER_TO_PAGE_PTR(pn);
}

static page_number_t ptr_to_page_number(ptr_t p)
{
  return PTR_TO_PAGE_NUMBER(p);
}

static cell_type_t cell_type(ptr_t p)
{
  return CELL_TYPE(p);
}

#ifdef ENABLE_RED_ZONES

static void first_generation_print(unsigned int heaviness)
{
  first_generation_fprint(stderr, heaviness);
}

#endif

#endif
