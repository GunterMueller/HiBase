/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Definitions and basic tools for lithp protheththing.
 */


#ifndef INCL_LIST
#define INCL_LIST 1


#include "includes.h"
#include "cells.h"
#include "shades.h"


word_t car(ptr_t);
word_t cdr(ptr_t);


#ifdef __GNUC__

#define CAR(p)					\
  ({						\
    assert(CELL_TYPE(p) == CELL_list);		\
    (p)[1];					\
  })
#define CDR(p)					\
  ({						\
    assert(CELL_TYPE(p) == CELL_list);		\
    (p)[2];					\
  })

#else

#define CAR(p)  car(p)
#define CDR(p)  cdr(p)

#endif

#define WCAR(x)  CAR(WORD_TO_PTR(x))
#define WCDR(x)  CDR(WORD_TO_PTR(x))

#define CAAR(p)   WCAR(CAR(p))
#define CADR(p)   WCAR(CDR(p))
#define CDAR(p)   WCDR(CAR(p))
#define CDDR(p)   WCDR(CDR(p))

#define WCAAR(x)   WCAR(WCAR(x))
#define WCADR(x)   WCAR(WCDR(x))
#define WCDAR(x)   WCDR(WCAR(x))
#define WCDDR(x)   WCDR(WCDR(x))

#define CAAAR(p)   WCAR(CAAR(p))
#define CAADR(p)   WCAR(CADR(p))
#define CADAR(p)   WCAR(CDAR(p))
#define CADDR(p)   WCAR(CDDR(p))
#define CDAAR(p)   WCDR(CAAR(p))
#define CDADR(p)   WCDR(CADR(p))
#define CDDAR(p)   WCDR(CDAR(p))
#define CDDDR(p)   WCDR(CDDR(p))

#define WCAAAR(x)   WCAR(WCAAR(x))
#define WCAADR(x)   WCAR(WCADR(x))
#define WCADAR(x)   WCAR(WCDAR(x))
#define WCADDR(x)   WCAR(WCDDR(x))
#define WCDAAR(x)   WCDR(WCAAR(x))
#define WCDADR(x)   WCDR(WCADR(x))
#define WCDDAR(x)   WCDR(WCDAR(x))
#define WCDDDR(x)   WCDR(WCDDR(x))

#define CAAAAR(p)   WCAR(CAAAR(p))
#define CAAADR(p)   WCAR(CAADR(p))
#define CAADAR(p)   WCAR(CADAR(p))
#define CAADDR(p)   WCAR(CADDR(p))
#define CADAAR(p)   WCAR(CDAAR(p))
#define CADADR(p)   WCAR(CDADR(p))
#define CADDAR(p)   WCAR(CDDAR(p))
#define CADDDR(p)   WCAR(CDDDR(p))
#define CDAAAR(p)   WCDR(CAAAR(p))
#define CDAADR(p)   WCDR(CAADR(p))
#define CDADAR(p)   WCDR(CADAR(p))
#define CDADDR(p)   WCDR(CADDR(p))
#define CDDAAR(p)   WCDR(CDAAR(p))
#define CDDADR(p)   WCDR(CDADR(p))
#define CDDDAR(p)   WCDR(CDDAR(p))
#define CDDDDR(p)   WCDR(CDDDR(p))

#define WCAAAAR(x)   WCAR(WCAAAR(x))
#define WCAAADR(x)   WCAR(WCAADR(x))
#define WCAADAR(x)   WCAR(WCADAR(x))
#define WCAADDR(x)   WCAR(WCADDR(x))
#define WCADAAR(x)   WCAR(WCDAAR(x))
#define WCADADR(x)   WCAR(WCDADR(x))
#define WCADDAR(x)   WCAR(WCDDAR(x))
#define WCADDDR(x)   WCAR(WCDDDR(x))
#define WCDAAAR(x)   WCDR(WCAAAR(x))
#define WCDAADR(x)   WCDR(WCAADR(x))
#define WCDADAR(x)   WCDR(WCADAR(x))
#define WCDADDR(x)   WCDR(WCADDR(x))
#define WCDDAAR(x)   WCDR(WCDAAR(x))
#define WCDDADR(x)   WCDR(WCDADR(x))
#define WCDDDAR(x)   WCDR(WCDDAR(x))
#define WCDDDDR(x)   WCDR(WCDDDR(x))


#define CONS_MAX_ALLOCATION  3


ptr_t cons(ptr_t, ptr_t);
word_t wcons(word_t, word_t);
int list_length(ptr_t);

#ifdef __GNUC__

#define CONS(a, d)				\
  ({						\
    ptr_t _r = allocate(3, CELL_list);		\
    _r[1] = PTR_TO_WORD(a);			\
    _r[2] = PTR_TO_WORD(d);			\
    _r;						\
  })
#define WCONS(a, d)				\
  ({						\
    ptr_t _r = allocate(3, CELL_list);		\
    _r[1] = (a);				\
    _r[2] = (d);				\
    PTR_TO_WORD(_r);				\
  })
#define LIST_LENGTH(l)					\
  ({							\
    ptr_t _l;						\
    int _n = 0;						\
							\
    for (_l = (l); _l != NULL_PTR; _l = CDR(_l))	\
      _n++;						\
    _n;							\
  })

#else

#define CONS(a, d)  cons((a), (d))
#define WCONS(a, d)  wcons((a), (d))
#define LIST_LENGTH(l)  list_length(l)

#endif

void print_list(ptr_t);

#endif /* INCL_TYPES */
