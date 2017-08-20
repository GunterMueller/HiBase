/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Sirkku-Liisa Paajanen <sirkku@iki.fi>
 */

#include "includes.h"
#include "shades.h"
#include "triev2.h"

static char *rev_id = "$Id: triev2.c,v 1.113 1998/03/19 16:36:53 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

/* Forward declaration. */
int triev2_make_assertions(ptr_t root, int key_length);

#define NODE_PREFIX_MASK  0xFFFFF
#define FULL_PREFIX_MASK  0xFFFFFF
/* Depreciated, although this was useful during development.
   #define I_TO_CELL_TYPE(i)  (0xF ^ (1 << (3 - (i)))) */

/* Given t = CELL_TYPE(p) - CELL_trie0 and b being the two first yet
   unused bits of the key, i = offset[t][b] is either 0 if the key is
   not in the trie, or otherwise p[i] is a pointer to the subtrie
   where the key might reside. */
#if !defined(__GNUC__) || !defined(NDEBUG) || defined(__STRICT_ANSI__)
static
#endif
unsigned char offset[15][4] = {
  { 1, 2, 3, 4 }, /* CELL_trie1234 */
  { 1, 2, 3, 0 }, /* CELL_trie123 */
  { 1, 2, 0, 3 }, /* CELL_trie124 */
  { 1, 2, 0, 0 }, /* CELL_trie12 */
  { 1, 0, 2, 3 }, /* CELL_trie134 */
  { 1, 0, 2, 0 }, /* CELL_trie13 */
  { 1, 0, 0, 2 }, /* CELL_trie14 */
  { 1, 0, 0, 0 }, /* CELL_trie1 */
  { 0, 1, 2, 3 }, /* CELL_trie234 */
  { 0, 1, 2, 0 }, /* CELL_trie23 */
  { 0, 1, 0, 2 }, /* CELL_trie24 */
  { 0, 1, 0, 0 }, /* CELL_trie2 */
  { 0, 0, 1, 2 }, /* CELL_trie34 */
  { 0, 0, 1, 0 }, /* CELL_trie3 */
  { 0, 0, 0, 1 }  /* CELL_trie4 */
};

unsigned char greatest_non_null[15][5] = {
  {0, 1, 2, 3, 4}, /* CELL_trie1234 */
  {0, 1, 2, 3, 3}, /* CELL_trie123 */
  {0, 1, 2, 2, 3}, /* CELL_trie124 */
  {0, 1, 2, 2, 2}, /* CELL_trie12 */
  {0, 1, 1, 2, 3}, /* CELL_trie134 */
  {0, 1, 1, 2, 2}, /* CELL_trie13 */
  {0, 1, 1, 1, 2}, /* CELL_trie14 */
  {0, 1, 1, 1, 1}, /* CELL_trie1 */
  {0, 0, 1, 2, 3}, /* CELL_trie234 */
  {0, 0, 1, 2, 2}, /* CELL_trie23 */
  {0, 0, 1, 1, 2}, /* CELL_trie24 */
  {0, 0, 1, 1, 1}, /* CELL_trie2 */
  {0, 0, 0, 1, 2}, /* CELL_trie34 */
  {0, 0, 0, 1, 1}, /* CELL_trie3 */
  {0, 0, 0, 0, 1}  /* CELL_trie4 */
};

/* deoffset[cti][offset[cti][x]] == x
 */
static unsigned char deoffset[15][4] = {
  { 0, 1, 2, 3 }, 		/* CELL_trie1234 */
  { 0, 1, 2, 0xFF },		/* CELL_trie123 */
  { 0, 1, 3, 0xFF },  		/* CELL_trie124 */
  { 0, 1, 0xFF, 0xFF },		/* CELL_trie12 */
  { 0, 2, 3, 0xFF },		/* CELL_trie134 */
  { 0, 2, 0xFF, 0xFF }, 	/* CELL_trie13 */
  { 0, 3, 0xFF, 0xFF },		/* CELL_trie14 */
  { 0, 0xFF, 0xFF, 0xFF }, 	/* CELL_trie1 */
  { 1, 2, 3, 0xFF }, 		/* CELL_trie234 */
  { 1, 2, 0xFF, 0xFF },		/* CELL_trie23 */
  { 1, 3, 0xFF, 0xFF },		/* CELL_trie24 */
  { 1, 0xFF, 0xFF, 0xFF },	/* CELL_trie2 */
  { 2, 3, 0xFF, 0xFF },		/* CELL_trie34 */
  { 2, 0xFF, 0xFF, 0xFF },	/* CELL_trie3 */
  { 3, 0xFF, 0xFF, 0xFF },	/* CELL_trie4 */
};

static cell_type_t cell_type[15] = {
  CELL_trie1234,  /* 0000 */
  CELL_trie123,   /* 0001 */
  CELL_trie124,   /* 0010 */
  CELL_trie12,    /* 0011 */
  CELL_trie134,   /* 0100 */
  CELL_trie13,    /* 0101 */
  CELL_trie14,    /* 0110 */
  CELL_trie1,     /* 0111 */
  CELL_trie234,   /* 1000 */
  CELL_trie23,    /* 1001 */
  CELL_trie24,    /* 1010 */ 
  CELL_trie2,     /* 1011 */
  CELL_trie34,    /* 1100 */
  CELL_trie3,     /* 1101 */
  CELL_trie4      /* 1110 */
};

static unsigned char first_nonexistent_key[15] = {
  0xFF,	/* CELL_trie1234 */
  3,	/* CELL_trie123  */
  2,	/* CELL_trie124  */
  2,	/* CELL_trie12   */
  1,	/* CELL_trie134  */
  1,	/* CELL_trie13   */
  1,	/* CELL_trie14   */
  1,	/* CELL_trie1    */
  0,	/* CELL_trie234  */
  0,	/* CELL_trie23   */
  0,	/* CELL_trie24   */
  0,	/* CELL_trie2    */
  0,	/* CELL_trie34   */
  0,	/* CELL_trie3    */
  0,	/* CELL_trie4	 */
};

static cell_type_t uni_cell_type[4] = {
  CELL_trie1,
  CELL_trie2,
  CELL_trie3,
  CELL_trie4
};

static cell_type_t bi_cell_type[4][4] = {
  { CELL_bonk, CELL_trie12, CELL_trie13, CELL_trie14 },
  { CELL_trie12, CELL_bonk, CELL_trie23, CELL_trie24 },
  { CELL_trie13, CELL_trie23, CELL_bonk, CELL_trie34 },
  { CELL_trie14, CELL_trie24, CELL_trie34, CELL_bonk }
};

static cell_type_t cell_type_without[15][4] = {
  { CELL_trie234, CELL_trie134, CELL_trie124, CELL_trie123 },
  { CELL_trie23, CELL_trie13, CELL_trie12, CELL_trie123 },
  { CELL_trie24, CELL_trie14, CELL_trie124, CELL_trie12 },
  { CELL_trie2, CELL_trie1, CELL_trie12, CELL_trie12 },
  { CELL_trie34, CELL_trie134, CELL_trie14, CELL_trie13 },
  { CELL_trie3, CELL_trie13, CELL_trie1, CELL_trie13 },
  { CELL_trie4, CELL_trie14, CELL_trie14, CELL_trie1 },
  { CELL_bonk, CELL_trie1, CELL_trie1, CELL_trie1 },
  { CELL_trie234, CELL_trie34, CELL_trie24, CELL_trie23 },
  { CELL_trie23, CELL_trie3, CELL_trie2, CELL_trie23 },
  { CELL_trie24, CELL_trie4, CELL_trie24, CELL_trie2 },
  { CELL_trie2, CELL_bonk, CELL_trie2, CELL_trie2 },
  { CELL_trie34, CELL_trie34, CELL_trie4, CELL_trie3 },
  { CELL_trie3, CELL_trie3, CELL_bonk, CELL_trie3 },
  { CELL_trie4, CELL_trie4, CELL_trie4, CELL_bonk }
};

static unsigned char cell_size[15] = { 
  5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2
};


/* Return the pointer stored behind the given key, or NULL_WORD if not
   found. */
word_t triev2_find(ptr_t p, word_t key, int key_length)
{
  word_t node_prefix, key_prefix, data;
  int prefix_length, i;

  /* Check whether the tree is non-empty. */
  if (p == NULL_PTR)
    return NULL_WORD;
  /* Remove the insignificant bits of the key. */
  assert(key_length > 0);
  key <<= 32 - key_length;
  goto loop;

  /* The loop below traverses through one trie node on each
     iteration. */
  do {
    assert(TAGGED_IS_PTR(data));
    p = WORD_TO_PTR(data);
  loop:
    /* Handle a possible common prefix. */
    prefix_length = (p[0] >> 19) & 0x1E;
    if (prefix_length != 0) {
      node_prefix = p[0] & NODE_PREFIX_MASK;
      key_prefix = key >> (32 - prefix_length);
      if (node_prefix != key_prefix)
	return NULL_WORD;
      /* `prefix_length' uppermost bits of the key have been compared,
         remove them. */
      key <<= prefix_length;
      key_length -= prefix_length;
    }

    /* Branch by the 2 uppermost unused bits of the key, 30 = 32 - 2. */
    i = offset[CELL_TYPE(p) - CELL_trie1234][key >> 30];
    if (i == 0)
      return NULL_WORD;
    data = p[i];
    /* Remove the 2 bits from the key. */
    key <<= 2;
    key_length -= 2;
  } while (key_length != 0);

  return data;
}


/* Return non-zero if the given key exists in the trie. */
int triev2_contains(ptr_t p, word_t key, int key_length)
{
  word_t node_prefix, key_prefix, data;
  int prefix_length, i;

  /* Check whether the tree is non-empty. */
  if (p == NULL_PTR)
    return 0;
  /* Remove the insignificant bits of the key. */
  assert(key_length > 0);
  key <<= 32 - key_length;
  goto loop;

  /* The loop below traverses through one trie node on each
     iteration. */
  do {
    assert(TAGGED_IS_PTR(data));
    p = WORD_TO_PTR(data);
  loop:
    /* Handle a possible common prefix. */
    prefix_length = (p[0] >> 19) & 0x1E;
    if (prefix_length != 0) {
      node_prefix = p[0] & NODE_PREFIX_MASK;
      key_prefix = key >> (32 - prefix_length);
      if (node_prefix != key_prefix)
	return 0;
      /* `prefix_length' uppermost bits of the key have been compared,
         remove them. */
      key <<= prefix_length;
      key_length -= prefix_length;
    }

    /* Branch by the 2 uppermost unused bits of the key, 30 = 32 - 2. */
    i = offset[CELL_TYPE(p) - CELL_trie1234][key >> 30];
    if (i == 0)
      return 0;
    data = p[i];
    /* Remove the 2 bits from the key. */
    key <<= 2;
    key_length -= 2;
  } while (key_length != 0);

  return 1;
}


/* Return the greatest key in the tree pointed by `p', or NULL_WORD if
   such key was not found.  The `key_length', the number of valid bits
   in the key, must be even and at most 32. */
word_t triev2_find_max(ptr_t p, int key_length)
{
  word_t max_key = 0, node_prefix;
  int prefix_length, i;

  /* Check whether the tree is non-empty. */
  if (p == NULL_PTR)
    return NULL_WORD;
  /* Remove the insignificant bits of the key. */
  assert(key_length > 0);

  /* The loop below traverses to the rightmost node of the trie. */
  do {
    /* Insert perfix to the `max_key'. */
    prefix_length = (p[0] >> 19) & 0x1E;
    if (prefix_length != 0) {
      node_prefix = p[0] & NODE_PREFIX_MASK;
      max_key <<= prefix_length;
      max_key |= node_prefix;
      key_length -= prefix_length;
    }
    /* Find the index of the rightmost subtree. */
    i = greatest_non_null[CELL_TYPE(p) - CELL_trie1234][4];
    assert(i > 0);
    /* Insert the 2 bits to the `max_key'. */
    max_key <<= 2;
    max_key |= deoffset[CELL_TYPE(p) - CELL_trie1234][i - 1];
    key_length -= 2;
    p = WORD_TO_PTR(p[i]);
  } while (key_length != 0);

  return max_key;
}


/* Return the TAGGED data word stored behind the `*keyp'.  If the key
   is not present, then return the greatest key less than `*keyp' and
   assign that key value to `*keyp'.  Return NULL_WORD if no key less
   or equal to `*keyp' was not found.  The `key_length', the number of
   valid bits in the key, must be even and at most 32. */
word_t triev2_find_at_most(ptr_t trie_root, word_t *keyp, int key_length)
{
  ptr_t p = trie_root;
  word_t node_prefix, key_prefix, data, final_key = 0;
  int level = 0, prefix_length, i;
  struct {
    ptr_t node;
    int ix;
    int offset;
  } path[16 + 1];  
  
  /* Check whether the tree is non-empty. */
  if (p == NULL_PTR)
    return NULL_WORD;
  path[0].node = p;

  /* Remove the insignificant bits of the key. */
  assert(key_length > 0);
  *keyp <<= 32 - key_length;
  goto loop;

  /* The loop below traverses through one trie node on each
     iteration. */
  do {
    assert(TAGGED_IS_PTR(data));
    p = WORD_TO_PTR(data);
    path[++level].node = p;
  loop:
    /* Handle a possible common prefix. */
    prefix_length = (p[0] >> 19) & 0x1E;
    if (prefix_length != 0) {
      node_prefix = p[0] & NODE_PREFIX_MASK;
      key_prefix = *keyp >> (32 - prefix_length);
      if (node_prefix != key_prefix) 
	goto check_prefix;
      /* `prefix_length' uppermost bits of the key have been compared,
         remove them. */
      *keyp <<= prefix_length;
      key_length -= prefix_length;
      final_key <<= prefix_length;
      final_key |= node_prefix;
    }
    
    /* Branch by the 2 uppermost unused bits of the key, 30 = 32 - 2. */
    path[level].ix = *keyp >> 30;
    i = offset[CELL_TYPE(p) - CELL_trie1234][*keyp >> 30];
    if (i == 0) {
      path[level].offset = 
	greatest_non_null[CELL_TYPE(p) - CELL_trie1234][path[level].ix] + 1;
      goto find_smaller;
    }
    path[level].offset = i; 
    data = p[i];
    /* Remove the 2 bits from the key. */
    *keyp <<= 2;
    key_length -= 2;
    final_key <<= 2;
    final_key |= path[level].ix;
  } while (key_length != 0);
  
  /* `*keyp' exists in the trie. */
  /*During the execution of the algorithm `*keyp' has become 0, and
    `final_key' `*keyp' . */
  *keyp = final_key;
  return data;
  

  /* `*keyp' doesn't exist in the trie. Next step is to find the
     greatest key less than `*keyp'. */
  
check_prefix:
  
  /* Check what the prefix of `p' tells about the keys in a subtree 
     starting from `p'. */ 
  if (node_prefix < key_prefix) {
    /* All the keys in this subtree are smaller than
       `*keyp'. Next step is to find the maximum key of the subtree. */
    if (level == 0)
      /* `data' must be initialized first. */
      data = PTR_TO_WORD(p);
    goto find_max; 
  }

  do {
    /* Go upwards by one node. */ 
    level--;
    if  (level < 0) 
      /* Search failed. All the keys in the tree are greater than 
	 the given key. */
      return NULL_WORD;
    p = path[level].node;
    /* Remove the index from the `final_key'. */
    key_length += 2;
    final_key >>= 2;

  find_smaller:
    
    /* Remove the possible prefix from the `final_key'. That's 
       because we're not yet sure if this node is going to belong 
       to the final path. */
    prefix_length = (p[0] >> 19) & 0x1E;
    key_length += prefix_length;
    final_key >>= prefix_length;
    
    /* Climb along the path upwards node by node and search a 
       the greatest key from left. */
    
    i = path[level].offset - 1;
    
  } while (i < 1);
  
  /* There is a path to the key that's at least the given key. The
     path to that key continues from `p[offset]'. Insert the prefix
     and `path[level].ix' to the `final_key'. */  
  if (prefix_length != 0) {
    node_prefix = p[0] & NODE_PREFIX_MASK;
    final_key <<= prefix_length;
    final_key |= node_prefix;
    key_length -= prefix_length;
  }

  final_key <<= 2;
  assert(deoffset[CELL_TYPE(p) - CELL_trie1234][i - 1] != 0xFF);
  final_key |= deoffset[CELL_TYPE(p) - CELL_trie1234][i - 1]; 
  key_length -= 2;
  
  data = p[i];

find_max:

  if (key_length == 0) { 
    *keyp = final_key;
    return data;
  }
  
  /* In a subtree exist the next greater keys than the given key. 
     Next step is to find and return maximum of the subtree. */
  assert(TAGGED_IS_PTR(data));
  p = WORD_TO_PTR(data);
  /* Handle a possible common prefix. */
  prefix_length = (p[0] >> 19) & 0x1E;
  if (prefix_length != 0) {
    final_key <<= prefix_length;
    final_key |= p[0] & NODE_PREFIX_MASK;
    key_length -= prefix_length;
  }
  
  /* Greatest key of the subtree starting from `p' exists as far
     right as possible. So, next step is to find greatest offset `i'
     and go to `WORD_TO_PTR(p[i])'. */
  i = greatest_non_null[CELL_TYPE(p) - CELL_trie1234][4];
  data = p[i];
  /* Remove the 2 bits from the key. */
  key_length -= 2;
  final_key <<= 2;
  assert(deoffset[CELL_TYPE(p) - CELL_trie1234][i - 1] != 0xFF);
  final_key |= deoffset[CELL_TYPE(p) - CELL_trie1234][i - 1];
  goto find_max;
  
}


/* Return the pointer stored behind the given key.  The given key
   MUST exist in the trie. */
word_t triev2_find_quick(ptr_t p, word_t key, int key_length)
{
  int prefix_length;
  word_t data;

  assert(p != NULL_PTR);
  /* Remove the insignificant bits of the key. */
  assert(key_length > 0);
  key <<= 32 - key_length;

next_level:
  /* Handle a possible common prefix. */
  prefix_length = (p[0] >> 19) & 0x1E;
  assert(prefix_length == 0
	 || (p[0] & NODE_PREFIX_MASK) == key >> (32 - prefix_length));
  key <<= prefix_length;
  key_length -= prefix_length;
  /* Branch by the 2 uppermost unused bits of the key, 30 = 32 - 2. */
  data = p[offset[CELL_TYPE(p) - CELL_trie1234][key >> 30]];
  /* If we reached the leaf, return what we found. */
  if (key_length == 2)
    return data;
  assert(TAGGED_IS_PTR(data));
  p = WORD_TO_PTR(data);
  /* Remove two bits from the key. */
  key <<= 2;
  key_length -= 2;

  goto next_level;
}


/* Given a `key' which is divisible by 16, return a key in the range
   from [key .. key + 15], which does not exists in the given trie.
   Return 0xFFFFFFFFL if no unused key exists in the specified key
   range. */
word_t triev2_find_nonexistent_key(ptr_t p, word_t key, int key_length)
{
  ptr_t uup = NULL_PTR, up = NULL_PTR;
  word_t node_prefix, key_prefix, rest_of_key;
  int prefix_length, i;

  assert(key % 16 == 0);

  /* Check whether the tree is non-empty. */
  if (p == NULL_PTR)
    return key;
  assert(key_length > 0);
  rest_of_key = key << (32 - key_length);
  goto loop;

  /* The loop below traverses through one trie node on each
     iteration. */
  do {
    assert(TAGGED_IS_PTR(p[i]));
    p = WORD_TO_PTR(p[i]);
  loop:
    /* Handle a possible common prefix. */
    prefix_length = (p[0] >> 19) & 0x1E;
    if (prefix_length != 0) {
      node_prefix = p[0] & NODE_PREFIX_MASK;
      key_prefix = rest_of_key >> (32 - prefix_length);
      if (node_prefix != key_prefix)
	return key;
      /* `prefix_length' uppermost bits of the key have been compared,
         remove them. */
      rest_of_key <<= prefix_length;
      key_length -= prefix_length;
    }

    /* Branch by the 2 uppermost unused bits of the key, 30 = 32 - 2. */
    i = offset[CELL_TYPE(p) - CELL_trie1234][rest_of_key >> 30];
    if (i == 0)
      /* The given key wasn't in the trie. */
      return key;
    uup = up; /* `uup' == grand father of `p'. */
    up = p; /* `up' == father of `p'. */
    /* Remove the 2 bits from the key. */
    rest_of_key <<= 2;
    key_length -= 2;
  } while (key_length != 0);

  assert(rest_of_key == 0);

  /* `key' exists in the trie. Find a key that doesn't exist in
     the trie and is in range [`key' ... `key' + 15]. */
  
  /* Go back upwards one level in the trie. `p' refers now to the leaf. */
  p = up;
  up = uup;

  /* Check if `p' has any `NULL_PTR's. */
  if (CELL_TYPE(p) != CELL_trie1234)
    return key + first_nonexistent_key[CELL_TYPE(p) - CELL_trie1234];

  if (((p[0] >> 19) & 0x1E) != 0
      || offset[CELL_TYPE(up) - CELL_trie1234][1] == 0)
    return key + 4;
  p = WORD_TO_PTR(up[offset[CELL_TYPE(up) - CELL_trie1234][1]]);
  assert(((p[0] >> 19) & 0x1E) == 0);
  if (CELL_TYPE(p) != CELL_trie1234)
    return key + 4 + first_nonexistent_key[CELL_TYPE(p) - CELL_trie1234];

  if (offset[CELL_TYPE(up) - CELL_trie1234][2] == 0)
    return key + 8;
  p = WORD_TO_PTR(up[offset[CELL_TYPE(up) - CELL_trie1234][2]]);
  assert(((p[0] >> 19) & 0x1E) == 0);
  if (CELL_TYPE(p) != CELL_trie1234)
    return key + 8 + first_nonexistent_key[CELL_TYPE(p) - CELL_trie1234];

  if (offset[CELL_TYPE(up) - CELL_trie1234][3] == 0)
    return key + 12;
  p = WORD_TO_PTR(up[offset[CELL_TYPE(up) - CELL_trie1234][3]]);
  assert(((p[0] >> 19) & 0x1E) == 0);
  if (CELL_TYPE(p) != CELL_trie1234)
    return key + 12 + first_nonexistent_key[CELL_TYPE(p) - CELL_trie1234];

  return 0xFFFFFFFFL;
}


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
		    word_t data)
{
  word_t node_prefix, key_prefix, new_root = NULL_WORD, old_data = NULL_WORD;
  ptr_t p = trie_root, new_p, pp = &new_root, ap;
  int prefix_length;
#ifndef NDEBUG
  int orig_key_length = key_length;
#endif

  assert(triev2_make_assertions(trie_root, orig_key_length));

  ap = get_allocation_point();
  assert(key_length > 0);
  key <<= 32 - key_length;
  goto loop;

  do {
    assert(TAGGED_IS_PTR(old_data));
    p = WORD_TO_PTR(old_data);

  loop:
    assert((key_length & 0x1) == 0); /* `key_length' is always even. */

    if (p == NULL_PTR) {
      if (key_length > 2) {
	prefix_length = (key_length < 22) ? key_length - 2 : 20; 
	key_prefix = key >> (32 - prefix_length);
	key <<= prefix_length;
	key_length -= prefix_length;
	new_p = allocate(2, uni_cell_type[key >> 30]);
	new_p[0] |= (prefix_length << 19) | key_prefix;
      } else
	new_p = allocate(2, uni_cell_type[key >> 30]);
      *pp = PTR_TO_WORD(new_p);
      pp = &new_p[1];
    } else {
      ptr_t tmp_pp;
      word_t full_prefix;

      full_prefix = p[0] & FULL_PREFIX_MASK;
      if (full_prefix != 0) {
	/* The old node in `p' is prefixed with `prefix_length' bits.
           Get the node's prefix and the corresponding number of bits
           from the key. */
	prefix_length = (full_prefix >> 19) & 0x1E;
	node_prefix = full_prefix & NODE_PREFIX_MASK;
	key_prefix = key >> (32 - prefix_length);
	if (node_prefix != key_prefix)
	  goto split;
	/* The prefixes agree, simply make `new_p' a copy of `p',
	   this time including also `p[0]', since it contains the
	   prefix. */
	key <<= prefix_length;
	key_length -= prefix_length;
      }
      switch ((CELL_TYPE(p) << 2) + (key >> 30)) {
      case (CELL_trie1234 << 2) + 0:
	new_p = allocate(5, CELL_trie1234);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	new_p[3] = p[3];
	new_p[4] = p[4];
	break;
      case (CELL_trie1234 << 2) + 1:
	new_p = allocate(5, CELL_trie1234);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	new_p[3] = p[3];
	new_p[4] = p[4];
	break;
      case (CELL_trie1234 << 2) + 2:
	new_p = allocate(5, CELL_trie1234);
	new_p[1] = p[1];
	new_p[2] = p[2];
	tmp_pp = &new_p[3];
	old_data = p[3];
	new_p[4] = p[4];
	break;
      case (CELL_trie1234 << 2) + 3:
	new_p = allocate(5, CELL_trie1234);
	new_p[1] = p[1];
	new_p[2] = p[2];
	new_p[3] = p[3];
	tmp_pp = &new_p[4];
	old_data = p[4];
	break;
      case (CELL_trie123 << 2) + 0:
	new_p = allocate(4, CELL_trie123);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	new_p[3] = p[3];
	break;
      case (CELL_trie123 << 2) + 1:
	new_p = allocate(4, CELL_trie123);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	new_p[3] = p[3];
	break;
      case (CELL_trie123 << 2) + 2:
	new_p = allocate(4, CELL_trie123);
	new_p[1] = p[1];
	new_p[2] = p[2];
	tmp_pp = &new_p[3];
	old_data = p[3];
	break;
      case (CELL_trie123 << 2) + 3:
	new_p = allocate(5, CELL_trie1234);
	new_p[1] = p[1];
	new_p[2] = p[2];
	new_p[3] = p[3];
	tmp_pp = &new_p[4];
	old_data = NULL_WORD;
	break;
      case (CELL_trie124 << 2) + 0:
	new_p = allocate(4, CELL_trie124);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	new_p[3] = p[3];
	break;
      case (CELL_trie124 << 2) + 1:
	new_p = allocate(4, CELL_trie124);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	new_p[3] = p[3];
	break;
      case (CELL_trie124 << 2) + 2:
	new_p = allocate(5, CELL_trie1234);
	new_p[1] = p[1];
	new_p[2] = p[2];
	tmp_pp = &new_p[3];
	old_data = NULL_WORD;
	new_p[4] = p[3];
	break;
      case (CELL_trie124 << 2) + 3:
	new_p = allocate(4, CELL_trie124);
	new_p[1] = p[1];
	new_p[2] = p[2];
	tmp_pp = &new_p[3];
	old_data = p[3];
	break;
      case (CELL_trie12 << 2) + 0:
	new_p = allocate(3, CELL_trie12);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	break;
      case (CELL_trie12 << 2) + 1:
	new_p = allocate(3, CELL_trie12);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	break;
      case (CELL_trie12 << 2) + 2:
	new_p = allocate(4, CELL_trie123);
	new_p[1] = p[1];
	new_p[2] = p[2];
	tmp_pp = &new_p[3];
	old_data = NULL_WORD;
	break;
      case (CELL_trie12 << 2) + 3:
	new_p = allocate(4, CELL_trie124);
	new_p[1] = p[1];
	new_p[2] = p[2];
	tmp_pp = &new_p[3];
	old_data = NULL_WORD;
	break;
      case (CELL_trie134 << 2) + 0:
	new_p = allocate(4, CELL_trie134);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	new_p[3] = p[3];
	break;
      case (CELL_trie134 << 2) + 1:
	new_p = allocate(5, CELL_trie1234);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	new_p[3] = p[2];
	new_p[4] = p[3];
	break;
      case (CELL_trie134 << 2) + 2:
	new_p = allocate(4, CELL_trie134);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	new_p[3] = p[3];
	break;
      case (CELL_trie134 << 2) + 3:
	new_p = allocate(4, CELL_trie134);
	new_p[1] = p[1];
	new_p[2] = p[2];
	tmp_pp = &new_p[3];
	old_data = p[3];
	break;
      case (CELL_trie13 << 2) + 0:
	new_p = allocate(3, CELL_trie13);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	break;
      case (CELL_trie13 << 2) + 1:
	new_p = allocate(4, CELL_trie123);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	new_p[3] = p[2];
	break;
      case (CELL_trie13 << 2) + 2:
	new_p = allocate(3, CELL_trie13);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	break;
      case (CELL_trie13 << 2) + 3:
	new_p = allocate(4, CELL_trie134);
	new_p[1] = p[1];
	new_p[2] = p[2];
	tmp_pp = &new_p[3];
	old_data = NULL_WORD;
	break;
      case (CELL_trie14 << 2) + 0:
	new_p = allocate(3, CELL_trie14);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	break;
      case (CELL_trie14 << 2) + 1:
	new_p = allocate(4, CELL_trie124);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	new_p[3] = p[2];
	break;
      case (CELL_trie14 << 2) + 2:
	new_p = allocate(4, CELL_trie134);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	new_p[3] = p[2];
	break;
      case (CELL_trie14 << 2) + 3:
	new_p = allocate(3, CELL_trie14);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	break;
      case (CELL_trie1 << 2) + 0:
	new_p = allocate(2, CELL_trie1);
	tmp_pp = &new_p[1];
	old_data = p[1];
	break;
      case (CELL_trie1 << 2) + 1:
	new_p = allocate(3, CELL_trie12);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	break;
      case (CELL_trie1 << 2) + 2:
	new_p = allocate(3, CELL_trie13);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	break;
      case (CELL_trie1 << 2) + 3:
	new_p = allocate(3, CELL_trie14);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	break;
      case (CELL_trie234 << 2) + 0:
	new_p = allocate(5, CELL_trie1234);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	new_p[3] = p[2];
	new_p[4] = p[3];
	break;
      case (CELL_trie234 << 2) + 1:
	new_p = allocate(4, CELL_trie234);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	new_p[3] = p[3];
	break;
      case (CELL_trie234 << 2) + 2:
	new_p = allocate(4, CELL_trie234);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	new_p[3] = p[3];
	break;
      case (CELL_trie234 << 2) + 3:
	new_p = allocate(4, CELL_trie234);
	new_p[1] = p[1];
	new_p[2] = p[2];
	tmp_pp = &new_p[3];
	old_data = p[3];
	break;
      case (CELL_trie23 << 2) + 0:
	new_p = allocate(4, CELL_trie123);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	new_p[3] = p[2];
	break;
      case (CELL_trie23 << 2) + 1:
	new_p = allocate(3, CELL_trie23);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	break;
      case (CELL_trie23 << 2) + 2:
	new_p = allocate(3, CELL_trie23);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	break;
      case (CELL_trie23 << 2) + 3:
	new_p = allocate(4, CELL_trie234);
	new_p[1] = p[1];
	new_p[2] = p[2];
	tmp_pp = &new_p[3];
	old_data = NULL_WORD;
	break;
      case (CELL_trie24 << 2) + 0:
	new_p = allocate(4, CELL_trie124);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	new_p[3] = p[2];
	break;
      case (CELL_trie24 << 2) + 1:
	new_p = allocate(3, CELL_trie24);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	break;
      case (CELL_trie24 << 2) + 2:
	new_p = allocate(4, CELL_trie234);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	new_p[3] = p[2];
	break;
      case (CELL_trie24 << 2) + 3:
	new_p = allocate(3, CELL_trie24);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	break;
      case (CELL_trie2 << 2) + 0:
	new_p = allocate(3, CELL_trie12);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	break;
      case (CELL_trie2 << 2) + 1:
	new_p = allocate(2, CELL_trie2);
	tmp_pp = &new_p[1];
	old_data = p[1];
	break;
      case (CELL_trie2 << 2) + 2:
	new_p = allocate(3, CELL_trie23);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	break;
      case (CELL_trie2 << 2) + 3:
	new_p = allocate(3, CELL_trie24);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	break;
      case (CELL_trie34 << 2) + 0:
	new_p = allocate(4, CELL_trie134);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	new_p[3] = p[2];
	break;
      case (CELL_trie34 << 2) + 1:
	new_p = allocate(4, CELL_trie234);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	new_p[3] = p[2];
	break;
      case (CELL_trie34 << 2) + 2:
	new_p = allocate(3, CELL_trie34);
	tmp_pp = &new_p[1];
	old_data = p[1];
	new_p[2] = p[2];
	break;
      case (CELL_trie34 << 2) + 3:
	new_p = allocate(3, CELL_trie34);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = p[2];
	break;
      case (CELL_trie3 << 2) + 0:
	new_p = allocate(3, CELL_trie13);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	break;
      case (CELL_trie3 << 2) + 1:
	new_p = allocate(3, CELL_trie23);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	break;
      case (CELL_trie3 << 2) + 2:
	new_p = allocate(2, CELL_trie3);
	tmp_pp = &new_p[1];
	old_data = p[1];
	break;
      case (CELL_trie3 << 2) + 3:
	new_p = allocate(3, CELL_trie34);
	new_p[1] = p[1];
	tmp_pp = &new_p[2];
	old_data = NULL_WORD;
	break;
      case (CELL_trie4 << 2) + 0:
	new_p = allocate(3, CELL_trie14);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	break;
      case (CELL_trie4 << 2) + 1:
	new_p = allocate(3, CELL_trie24);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	break;
      case (CELL_trie4 << 2) + 2:
	new_p = allocate(3, CELL_trie34);
	tmp_pp = &new_p[1];
	old_data = NULL_WORD;
	new_p[2] = p[1];
	break;
      case (CELL_trie4 << 2) + 3:
	new_p = allocate(2, CELL_trie4);
	tmp_pp = &new_p[1];
	old_data = p[1];
	break;
      }
      new_p[0] |= full_prefix;
      *pp = PTR_TO_WORD(new_p);
      pp = tmp_pp;
    }

  out:
    /* Drop the compared bits away from the key. */
    key <<= 2;
    key_length -= 2;
  } while (key_length != 0);
  
  assert(key == 0);

  if (old_data != NULL_WORD) {
    if (f != NULL)
      data = f(old_data, data, context);
    if (old_data == data) {
      /* The insertion is nilpotent: retract the work and return the
	 original root. */
      restore_allocation_point(ap);
      return trie_root;
    }
  }
  /* Put the data in the last slot we used.  Then return the new
     path's root.  */
  *pp = data;
  assert(triev2_make_assertions(WORD_TO_PTR(new_root), orig_key_length));
  return WORD_TO_PTR(new_root);

  if (0) {
    ptr_t old_p;
    word_t old_prefix;
    cell_type_t ct;
    int i, cs, cti /* cell type index */, old_prefix_length;

  split:
    /* Make `old_p' the equivalent of `p' except that decrease the
       prefix length so that it can serve as a subnode of the new
       intermediate node, which we create in `new_p'. */
    ct = CELL_TYPE(p);
    cti = ct - CELL_trie1234;
    old_p = allocate(cell_size[cti], ct);
    old_prefix_length =
      /* First bit that differs: */ highest_bit(node_prefix ^ key_prefix)
      /* Decrease if odd: */ & ~0x1;
    /* Take the `old_prefix_length' lowest bits of `node_prefix' to
       the new old node, otherwise it is just like the old node `p'. */
    old_prefix = node_prefix & ((1 << old_prefix_length) - 1);
    old_p[0] |= (old_prefix_length << 19) | old_prefix;
    cs = cell_size[cti];
    for (i = 1; i < cs; i++) 
      old_p[i] = p[i];
      
    prefix_length -= 2 + old_prefix_length;
    ct = bi_cell_type
      [(key << prefix_length) >> 30]
      [(node_prefix >> old_prefix_length) & 0x3];
    cti = ct - CELL_trie1234;
    assert(cell_size[cti] == 3);
    new_p = allocate(3, ct);
    /* Take the agreeing prefix part minus 2 to the prefix of `new_p'. */
    if (prefix_length > 0) {
      new_p[0] |= (prefix_length << 19) | (key >> (32 - prefix_length));
      key_length -= prefix_length;
      key <<= prefix_length;
    }
    /* Make `old_p' a child of `new_p'. */
    i = offset[cti][(node_prefix >> old_prefix_length) & 0x3];
    assert(i != 0);
    new_p[i] = PTR_TO_WORD(old_p);
    *pp = PTR_TO_WORD(new_p);
    pp = &new_p[offset[cti][key >> 30]];
    old_data = NULL_WORD;
    goto out;
  }
}


/* If the `key' is known to exist in the trie, then replace it with
   `data', or if `f' is non-NULL, with `f(old_data, data, context)'.
   This is noticeably faster than either `triev2_insert' or
   `triev2_delete', but can be used ONLY when the key really is in the
   trie, otherwise it may go berserk.  */
ptr_t triev2_replace(ptr_t trie_root,
		     word_t key,
		     int key_length,
		     word_t (*f)(word_t, word_t, void *), void *context,
		     word_t data)
{
  word_t full_prefix, new_root = NULL_WORD, old_data = NULL_WORD;
  ptr_t p = trie_root, new_p, pp = &new_root, ap, tmp_pp;
  int prefix_length;
#ifndef NDEBUG
  int orig_key_length = key_length;
#endif

  assert(triev2_make_assertions(trie_root, orig_key_length));

  ap = get_allocation_point();
  assert(key_length > 0);
  key <<= 32 - key_length;
  goto loop;

  do {
    assert(TAGGED_IS_PTR(old_data));
    p = WORD_TO_PTR(old_data);

  loop:
    assert((key_length & 0x1) == 0); /* `key_length' is always even. */
    assert(p != NULL_PTR);

    full_prefix = p[0] & FULL_PREFIX_MASK;
    if (full_prefix != 0) {
      /* The old node in `p' is prefixed with `prefix_length' bits.
	 Get the node's prefix and the corresponding number of bits
	 from the key. */
      prefix_length = (full_prefix >> 19) & 0x1E;
      assert((full_prefix & NODE_PREFIX_MASK) == key >> (32 - prefix_length));
      key <<= prefix_length;
      key_length -= prefix_length;
    }
    switch ((CELL_TYPE(p) << 2) + (key >> 30)) {
    case (CELL_trie1234 << 2) + 0:
      new_p = allocate(5, CELL_trie1234);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      new_p[4] = p[4];
      break;
    case (CELL_trie1234 << 2) + 1:
      new_p = allocate(5, CELL_trie1234);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      new_p[3] = p[3];
      new_p[4] = p[4];
      break;
    case (CELL_trie1234 << 2) + 2:
      new_p = allocate(5, CELL_trie1234);
      new_p[1] = p[1];
      new_p[2] = p[2];
      tmp_pp = &new_p[3];
      old_data = p[3];
      new_p[4] = p[4];
      break;
    case (CELL_trie1234 << 2) + 3:
      new_p = allocate(5, CELL_trie1234);
      new_p[1] = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      tmp_pp = &new_p[4];
      old_data = p[4];
      break;
    case (CELL_trie123 << 2) + 0:
      new_p = allocate(4, CELL_trie123);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      break;
    case (CELL_trie123 << 2) + 1:
      new_p = allocate(4, CELL_trie123);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      new_p[3] = p[3];
      break;
    case (CELL_trie123 << 2) + 2:
      new_p = allocate(4, CELL_trie123);
      new_p[1] = p[1];
      new_p[2] = p[2];
      tmp_pp = &new_p[3];
      old_data = p[3];
      break;
    case (CELL_trie123 << 2) + 3:
      new_p = allocate(5, CELL_trie1234);
      new_p[1] = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      tmp_pp = &new_p[4];
      old_data = NULL_WORD;
      break;
    case (CELL_trie124 << 2) + 0:
      new_p = allocate(4, CELL_trie124);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      break;
    case (CELL_trie124 << 2) + 1:
      new_p = allocate(4, CELL_trie124);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      new_p[3] = p[3];
      break;
    case (CELL_trie124 << 2) + 2:
      new_p = allocate(5, CELL_trie1234);
      new_p[1] = p[1];
      new_p[2] = p[2];
      tmp_pp = &new_p[3];
      old_data = NULL_WORD;
      new_p[4] = p[3];
      break;
    case (CELL_trie124 << 2) + 3:
      new_p = allocate(4, CELL_trie124);
      new_p[1] = p[1];
      new_p[2] = p[2];
      tmp_pp = &new_p[3];
      old_data = p[3];
      break;
    case (CELL_trie12 << 2) + 0:
      new_p = allocate(3, CELL_trie12);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      break;
    case (CELL_trie12 << 2) + 1:
      new_p = allocate(3, CELL_trie12);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      break;
    case (CELL_trie12 << 2) + 2:
      new_p = allocate(4, CELL_trie123);
      new_p[1] = p[1];
      new_p[2] = p[2];
      tmp_pp = &new_p[3];
      old_data = NULL_WORD;
      break;
    case (CELL_trie12 << 2) + 3:
      new_p = allocate(4, CELL_trie124);
      new_p[1] = p[1];
      new_p[2] = p[2];
      tmp_pp = &new_p[3];
      old_data = NULL_WORD;
      break;
    case (CELL_trie134 << 2) + 0:
      new_p = allocate(4, CELL_trie134);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      break;
    case (CELL_trie134 << 2) + 1:
      new_p = allocate(5, CELL_trie1234);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      new_p[3] = p[2];
      new_p[4] = p[3];
      break;
    case (CELL_trie134 << 2) + 2:
      new_p = allocate(4, CELL_trie134);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      new_p[3] = p[3];
      break;
    case (CELL_trie134 << 2) + 3:
      new_p = allocate(4, CELL_trie134);
      new_p[1] = p[1];
      new_p[2] = p[2];
      tmp_pp = &new_p[3];
      old_data = p[3];
      break;
    case (CELL_trie13 << 2) + 0:
      new_p = allocate(3, CELL_trie13);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      break;
    case (CELL_trie13 << 2) + 1:
      new_p = allocate(4, CELL_trie123);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      new_p[3] = p[2];
      break;
    case (CELL_trie13 << 2) + 2:
      new_p = allocate(3, CELL_trie13);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      break;
    case (CELL_trie13 << 2) + 3:
      new_p = allocate(4, CELL_trie134);
      new_p[1] = p[1];
      new_p[2] = p[2];
      tmp_pp = &new_p[3];
      old_data = NULL_WORD;
      break;
    case (CELL_trie14 << 2) + 0:
      new_p = allocate(3, CELL_trie14);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      break;
    case (CELL_trie14 << 2) + 1:
      new_p = allocate(4, CELL_trie124);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      new_p[3] = p[2];
      break;
    case (CELL_trie14 << 2) + 2:
      new_p = allocate(4, CELL_trie134);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      new_p[3] = p[2];
      break;
    case (CELL_trie14 << 2) + 3:
      new_p = allocate(3, CELL_trie14);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      break;
    case (CELL_trie1 << 2) + 0:
      new_p = allocate(2, CELL_trie1);
      tmp_pp = &new_p[1];
      old_data = p[1];
      break;
    case (CELL_trie1 << 2) + 1:
      new_p = allocate(3, CELL_trie12);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      break;
    case (CELL_trie1 << 2) + 2:
      new_p = allocate(3, CELL_trie13);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      break;
    case (CELL_trie1 << 2) + 3:
      new_p = allocate(3, CELL_trie14);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      break;
    case (CELL_trie234 << 2) + 0:
      new_p = allocate(5, CELL_trie1234);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      new_p[3] = p[2];
      new_p[4] = p[3];
      break;
    case (CELL_trie234 << 2) + 1:
      new_p = allocate(4, CELL_trie234);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      break;
    case (CELL_trie234 << 2) + 2:
      new_p = allocate(4, CELL_trie234);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      new_p[3] = p[3];
      break;
    case (CELL_trie234 << 2) + 3:
      new_p = allocate(4, CELL_trie234);
      new_p[1] = p[1];
      new_p[2] = p[2];
      tmp_pp = &new_p[3];
      old_data = p[3];
      break;
    case (CELL_trie23 << 2) + 0:
      new_p = allocate(4, CELL_trie123);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      new_p[3] = p[2];
      break;
    case (CELL_trie23 << 2) + 1:
      new_p = allocate(3, CELL_trie23);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      break;
    case (CELL_trie23 << 2) + 2:
      new_p = allocate(3, CELL_trie23);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      break;
    case (CELL_trie23 << 2) + 3:
      new_p = allocate(4, CELL_trie234);
      new_p[1] = p[1];
      new_p[2] = p[2];
      tmp_pp = &new_p[3];
      old_data = NULL_WORD;
      break;
    case (CELL_trie24 << 2) + 0:
      new_p = allocate(4, CELL_trie124);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      new_p[3] = p[2];
      break;
    case (CELL_trie24 << 2) + 1:
      new_p = allocate(3, CELL_trie24);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      break;
    case (CELL_trie24 << 2) + 2:
      new_p = allocate(4, CELL_trie234);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      new_p[3] = p[2];
      break;
    case (CELL_trie24 << 2) + 3:
      new_p = allocate(3, CELL_trie24);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      break;
    case (CELL_trie2 << 2) + 0:
      new_p = allocate(3, CELL_trie12);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      break;
    case (CELL_trie2 << 2) + 1:
      new_p = allocate(2, CELL_trie2);
      tmp_pp = &new_p[1];
      old_data = p[1];
      break;
    case (CELL_trie2 << 2) + 2:
      new_p = allocate(3, CELL_trie23);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      break;
    case (CELL_trie2 << 2) + 3:
      new_p = allocate(3, CELL_trie24);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      break;
    case (CELL_trie34 << 2) + 0:
      new_p = allocate(4, CELL_trie134);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      new_p[3] = p[2];
      break;
    case (CELL_trie34 << 2) + 1:
      new_p = allocate(4, CELL_trie234);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      new_p[3] = p[2];
      break;
    case (CELL_trie34 << 2) + 2:
      new_p = allocate(3, CELL_trie34);
      tmp_pp = &new_p[1];
      old_data = p[1];
      new_p[2] = p[2];
      break;
    case (CELL_trie34 << 2) + 3:
      new_p = allocate(3, CELL_trie34);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = p[2];
      break;
    case (CELL_trie3 << 2) + 0:
      new_p = allocate(3, CELL_trie13);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      break;
    case (CELL_trie3 << 2) + 1:
      new_p = allocate(3, CELL_trie23);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      break;
    case (CELL_trie3 << 2) + 2:
      new_p = allocate(2, CELL_trie3);
      tmp_pp = &new_p[1];
      old_data = p[1];
      break;
    case (CELL_trie3 << 2) + 3:
      new_p = allocate(3, CELL_trie34);
      new_p[1] = p[1];
      tmp_pp = &new_p[2];
      old_data = NULL_WORD;
      break;
    case (CELL_trie4 << 2) + 0:
      new_p = allocate(3, CELL_trie14);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      break;
    case (CELL_trie4 << 2) + 1:
      new_p = allocate(3, CELL_trie24);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      break;
    case (CELL_trie4 << 2) + 2:
      new_p = allocate(3, CELL_trie34);
      tmp_pp = &new_p[1];
      old_data = NULL_WORD;
      new_p[2] = p[1];
      break;
    case (CELL_trie4 << 2) + 3:
      new_p = allocate(2, CELL_trie4);
      tmp_pp = &new_p[1];
      old_data = p[1];
      break;
    }
    new_p[0] |= full_prefix;
    *pp = PTR_TO_WORD(new_p);
    pp = tmp_pp;

    /* Drop the compared bits away from the key. */
    key <<= 2;
    key_length -= 2;
  } while (key_length != 0);
  
  assert(key == 0);

  if (f != NULL)
    data = f(old_data, data, context);
  if (old_data == data) {
    /* The insertion is nilpotent: retract the work and return the
       original root. */
    restore_allocation_point(ap);
    return trie_root;
  }
  /* Put the data in the last slot we used.  Then return the new
     path's root.  */
  *pp = data;
  assert(triev2_make_assertions(WORD_TO_PTR(new_root), orig_key_length));
  return WORD_TO_PTR(new_root);
}


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
		    word_t data)
{
  ptr_t q, p = trie_root, new_p;
  word_t node_prefix, key_prefix, old_data;
  int prefix_length, ix, i, j, cs, level = 0, cti;
  cell_type_t ct;

  struct {
    ptr_t node;
    int ix;
    int offset;
  } path[16 + 1];
#ifndef NDEBUG
  int orig_key_length = key_length;
#endif

  /* Check whether the tree is non-empty. */
  if (p == NULL_PTR)
    return NULL_PTR;
  assert(triev2_make_assertions(trie_root, orig_key_length));
  assert(key_length > 0);
  key <<= 32 - key_length;
  goto loop;

  /* First step is to find data behind `key'. Return initial trie if
     `key' with data doesn't exist in the trie yet. */
  do {
    assert(TAGGED_IS_PTR(old_data));
    p = WORD_TO_PTR(old_data);

  loop:
    /* Handle a possible common prefix. */
    prefix_length = (p[0] >> 19) & 0x1E;
    if (prefix_length != 0) {
      node_prefix = p[0] & NODE_PREFIX_MASK;
      key_prefix = key >> (32 - prefix_length);
      if (node_prefix != key_prefix)
	return trie_root;
      /* `prefix_length' uppermost bits of the key have been compared,
         remove them. */
      key <<= prefix_length;
      key_length -= prefix_length;
    }

    /* Branch by the 2 uppermost unused bits of the key, 30 = 32 - 2. */
    ct = CELL_TYPE(p);
    cti = ct - CELL_trie1234;
    ix = key >> 30;
    i = offset[cti][ix];
    if (i == 0)
      return trie_root;    
    /* Append the new node to the vector `path'. */
    path[level].node = p;
    path[level].ix = ix;
    path[level].offset = i;
    old_data = p[i];
    level++;
    /* Remove the 2 bits from the key. */
    key <<= 2;
    key_length -= 2;
  } while (key_length != 0);

  /* `old_data' points to data behind `key'. Now delete or update the
     data. */

  if (f != NULL)
    /* Update data behind the key. */
    data = f(old_data, data, context);
  if (data == old_data)
    return trie_root;

  if (data == NULL_WORD) {
    int last_internal_node_level = level - 1, q_prefix_length;

    while (level > 0) {
      level--;
      p = path[level].node;
      ct = CELL_TYPE(p);
      cti = ct - CELL_trie1234;
      cs = cell_size[cti];
      switch (cs) {
      case 3:
	q = WORD_TO_PTR(p[3 - path[level].offset]);
	if (level == last_internal_node_level)
	  goto no_merge;
	prefix_length = (p[0] >> 19) & 0x1E;
	q_prefix_length = (q[0] >> 19) & 0x1E;
	if (prefix_length + q_prefix_length <= 18) {
	  /* Join `p' with its only descendant `q'. */
	  i = CELL_TYPE(q);
	  assert(i != CELL_word_vector);
	  new_p = allocate(cell_size[i - CELL_trie1234], i);
	  node_prefix = p[0] & NODE_PREFIX_MASK;
	  node_prefix <<= 2;
	  assert(deoffset[cti][2 - path[level].offset] != 0xFF);
	  node_prefix |= deoffset[cti][2 - path[level].offset];
	  node_prefix <<= q_prefix_length;
	  node_prefix |= q[0] & NODE_PREFIX_MASK;
	  new_p[0] |= 
	    ((prefix_length + q_prefix_length + 2) << 19) | node_prefix;
	  /* Copy the rest of the `q' to `new_p'. */
	  cs = cell_size[i - CELL_trie1234];
	  for (i = 1; i < cs ; i++)
	    new_p[i] = q[i];
	} else {
	no_merge:
	  /* Copy `p' such that its only descendant is `q'. */
	  i = cell_type_without[cti][path[level].ix] - CELL_trie1234;
	  assert(cell_size[i] == 2);
	  new_p = allocate(2, cell_type[i]);
	  new_p[0] |= p[0] & FULL_PREFIX_MASK;
	  new_p[1] = PTR_TO_WORD(q);
	}
	data = PTR_TO_WORD(new_p);
	goto out;
	break;
      case 4: case 5:
	i = cell_type_without[cti][path[level].ix] - CELL_trie1234;
	assert(cell_size[i] == cs - 1);
	new_p = allocate(cs - 1, cell_type[i]);
	new_p[0] |= p[0] & FULL_PREFIX_MASK;
	for (j = 0, ix = 0; j < 4; j++) {
	  i = offset[cti][j];
	  if (i != 0 && j != path[level].ix)
	    new_p[++ix] = p[i];
	}
	data = PTR_TO_WORD(new_p);
	goto out;
	break;
      }
    }
    if (level == 0)
      /* We come here if we emptied the whole tree. */
      return NULL_PTR;
  }
out:

  /* Go upwards along the path inserting the new subtree on each
     level. */
  while (level > 0) {
    level--;
    p = path[level].node;
    ct = CELL_TYPE(p);
    cti = ct - CELL_trie1234;
    cs = cell_size[cti];
    /* Copy `p' and go upwards. */
    new_p = allocate(cs, ct);
    new_p[0] = p[0];
    new_p[1] = p[1];
    if (cs >= 4) {		/* Manually optimized decision tree on `cs'. */
      new_p[2] = p[2];
      new_p[3] = p[3];
      if (cs == 5)
	new_p[4] = p[4];
    } else if (cs == 3)
      new_p[2] = p[2];
    new_p[path[level].offset] = data;
    data = PTR_TO_WORD(new_p);
  }
  assert(triev2_make_assertions(new_p, orig_key_length));
  return new_p;
}



static void make_assertions(ptr_t p, word_t prefix, int key_length)
{
  int prefix_length;
  
  assert((key_length & 0x1) == 0);
  
  if (key_length > 0) {
    switch (CELL_TYPE(p)) {
    case CELL_trie1234:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x1,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[3]), prefix | 0x2,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[4]), prefix | 0x3,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie123: 
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x1,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[3]), prefix | 0x2,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie124:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x1,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[3]), prefix | 0x3,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie12:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x1,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie134:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x2,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[3]), prefix | 0x3,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie13:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x2,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie14:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x3,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie1:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie234:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix | 0x1,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x2,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[3]), prefix | 0x3,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie23:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix | 0x1,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x2,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie24:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix | 0x1,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x3,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie2:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix | 0x1,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie34:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix | 0x2,
		      key_length - (prefix_length + 2));
      make_assertions(WORD_TO_PTR(p[2]), prefix | 0x3,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie3:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix | 0x2,
		      key_length - (prefix_length + 2));
      break;
    case CELL_trie4:
      /* Handle a possible common prefix. */
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length > 0) {
	prefix <<= prefix_length;
	prefix |= p[0] & NODE_PREFIX_MASK;
      } else 
	assert((p[0] & FULL_PREFIX_MASK) == 0x0);
      prefix <<= 2;
      make_assertions(WORD_TO_PTR(p[1]), prefix | 0x3,
		      key_length - (prefix_length + 2));
      break;
    default:
      assert(0);
    }
#if 0
  } else {
    /* This can be used only in conjunction with `test_triev2.c'. */
    assert(CELL_TYPE(p) == CELL_word_vector);
#endif
  }
  return;
}

int triev2_make_assertions(ptr_t root, int key_length)
{
  if (root == NULL_PTR || first_generation_assertion_heaviness < 10)
    return 1;
  make_assertions(root, 0, key_length);
  return 1;
}
