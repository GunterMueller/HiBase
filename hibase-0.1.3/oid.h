/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Object IDentities as understood by the Objectionably Oriented.
 */


#ifndef INCL_OID
#define INCL_OID 1

#include "includes.h"
#include "shades.h"
#include "triev2.h"


typedef word_t oid_t;

#define INVALID_OID  0xFFFFFFFFL


/* XXX Introduce the oid cache. */


/* Allocate a new object identity and store the given value for that
   oid. */
oid_t oid_allocate(word_t value);

#define OID_ALLOCATE_MAX_ALLOCATION  TRIEV2_MAX_ALLOCATION


/* Remove the given oid from the global oid map.  Returns the value
   that the object identity referred to.  Assumes the OID does exist
   before the call. */
word_t oid_dispose(oid_t oid);

/* 3 additional words for the `CELL_oid_freelist'. */
#define OID_DISPOSE_MAX_ALLOCATION  (TRIEV2_MAX_ALLOCATION + 3)	


word_t oid_ref(oid_t oid);


/* Returns the old value. */
word_t oid_update(oid_t oid, word_t new_value);

#define OID_UPDATE_MAX_ALLOCATION  TRIEV2_MAX_ALLOCATION


/* Called to initialize the oid system, particularly the oid cache. */
void oid_init(void);


#endif /* INCL_OID */
