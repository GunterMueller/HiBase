/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * Copyright (c) 2001 Kenneth Oksanen
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* "Smart pointers" is a means of having pointers from C (or some
   other language) refer to cells in Shades so that Shades is able to
   recognize that these cells are live and redirect all pointer values
   accordingly.  

   However, unlike the root block, smart pointers do NOT survive a
   crash.  Therefore you can consider your data to be fully committed
   when the it is referred (directly or indirectly) from the root
   block and you have performed a successful group commit.

   The standard way of using smart pointers is as follows:

   #include "smart_ptr.h"
   ...
   {
     smart_ptr_t p;
     smart_ptr_init(&p, GET_ROOT_PTR(test1));
     ...
     if (!can_allocate(2))
       flush_batch();
     assert(can_allocate(2));
     smart_ptr_assign(&p, allocate(2, CELL_word_vector));
     smart_ptr_ref(&p)[1] = 42;
     ...
   or
     ...
     if (!can_allocate(TRIE_MAX_ALLOCATION))
       flush_batch();
     assert(can_allocate(TRIE_MAX_ALLOCATION));
     smart_ptr_assign(&p, trie_insert(smart_ptr_ref(&p), ..));
     ...
     SET_ROOT_PTR(test1, smart_ptr_ref(&p));
     smart_ptr_uninit(&p);
   }

   It is fatal to assign or ref uninited smart pointer, to init a
   smart pointer is inited twice without uniniting it in between, or
   to leave inited smart pointers uninited when freeing them. */


#ifndef INCL_SMARTPTR
#define INCL_SMARTPTR  1

#include "includes.h"
#include "shades.h"

typedef struct smart_ptr_t {
  word_t ptr;
  struct smart_ptr_t *next, *prev;
} smart_ptr_t;

extern smart_ptr_t first_smart_ptr;

#if !defined(__GNUC__) || !defined(NDEBUG) || !defined(__STRICT_ANSI__)

void smart_ptr_init(smart_ptr_t *ptr, ptr_t value);
void smart_ptr_uninit(smart_ptr_t *ptr);
void smart_ptr_assign(smart_ptr_t *ptr, ptr_t value);
ptr_t smart_ptr_ref(smart_ptr_t *ptr);

#else

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

#endif /* INCL_SMARTPTR */
