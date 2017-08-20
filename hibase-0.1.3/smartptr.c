/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * Copyright (c) 2001 Kenneth Oksanen
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

#include "includes.h"
#include "smartptr.h"

smart_ptr_t first_smart_ptr = {
  NULL_WORD,
  &first_smart_ptr,
  &first_smart_ptr
};

#if !defined(__GNUC__) || !defined(NDEBUG) || !defined(__STRICT_ANSI__)

void smart_ptr_init(smart_ptr_t *ptr, ptr_t value)
{
  ptr->ptr = PTR_TO_WORD(value);
  (ptr->next = first_smart_ptr.next)->prev = ptr;
  ptr->prev = &first_smart_ptr;
  first_smart_ptr.next = ptr;
}

void smart_ptr_uninit(smart_ptr_t *ptr)
{
  smart_ptr_t *prev, *next;

  next = ptr->next;
  prev = ptr->prev;
  next->prev = prev;
  prev->next = next;
}

void smart_ptr_assign(smart_ptr_t *ptr, ptr_t value)
{
  ptr->ptr = PTR_TO_WORD(value);
}

ptr_t smart_ptr_ref(smart_ptr_t *ptr)
{
  return WORD_TO_PTR(ptr->ptr);
}

#endif
