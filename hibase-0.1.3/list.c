/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Definitions and basic tools for lithp protheththing.
 */


#include "list.h"


word_t car(ptr_t p)
{
  assert(CELL_TYPE(p) == CELL_list);
  return p[1];
}

word_t cdr(ptr_t p)
{
  assert(CELL_TYPE(p) == CELL_list);
  return p[2];
}


ptr_t cons(ptr_t a, ptr_t d)
{
  ptr_t r = allocate(3, CELL_list);
  r[1] = PTR_TO_WORD(a);
  r[2] = PTR_TO_WORD(d);
  return r;
}

word_t wcons(word_t a, word_t d)
{
  ptr_t r = allocate(3, CELL_list);
  r[1] = a;
  r[2] = d;
  return PTR_TO_WORD(r);
}

int list_length(ptr_t l)
{
  int n = 0;

  for (; l != NULL_PTR; l = WORD_TO_PTR(CDR(l)))
    n++;
  return n;
}


void print_list(ptr_t list)
{
  ptr_t p = list, q;
  int number_of_words, i;

  fprintf(stderr, "\n( ");
  while (p != NULL_PTR) {
    q = WORD_TO_PTR(p[1]);
    if (CELL_TYPE(q) == CELL_word_vector) {
      number_of_words = q[0] & 0xFFFFFF;
      if (number_of_words == 1) 
	fprintf(stderr, "%lu ", q[1]);
      else {
	fprintf(stderr, "[ "); 
	for (i = 0; i < number_of_words; i++)
	  fprintf(stderr, "%lu ", q[i + 1]);
	fprintf(stderr, "] ");
      }
    } else
      cell_fprint(stdout, q, 16);
    /* p = p->next. */
    p = WORD_TO_PTR(p[2]);
  }
  fprintf(stderr, ")\n");
  return;
}
