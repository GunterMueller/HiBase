/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Sirkku-Liisa Paajanen <sirkku@cs.hut.fi>
 */

/* A 4-way trie with one-word key and arbitrary pointers to data cells.
   For more information about tries, see for example Robert Sedgewick's 
   Algorithms, Addison-Wesley, 1988, pp. 245-259. 
 */

#ifndef INCL_TRIE_H
#define INCL_TRIE_H 1


#include "includes.h"
#include "shades.h"


/* The keys are 32 bits wide and we use a quadtrie, so any path is at
   most 16 nodes deep.  A node is at most 5 words.  Breaking up common
   prefixing can happen at most once. */
#define TRIE_MAX_ALLOCATION  ((16 + 1) * 5)


/* Return the pointer stored behind the given key, or NULL_PTR if not
   found. */
ptr_t trie_find(ptr_t trie_root, word_t key);


/* Find a key that's equal to or greater than the given key. If operation 
   fails, return NULL_PTR, else store the found key in `*key_p' and return 
   the data associated to it. */  
ptr_t trie_find_at_least(ptr_t trie_root, word_t *key_p);


/* Insert (or change) the pointer stored behind the given key.  If `f'
   is NULL, then insert `data', otherwise insert the value returned by
   `f(old_data, data)', where `old_data' is the old value stored in
   the given key position.  Returns the new root. */
ptr_t trie_insert(ptr_t trie_root, 
		  word_t key, 
		  ptr_t (*f)(ptr_t, ptr_t),
		  ptr_t data);


/* Delete or update the pointer stored behind the key. Return the
   initial root, if the key doesn't exist. Otherwise, if `f' is NULL,
   then delete the pointer, otherwise insert the value returned by
   `f(old_data, data)', where `old_data' is the old value stored in
   the given key position. Return the root of the new trie. */
ptr_t trie_delete(ptr_t trie_root, 
		  word_t key,
		  ptr_t (*f)(ptr_t, ptr_t),
		  ptr_t data);

/* Insert (or change) the given set of pointers . Return the new root. */
ptr_t trie_insert_set(ptr_t trie_root, 
		      word_t key[], 
		      ptr_t data[], 
		      int number_of_keys);

/* Delete all the keys in the given `key[]'.  Return the new
   root if the deletion succeeded, the old root if the deletion
   failed or none of the keys existed in the tree.  */
ptr_t trie_delete_set(ptr_t trie_root,
		      word_t key[], 
		      int number_of_keys);

#endif /* INCL_TRIE_H */



