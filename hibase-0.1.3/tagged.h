/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1998 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Common routines for tagged data types.
 */

#ifndef INCL_TAGGED
#define INCL_TAGGED  1


void tagged_fprint(FILE *fp, word_t word);


#define TAGGED_TO_WORD(x)  (((word_t) (x)) >> 2)
#define WORD_TO_TAGGED(x)  ((((word_t) (x)) << 2) | 0x1)


#define TAGGED_TRUE	WORD_TO_TAGGED(1)
#define TAGGED_FALSE	WORD_TO_TAGGED(0)


/* Convert a tagged word to a signed word, and vice versa. */

#ifndef TAGGED_TO_SIGNED_WORD
#if -1 == (((-1) << 1) + 1) >> 1 /* XXX Doesn't work when cross-compiling? */
/* We have arithmetic shifts. */
#define TAGGED_TO_SIGNED_WORD(x)  (((signed_word_t) (x)) >> 2)
#define SIGNED_WORD_TO_TAGGED(x)  ((((signed_word_t) (x)) << 2) | 0x1)
#else
/* We have logical shifts. */
#define TAGGED_TO_SIGNED_WORD(x)		\
  ((((signed_word_t) (x)) >= 0)			\
   ? (((signed_word_t) (x)) >> 2)		\
   : ~((~((signed_word_t) (x))) >> 2))
#define SIGNED_WORD_TO_TAGGED(x)		\
  ((((signed_word_t) (x)) >= 0)			\
   ? ((((signed_word_t) (x)) << 2) | 0x1)	\
   : ~(((~((signed_word_t) (x))) << 2) | 0x2))
#endif
#endif


/* Returns non-zero if the two tagged words EQUAL (accordingly some
   currently unspecified semantic). */
int tagged_is_equal(ptr_t p, ptr_t q);

#define TAGGED_IS_EQUAL(a, b)					\
  ((a) == (b) 							\
   || (TAGGED_IS_PTR(a) 					\
       && TAGGED_IS_PTR(b) 					\
       && tagged_is_equal(WORD_TO_PTR(a), WORD_TO_PTR(b))))



#endif /* INCL_TAGGED */
