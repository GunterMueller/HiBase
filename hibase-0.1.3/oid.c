/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

#include "includes.h"
#include "shades.h"
#include "root.h"
#include "cells.h"
#include "triev2.h"
#include "oid.h"


static char *rev_id = "$Id: oid.c,v 1.7 1997/09/18 12:39:51 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


/* Allocate a new object identity and store the given value for that
   oid. */
oid_t oid_allocate(word_t value)
{
  oid_t oid, tmp;
  int i;

  /* Try the `ROOT_oid_freelist' for oids freed during this commit
     group. */
  if (GET_ROOT_WORD(oid_freelist) != NULL_WORD) {
    oid = GET_ROOT_PTR(oid_freelist)[1];
    SET_ROOT_WORD(oid_freelist, GET_ROOT_PTR(oid_freelist)[2]);
    goto found;
  }

  /* Try looking around the `ROOT_oid_allocation_cursor', and if it
     fails, try the next, and possibly the third range also. */
  tmp = GET_ROOT_WORD(oid_allocation_cursor);
  assert(tmp % 16 == 0);
  oid = triev2_find_nonexistent_key(GET_ROOT_PTR(oid_map), tmp, 30);
  if (oid == 0xFFFFFFFFL) {
    tmp += 16;
    if (tmp >= GET_ROOT_WORD(oid_max))
      tmp = 0;
    SET_ROOT_WORD(oid_allocation_cursor, tmp);
    /* Try the next two ranges. */
    for (i = 2; i != 0; i--) {
      oid = triev2_find_nonexistent_key(GET_ROOT_PTR(oid_map), tmp, 30);
      if (oid == 0xFFFFFFFFL) {
	tmp += 16;
	if (tmp >= GET_ROOT_WORD(oid_max))
	  tmp = 0;
	SET_ROOT_WORD(oid_allocation_cursor, tmp);
      } else if (oid < GET_ROOT_WORD(oid_max)) {
	assert(oid < tmp + 16);
	goto found;
      }
    }
  } else if (oid < GET_ROOT_WORD(oid_max)) {
    assert(oid < tmp + 16);
    goto found;
  }

  /* Try looking at a random point. */
  if (GET_ROOT_WORD(oid_max) > 0) {
    /* Try first the place where we previously found an unused key.
       This way we should get fuller leaf nodes. */
    oid = GET_ROOT_WORD(oid_prev_random);
    if (oid != 0xFFFFFFFFL) {
      assert(oid % 16 == 0);
      oid = triev2_find_nonexistent_key(GET_ROOT_PTR(oid_map), oid, 30);
      if (oid < GET_ROOT_WORD(oid_max))
	goto found;
    }

    /* If there are only relatively few unused OIDs in the current OID
       set, then don't try a random OID search at all in order to save
       time, otherwise loop several times in order to save space. */
    if (GET_ROOT_WORD(oid_in_use) + (GET_ROOT_WORD(oid_in_use) >> 4)
	> GET_ROOT_WORD(oid_max))
      i = 0;
    else if (GET_ROOT_WORD(oid_in_use) + (GET_ROOT_WORD(oid_in_use) >> 3)
	     > GET_ROOT_WORD(oid_max))
      i = 1;
    else if (GET_ROOT_WORD(oid_in_use) + (GET_ROOT_WORD(oid_in_use) >> 2)
	     > GET_ROOT_WORD(oid_max))
      i = 2;
    else if (GET_ROOT_WORD(oid_in_use) + (GET_ROOT_WORD(oid_in_use) >> 1)
	     > GET_ROOT_WORD(oid_max))
      i = 5;
    else
      i = 16;
    for (; i != 0; i--) {
      /* Pick a random point, and try it. */
      tmp = random() % GET_ROOT_WORD(oid_max);
      tmp &= 0xFFFFFFF0L;		/* Round towards lower 16's. */
      oid = triev2_find_nonexistent_key(GET_ROOT_PTR(oid_map), tmp, 30);
      if (oid < GET_ROOT_WORD(oid_max)) {
	/* Memorize this point so that we try it first the next
           time. */
	SET_ROOT_WORD(oid_prev_random, tmp);
	goto found;
      }
    }
    SET_ROOT_WORD(oid_prev_random, 0xFFFFFFFFL);
  }

  /* Finally, if all of the above failed, allocate a totally new
     oid by incrementing ROOT_oid_max. */
  oid = GET_ROOT_WORD(oid_max);
  SET_ROOT_WORD(oid_max, GET_ROOT_WORD(oid_max) + 1);
  
found:
  assert(triev2_find(GET_ROOT_PTR(oid_map), oid, 30) == NULL_WORD);
  SET_ROOT_WORD(oid_in_use, GET_ROOT_WORD(oid_in_use) + 1);
  SET_ROOT_PTR(oid_map,
	       triev2_insert(GET_ROOT_PTR(oid_map),
			     oid,
			     30, /* 2 bits will be reserved for the tag. */
			     NULL, NULL,
			     value));
  /* Return the tagged oid. */
  return (oid << 2) | 0x2;
}


static word_t old_value;

static word_t oid_dispose_f(word_t old_data, word_t new_data, void *dummy)
{
  assert(new_data == NULL_WORD);
  old_value = old_data;
  return NULL_WORD;
}

/* Remove the given oid from the global oid map.  Returns the value
   that the object identity referred to.  Assumes the OID does
   exist before the call. */
word_t oid_dispose(oid_t oid)
{
  ptr_t freelist;

  assert(TAGGED_IS_OID(oid));
  oid >>= 2;
  assert(oid <= GET_ROOT_WORD(oid_max));

  freelist = allocate(3, CELL_oid_freelist);
  freelist[1] = oid;
  freelist[2] = GET_ROOT_WORD(oid_freelist);
  SET_ROOT_PTR(oid_freelist, freelist);
  SET_ROOT_WORD(oid_in_use, GET_ROOT_WORD(oid_in_use) - 1);

  old_value = NULL_WORD;
  SET_ROOT_PTR(oid_map,
	       triev2_delete(GET_ROOT_PTR(oid_map),
			     oid,
			     30,
			     oid_dispose_f, NULL,
			     NULL_WORD));
  assert(triev2_find(GET_ROOT_PTR(oid_map), oid, 30) == NULL_WORD);
  return old_value;
}


word_t oid_ref(oid_t oid)
{
  assert(TAGGED_IS_OID(oid));
  oid >>= 2;
  assert(oid <= GET_ROOT_WORD(oid_max));
  return triev2_find(GET_ROOT_PTR(oid_map),
		     oid,
		     30);
}


static word_t oid_update_f(word_t old_data, word_t new_data, void *dummy)
{
  old_value = old_data;
  return new_data;
}

word_t oid_update(oid_t oid, word_t new_value)
{
  assert(TAGGED_IS_OID(oid));
  old_value = NULL_WORD;
  assert(oid <= GET_ROOT_WORD(oid_max));
  SET_ROOT_PTR(oid_map,
	       triev2_insert(GET_ROOT_PTR(oid_map),
			     oid,
			     30,
			     oid_update_f, NULL,
			     new_value));
  return old_value;
}


/* Called to initialize the oid system, particularly the oid cache. */
void oid_init(void)
{
  /* XXX No oid cache yet to initialize. */
}
