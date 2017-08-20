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

#include "includes.h"
#include "shades.h"
#include "bitops.h"

static char *rev_id = "$Id: bitops.c,v 1.4 1997/02/25 15:41:53 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


static int highest_bit_index[] = {
  0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

int highest_bit(word_t x)
{
  int n = 0;

  if (x >> 16) {
    n = 16;
    x >>= 16;
  }
  if (x >> 8) {
    n += 8;
    x >>= 8;
  }
  return n + highest_bit_index[x];
}


int ilog2(word_t x)
{
  int n;

  if (!x)
    return 0;

  n = 1;
  if (x >> 16) {
    n = 16;
    x >>= 16;
  }
  if (x >> 8) {
    n += 8;
    x >>= 8;
  }
  return n + highest_bit_index[x];
}


static int lowest_bit_index[] = {
  0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
};

int lowest_bit(word_t x)
{
  int n = 0;

  if (!(x << 16)) {
    n = 16;
    x >>= 16;
  }
  if (!(x << 8)) {
    n += 8;
    x >>= 8;
  }
  return n + lowest_bit_index[x & 0xFF];
}


/* This routine is a hand-optimized version of the famous HAKMEM
   trick.  Its drawback is that it requires loading rather large
   values to registers so if you use it repetitively e.g. inside a
   loop, prefer the macro given in `bitops.h'.  */
int count_bits(word_t x)
{
  x = x - ((x >> 1) & 033333333333) - ((x >> 2) & 011111111111);
  x = (x + (x >> 3)) & 030707070707;
  x = x + (x >> 6);
  x = (x + (x >> 12) + (x >> 24)) & 077;
  return x;
}


word_t swap_bytes(word_t x)
{
  word_t m = 0xFF00FFL;
  /* First swap bytes 1 and 2 with each other, and simultaneously
     bytes 3 and 4. */
  x = ((x & m) << 8) | ((x >> 8) & m);
  /* Swap the two half-words, and return the result. */
  return (x << 16) | (x >> 16);
}
