/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Common definitions and machine-dependent configurations.  This file
 * should be included first by all other code files in Shades.
 */

#ifndef INCL_INCLUDES
#define INCL_INCLUDES 1


/* AIX requires this to be the first thing in the file.  */
#ifndef alloca
#ifdef __GNUC__
# define alloca __builtin_alloca
#else
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif
#endif

/* Note: autoconf documentation tells to use the <...> syntax and have -I. */
#include <config.h>

#ifdef	STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#else
#include <strings.h>
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr(), *strrchr();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy((s), (d), (n))
#  define memmove(d, s, n) bcopy((s), (d), (n))
#  define memcmp(a, b, n) bcmp((a), (b), (n))
# endif
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>


/* assert() should be used for tests that affect the total running
   time of the programs with at most a constant factor.
   heavy_assert() should be used for other tests. */
#include <assert.h>
#ifdef ENABLE_HEAVY_ASSERT
#define heavy_assert(cond) assert(cond)
#else
#define heavy_assert(cond) ((void) 0)
#endif


#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#include <math.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "params.h"

/* Define `word' to be a 32-bit unsigned integer type. */
#if SIZEOF_LONG == 4
typedef unsigned long word_t;
typedef signed long signed_word_t;
#else
#if SIZEOF_INT == 4
typedef unsigned int word_t;
typedef signed int signed_word_t;
#else
#if SIZEOF_SHORT == 4
typedef unsigned short word_t;
typedef signed short signed_word_t;
#else
#error No 32-bit unsigned integer type exists.
#endif
#endif
#endif

typedef word_t *ptr_t;

/* Define `ptr_as_scalar' be the an unsigned integer type of equal size
   to a pointer type. */
#if SIZEOF_LONG == SIZEOF_INT_P
typedef unsigned long ptr_as_scalar_t;
#else
#if SIZEOF_INT == SIZEOF_INT_P
typedef unsigned int ptr_as_scalar_t;
#else
#if SIZEOF_SHORT == SIZEOF_INT_P
typedef unsigned short ptr_as_scalar_t;
#else
#error No integer type is of same size than the pointer type.
#endif
#endif
#endif


#endif /* INCL_INCLUDES */
