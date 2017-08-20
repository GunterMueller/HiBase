/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Sirkku-Liisa Paajanen <sirkku@iki.fi>
 */

/* A 4-way width and path compressed trie with at most 32-bit keys and
   values of the word type TAGGED (see cells.h and cells-def.h).  
   For more information about tries, see for example Robert
   Sedgewick's Algorithms, Addison-Wesley, 1988, pp. 245-259. */

#ifndef INCL_TRIEV2_H
#define INCL_TRIEV2_H 1

#include "includes.h"
#include "shades.h"


/* The keys are at most 32 bits wide and we use a quadtrie, so any
   path is at most 16 nodes deep.  A node is at most 5 words.
   Additionally, breaking up common prefixing can happen at most
   once. */
#define TRIEV2_MAX_ALLOCATION  ((16 + 1) * 5)

/* Return the TAGGED data word stored behind the given key, or
   NULL_WORD if the key was not found.  The `key_length', the number
   of valid bits in the key, must be even and at most 32. */
word_t triev2_find(ptr_t trie_root, word_t key, int key_length);

/* Return non-zero if the given key exists in the trie. */
int triev2_contains(ptr_t trie_root, word_t key, int key_length);

/* Return the the greatest key in the tree pointed by `trie_root´, or
   NULL_WORD if the key was not found.  The `key_length', the number
   of valid bits in the key, must be even and at most 32. */
word_t triev2_find_max(ptr_t trie_root, int key_length);

#if !defined(__GNUC__) || !defined(NDEBUG) || defined(__STRICT_ANSI__)

/* Return the TAGGED data stored behind the given key.  The given key
   MUST exist in the trie. */
word_t triev2_find_quick(ptr_t p, word_t key, int key_length);

#else

/* If aiming for speed, the above function is defined as inlinable
   below.  Note that it uses as few new registers as possible by
   minimizing the scope of local variables `prefix_length' and
   `data'. */
extern inline word_t triev2_find_quick(ptr_t p, word_t key, int key_length)
{
  extern unsigned char offset[15][4];

  /* Remove the insignificant bits of the key. */
  key <<= 32 - key_length;
next_level:
  /* Handle a possible common prefix. */
  {
    int prefix_length = (p[0] >> 19) & 0x1E;
    key <<= prefix_length;
    key_length -= prefix_length;
  }
  /* Branch by the 2 uppermost unused bits of the key, 30 = 32 - 2. */
  { 
    word_t data = p[offset[CELL_TYPE(p) - CELL_trie1234][key >> 30]];
    /* If we reached the leaf, return what we found. */
    if (key_length == 2)
      return data;
    p = WORD_TO_PTR(data);
  }
  /* Remove two bits from the key. */
  key <<= 2;
  key_length -= 2;

  goto next_level;
}

#endif

/* Return the TAGGED data word stored behind the `*keyp'.  If the key
   is not present, then return the greatest key less than `*keyp' and
   assign that key value to `*keyp'.  Return NULL_WORD if no key less
   or equal to `*keyp' was not found.  The `key_length', the number of
   valid bits in the key, must be even and at most 32. */
word_t triev2_find_at_most(ptr_t trie_root, word_t *keyp, int key_length);

/* Given a `key' which is divisible by 16, return a key in the range
   from [key .. key + 15] which does not exists in the given trie.
   Return 0xFFFFFFFFL if no unused key exists in the specified key
   range. */
word_t triev2_find_nonexistent_key(ptr_t root, word_t key, int key_length);

/* Insert (or change) the TAGGED data word stored behind the given
   key.  If there is no old data of for the same key or if `f' is
   NULL, then insert `data', otherwise insert the value returned by
   `f(old_data, data, context)', where `old_data' is the old value
   stored in the given key position.  `data' must not be NULL_WORD.
   Returns the new root.

   If it is known that the key to be inserted is already in the trie,
   `f' is not NULL, and `f(old_data, data, context) returns `old_data'
   in more than ~25% of the cases, I suggest using `triev2_delete'
   instead.  */
ptr_t triev2_insert(ptr_t trie_root,
		    word_t key,
		    int key_length,
		    word_t (*f)(word_t, word_t, void *), void *context,
		    word_t data);

/* If the `key' is known to exist in the trie, then replace it with
   `data', or if `f' is non-NULL, with `f(old_data, data, context)'.
   This is noticeably faster than either `triev2_insert' or
   `triev2_delete', but can be used ONLY when the key really is in the
   trie, otherwise it may go berserk.  */
ptr_t triev2_replace(ptr_t trie_root,
		     word_t key,
		     int key_length,
		     word_t (*f)(word_t, word_t, void *), void *context,
		     word_t data);

/* Delete or update the pointer stored behind the key and return the
   new root of the trie. Return the initial root, if the key doesn't
   exist or the update function returns the original data.  Delete the
   value if `data' is, or `f(old_data, data, context)' returns
   `NULL_WORD'.  If `data' is not `NULL_WORD', or `f(old_data, data,
   context)' does not return `NULL_WORD', then insert that value.
   Here `old_data' is the old value stored in the given key
   position. */
ptr_t triev2_delete(ptr_t trie_root,
		    word_t key,
		    int key_length,
		    word_t (*f)(word_t, word_t, void *), void *context,
		    word_t data);

#endif /* INCL_TRIEV2_H */
