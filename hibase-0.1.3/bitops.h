/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1991 Kenneth Oksanen
 * Permission to freely use, modify, and distribute this file is permitted.
 * (Most of this code has existed before the Shades-project, but has been
 * modified to suit Shades' needs.)
 *
 * Authors: Kenneth <cessu@iki.fi>
 */

/* Various bitwise operations on `word_t's.
 */

#ifndef INCL_BITOPS_H
#define INCL_BITOPS_H 1


/* This macro tells how much its argument must at least be shifted
   right in order to become 0.  Use it only for constant expressions,
   otherwise it might generate lots of suboptimal code. */
#define DO_ILOG2(x)							      \
  ((unsigned long) (x) < 0x1 ? 0 :					      \
  ((unsigned long) (x) < 0x2 ? 1 :					      \
  ((unsigned long) (x) < 0x4 ? 2 :					      \
  ((unsigned long) (x) < 0x8 ? 3 :					      \
  ((unsigned long) (x) < 0x10 ? 4 :					      \
  ((unsigned long) (x) < 0x20 ? 5 :					      \
  ((unsigned long) (x) < 0x40 ? 6 :					      \
  ((unsigned long) (x) < 0x80 ? 7 :					      \
  ((unsigned long) (x) < 0x100 ? 8 :					      \
  ((unsigned long) (x) < 0x200 ? 9 :					      \
  ((unsigned long) (x) < 0x400 ? 10 :					      \
  ((unsigned long) (x) < 0x800 ? 11 :					      \
  ((unsigned long) (x) < 0x1000 ? 12 :					      \
  ((unsigned long) (x) < 0x2000 ? 13 :					      \
  ((unsigned long) (x) < 0x4000 ? 14 :					      \
  ((unsigned long) (x) < 0x8000 ? 15 :					      \
  ((unsigned long) (x) < 0x10000 ? 16 :					      \
  ((unsigned long) (x) < 0x20000 ? 17 :					      \
  ((unsigned long) (x) < 0x40000 ? 18 :					      \
  ((unsigned long) (x) < 0x80000 ? 19 :					      \
  ((unsigned long) (x) < 0x100000 ? 20 :				      \
  ((unsigned long) (x) < 0x200000 ? 21 :				      \
  ((unsigned long) (x) < 0x400000 ? 22 :				      \
  ((unsigned long) (x) < 0x800000 ? 23 :				      \
  ((unsigned long) (x) < 0x1000000 ? 24 :				      \
  ((unsigned long) (x) < 0x2000000 ? 25 :				      \
  ((unsigned long) (x) < 0x4000000 ? 26 :				      \
  ((unsigned long) (x) < 0x8000000 ? 27 :				      \
  ((unsigned long) (x) < 0x10000000 ? 28 :				      \
  ((unsigned long) (x) < 0x20000000 ? 29 :				      \
  ((unsigned long) (x) < 0x40000000 ? 30 :				      \
  ((unsigned long) (x) < 0x80000000 ? 31 : 32))))))))))))))))))))))))))))))))

#ifdef __GNUC__
#define ILOG2(x)				\
  ({						\
    assert((x) <= 0xFFFFFFFFL);			\
    DO_ILOG2(x);				\
  })
#else
#define ILOG2(x)  DO_ILOG2(x)
#endif

/* As the macro above. */
int ilog2(word_t x);

/* These functions return the place of the highest and lowest set bit
   of the given argument.  The lowest bit is bit 0.  Note that
     log2(x) =
       0, if x == 0
       1 + highest_bit(x), otherwise. */

int highest_bit(word_t x);
int lowest_bit(word_t x);

/* Return the number of set bits in the given argument.  Prefer the
   macro if in a tight loop. */
int count_bits(word_t x);

/* Return the word with bytes swapped.  Prefer the macro in a tight
   loop. */
word_t swap_bytes(word_t x);

#ifdef __GNUC__
#define COUNT_BITS(x)							\
  ({									\
    word_t _x = x;							\
    _x = _x - ((_x >> 1) & 033333333333) - ((_x >> 2) & 011111111111);	\
    _x = (_x + (_x >> 3)) & 030707070707;				\
    _x = _x + (_x >> 6);						\
    _x = (_x + (_x >> 12) + (_x >> 24)) & 077;				\
    _x;									\
  })
#define SWAP_BYTES(x)				\
  ({						\
    word_t _x = x, _m = 0xFF00FFL;		\
    _x = ((_x & _m) << 8) | ((_x >> 8) & _m);	\
    _x = (_x << 16) | (_x >> 16);		\
    _x;						\
  })
#else
#define COUNT_BITS(x)  count_bits(x)
#define SWAP_BYTES(x)  swap_bytes(x)
#endif


#endif /* INCL_BITOPS_H */
