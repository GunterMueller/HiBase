/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Sirkku-Liisa Paajanen <sirkku@cs.hut.fi>
 */

#include "includes.h"
#include "shades.h"
#include "trie.h"

static char *rev_id = "$Id: trie.c,v 1.117 1997/02/27 10:18:43 sirkku Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


/* This file implements 4-way prefixed (= interpolating) tries.  The
   keys are single `word_t's and data is a pointer to a cell of any
   type.

   The first word of each trie node contains the cell type tag and
   possibly the prefix and information of how long the prefix is.  The
   words 1, 2, 3, and 4 in the trie node are pointers (of course
   translated by `PTR_TO_WORD') to the subnodes or the data.

   The eight uppermost bits, i.e. bits 24 to 31, in the first word
   contain the cell type tag.  The next four bits, i.e. bits 20 to 23,
   tell how many 2-bits are prefixed.  The prefix bits are in bits 0
   to 19.

   The prefix length is always a multiple of two, therefore the range
   0 to 20 of the prefix length can be encoded unambiguously in the 4
   bits.  The standard phrase to decode the prefix length from a node
   pointed to by `p' is
     prefix_length = ((p[0]) >> 19) & 0x1E
   which is equivalent to but one instruction faster than
     prefix_length = ((p[0] >> 20) & 0xF) * 2
   Conversely the standard phrase of assigning the length bits is
     p[0] |= prefix_length << 19
   assuming the bits 20 to 23 are initially clear in `p[0]'.

   To decode the prefix stored in the node `p', we use the phrase
     node_prefix = p[0] & NODE_PREFIX_MASK
   and to assigning the prefix bits to clear bits is simply
     p[0] |= prefix;

   The key is kept shifted as far left as possible, so that the
   uppermost uncompared bit is in position 31.  Taking the next `n'
   bits for comparison is done by
     key >> (32 - n)
   The variable `key_length' denotes the number of bits in the key
   that have not yet been compared.  It is initialized to 32 and when
   it reaches 0 the data cell should be at hand.  `key_length' is
   always even.  Removing `n' uppermost bits of the key is done by
     key <<= n;
     key_length -= n; */


#define NODE_PREFIX_MASK  0xFFFFF

/* Find the smallest key in the given trie.  Assign the found key to
   `*key_p' and return pointer to the data assosiated to it. */
static ptr_t find_min(ptr_t trie_node, int key_length, word_t *key_p)
{
  ptr_t p = trie_node;
  word_t node_prefix;
  int prefix_length, i;
  
  assert(key_length > 0);

  do {
    
    if (p == NULL_PTR)
      return NULL_PTR;

    assert(CELL_TYPE(p) == CELL_quad_trie);

    /* Insert the possible prefix of the current node in the `*key_p'. */ 
    prefix_length = (p[0] >> 19) & 0x1E;
    node_prefix = p[0] & NODE_PREFIX_MASK;
    *key_p <<= prefix_length;
    *key_p |= node_prefix;

    /* Smallest key in the tree will be found as far left as possible. 
       Find the first son that is not NULL and store the index 
       in the `i'. */
    i = 1;
    while (p[i] == NULL_WORD) {
      i++;
      assert(i < 5);
    }

    /* Insert the index in the `*key_p'. */ 
    *key_p <<= 2;
    *key_p |= i - 1;

    /* Go deeper in the tree by one node. */
    p = WORD_TO_PTR(p[i]);
    key_length -= prefix_length;
    key_length -= 2;
    
  } while (key_length != 0);

  return p;
}


/* Return the pointer stored behind the given key, or NULL_PTR if not
   found. */
ptr_t trie_find(ptr_t trie_root, word_t key)
{
  ptr_t p = trie_root;
  word_t node_prefix, key_prefix;
  int prefix_length, key_length = 32;

  /* The loop below traverses through one trie node on each
     iteration. */
  do {
    /* Check whether the data is present. */
    if (p == NULL_PTR)
      return NULL_PTR;

    assert(CELL_TYPE(p) == CELL_quad_trie);
    assert((key_length & 0x1) == 0); /* `key_length' is always even. */

    /* Handle a possible common prefix. */
    prefix_length = (p[0] >> 19) & 0x1E;
    if (prefix_length != 0) {
      node_prefix = p[0] & NODE_PREFIX_MASK;
      key_prefix = key >> (32 - prefix_length);
      if (node_prefix != key_prefix)
	return NULL_PTR;
      /* `prefix_length' uppermost bits of the key have been compared,
         remove them. */
      key <<= prefix_length;
      key_length -= prefix_length;
    }

    /* Branch by the 2 uppermost unused bits of the key, 30 = 32 - 2. */
    p = WORD_TO_PTR(p[1 + (key >> 30)]);
    /* Remove the 2 bits from the key. */
    key <<= 2;
    key_length -= 2;
  } while (key_length != 0);

  assert(key == 0);

  return p;
}


/* Find a key that's equal to or greater than the given key. If operation 
   fails, return NULL_PTR, else store the found key in `*key_p' and return 
   the data assosiated to it. */  
ptr_t trie_find_at_least(ptr_t trie_root, word_t *key_p)
{
  ptr_t p = trie_root, data_p;
  word_t node_prefix, key_prefix, final_key = 0;
  int prefix_length, key_length = 32, level = 0, key_exists = 1, i;
  struct {
    ptr_t node;
    int ix;
  } path[16 + 1];

 
  /* The loop below traverses through one trie node on each
     iteration. */
  do {
    /* Check whether the data is present. */
    if (p == NULL_PTR)
      return NULL_PTR;

    assert(CELL_TYPE(p) == CELL_quad_trie);
    assert((key_length & 0x1) == 0); /* `key_length' is always even. */

    /* Handle a possible common prefix. */
    prefix_length = (p[0] >> 19) & 0x1E;
    if (prefix_length != 0) {
      node_prefix = p[0] & NODE_PREFIX_MASK;
      key_prefix = *key_p >> (32 - prefix_length);
      if (node_prefix == key_prefix) {
	i = 1 + ((*key_p << prefix_length) >> 30);
	if (p[i] == NULL_WORD) 
	  key_exists = 0;
        else {
	  /* `prefix_length' uppermost bits of the key have been compared,
	     remove them. */
	  *key_p <<= prefix_length;
	  key_length -= prefix_length;
	  /* Insert the prefix to the `final_key'. */
	  final_key <<= prefix_length;
	  final_key |= node_prefix;
	  /* Remove the 2 bits from the key. */
	  *key_p <<= 2;
	  key_length -= 2;
	  /* Insert the 2 bits in the `final_key'. */
	  final_key <<= 2;
	  final_key |= i - 1;
	  /* Go deeper in the tree. */
	  path[level].node = p;
	  path[level].ix = i;
	  level++;
	  p = WORD_TO_PTR(p[i]);
	}
      } else 
	key_exists = 0;
    } else {
      i = 1 + (*key_p >> 30);
      if (p[i] == NULL_WORD)
	key_exists = 0;
      else {
	/* Remove the 2 bits from the key. */
	*key_p <<= 2;
	key_length -= 2;
	/* Insert the 2 bits in the `final_key'. */
	final_key <<= 2;
	final_key |= i - 1;
	/* Go deeper in the tree. */
	path[level].node = p;
	path[level].ix = i;
	level++;
	p = WORD_TO_PTR(p[i]);
      }
    }
  } while ((key_length != 0) && (key_exists));

  if (key_length == 0) {
    /* Same key as the the given key is in the tree! Return it. */
    assert(*key_p == 0);
    *key_p = final_key;
    return p;
  }

  /* There isn't same key as the given key in the tree. So we must 
     search for greater keys. Such keys exist at the right side of 
     the path. */
 
  if ((prefix_length != 0) && (node_prefix != key_prefix)) {
    /* Check what the prefix of `p' tells about the keys in a subtree 
       starting from `p'. */ 
    if (node_prefix > key_prefix) {
      /* In a subtree exist only greater keys. 
	 Next step is to find minimum of the subtree. That minimun 
	 is the asked key. */

      data_p = find_min(p, key_length, &final_key);
      *key_p = final_key;
      return data_p;
    } else {
      /* In a subtree exist only smaller keys than the given key. We 
	 must go by one node upwards. */
      level --;
      if (level < 0)
	/* Search failed. All the keys in the tree are smaller than 
	   the given key. */
	return NULL_PTR;
      key_length += 2;
      final_key >>= 2;
      p = path[level].node;
      i = path[level].ix;
      /* Clear the possible prefix from the `final_key'. That's because 
	 we're not yet sure if this node is going to belong to the 
	 final path. */
      prefix_length = (p[0] >> 19) & 0x1E;
      key_length += prefix_length;
      final_key >>= prefix_length;
    }
  }
  /* Climb along the path upwards node by node and search a 
     first non-NULL index. */
  do {

    i++;
    while (i == 5) {
      /* Index has overflown. There was no pointers to childs in this 
	 node. Go upwards by one node (not possible if we are in the 
	 root i.e level must be greater than 0). */ /* XXX */
      level--;
      if  (level < 0) 
	/* Search failed. All the keys in the tree are smaller than 
	   the given key. */
	return NULL_PTR;
      i = path[level].ix + 1;
      p = path[level].node;
      /* Remove the index from the `final_key'. */
      key_length += 2;
      final_key >>= 2;
      /* Remove the possible prefix from the `final_key'. That's 
	 because we're not yet sure if this node is going to belong 
	 to the final path. */
      prefix_length = (p[0] >> 19) & 0x1E;
      key_length += prefix_length;
      final_key >>= prefix_length;
    } 
  } while (p[i] == NULL_WORD);

  /* There is a path to the key that's at least the given key. The path 
     to that key continues from `p[i]'. Insert the prefix and `i' to the 
     `final_key'. */
 
  prefix_length = (p[0] >> 19) & 0x1E;
  node_prefix = p[0] & NODE_PREFIX_MASK;
  final_key <<= prefix_length;
  final_key |= node_prefix;
  key_length -= prefix_length;

  final_key <<= 2;
  final_key |= i - 1;
  key_length -= 2;

  
  p = WORD_TO_PTR(p[i]);
  if (key_length > 0) {
    /* In a subtree exist the next greater keys than the given key. 
       Next step is to find minimum of the subtree. That minimun 
       is the asked key. */
    data_p = find_min(p, key_length, &final_key);
    *key_p = final_key;
    return data_p;
  } 
  *key_p = final_key;
  return p;
}


/* Insert (or change) the pointer stored behind the given key.  If `f'
   is NULL, then insert `data', otherwise insert the value returned by
   `f(old_data, data)', where `old_data' is the old value stored in
   the given key position.  Returns the new root. */
ptr_t trie_insert(ptr_t trie_root, 
			 word_t key, 
			 ptr_t (*f)(ptr_t, ptr_t),
			 ptr_t data)
{
  /* The algorithm constructs a new path starting from the root to the
     insertion point.

     `p' contains the pointer to the current node in the old tree.
     `new_p' contains the pointer to the new node.
     `new_pp' is the pointer to one of the slots in the parent node
     (that was previously pointed to by `new_p'), that will eventually
     be assigned the pointer to `new_p'.
     `new_root' is naturally the address of the newly constructed root
     of the tree.
     `new_old_p' refers to an occasionally used additional new node. */
  ptr_t p, new_p, new_pp, new_old_p, new_root, ap;
  word_t node_prefix, key_prefix, new_old_prefix;
  int i, prefix_length, new_old_prefix_length, key_length;

  ap = get_allocation_point();

  /* Initialize the iteration by constructing a new root node. */
  key_length = 32;
  p = trie_root;
  new_root = new_p = allocate(5, CELL_quad_trie);

  /* The goto into the loop allows a little easier bookkeeping of the
     root pointer. */
  goto insert_to_level;

  do {
    /* Allocate a new node deeper in the trie and chain it to be a
       child of the previously constructed node. */
    new_p = allocate(5, CELL_quad_trie);
    *new_pp = PTR_TO_WORD(new_p);
    
  insert_to_level:
    
    assert((key_length & 0x1) == 0); /* `key_length' is always even. */

    if (p == NULL_PTR) {
      /* There's no old path to copy. */
      new_p[1] = new_p[2] = new_p[3] = new_p[4] = NULL_WORD;
      /* Use as much as possible as a common prefix, but leave at
         least two bits in the key. */
      prefix_length = (key_length > 22) ? 20 : key_length - 2;
      if (prefix_length > 0) {
	new_p[0] |= (prefix_length << 19) | (key >> (32 - prefix_length));
	key_length -= prefix_length;
	key <<= prefix_length;
      }
      /* Branching by the correct slot in `new_p' is done after the
         lengthy else-part. */
    } else {
      prefix_length = (p[0] >> 19) & 0x1E;

      if (prefix_length == 0) {
	/* No prefix in `p'.  Make `new_p' a identical to the old `p'.
	   Later one of its slots is changed to refer to the next
	   `new_p' or the data.

	   Note that here `p[0]' is already initialized to contain
	   only the cell type tag, there no need to clear the prefix
	   in it. */
	new_p[1] = p[1];
	new_p[2] = p[2];
	new_p[3] = p[3];
	new_p[4] = p[4];
      } else {
	/* The old node in `p' is prefixed with `prefix_length' bits.
           Get the node's prefix and the corresponding number of bits
           from the key. */
	node_prefix = p[0] & NODE_PREFIX_MASK;
	key_prefix = key >> (32 - prefix_length);
	if (node_prefix == key_prefix) {
	  /* The prefixes agree, simply make `new_p' a copy of `p',
             this time including also `p[0]', since it contains the
             prefix. */
	  new_p[0] = p[0];
	  new_p[1] = p[1];
	  new_p[2] = p[2];
	  new_p[3] = p[3];
	  new_p[4] = p[4];
	  /* Drop the compared prefix's length of bits away from the
             key. */
	  key_length -= prefix_length;
	  key <<= prefix_length;
	} else {
	  /* This is the hard part.
	     Make `new_old_p' the equivalent of `p' except that
	     decrease the prefix length so that it can serve as a
	     subnode of the new intermediate node, which we create in
	     `new_p'. */
	  new_old_p = allocate(5, CELL_quad_trie);
	  new_old_prefix_length = 
	    /* First bit that differs: */ highest_bit(node_prefix ^ key_prefix)
	    /* Decrease if odd: */ & ~0x1;
	  /* Take the `new_old_prefix_length' lowest bits of
             `node_prefix' to the new old node, otherwise it is just
             like the old node `p'. */
	  new_old_prefix = node_prefix & ((1 << new_old_prefix_length) - 1);
	  new_old_p[0] |= (new_old_prefix_length << 19) | new_old_prefix;
	  new_old_p[1] = p[1];
	  new_old_p[2] = p[2];
	  new_old_p[3] = p[3];
	  new_old_p[4] = p[4];

	  /* Take the agreeing prefix part minus 2 to the prefix of
	     `new_p'. */
	  new_p[1] = new_p[2] = new_p[3] = new_p[4] = NULL_WORD;
	  prefix_length -= 2 + new_old_prefix_length;
	  if (prefix_length > 0) {
	    new_p[0] |= (prefix_length << 19) | (key >> (32 - prefix_length));
	    key_length -= prefix_length;
	    key <<= prefix_length;
	  }
	  /* Make `new_old_p' a child of `new_p'. */
	  new_p[1 + (0x3 & (node_prefix >> new_old_prefix_length))] =
	    PTR_TO_WORD(new_old_p);
	}
      }
    }

    /* Branch according to the next two uncompared bits in the key. */
    i = 1 + (key >> 30);
    p = WORD_TO_PTR(new_p[i]);
    new_pp = &new_p[i];
    key_length -= 2;
    key <<= 2;
  } while (key_length != 0);
  
  assert(key == 0);

  if (f != NULL)
    data = f(p, data);

  if (*new_pp == PTR_TO_WORD(data)) {
    /* The insertion is nilpotent: retract the work and return the
       original root. */
    restore_allocation_point(ap);
    return trie_root;
  } else {
    /* Put the data in the last slot we used.  Then return the new
       path's root.  */
    *new_pp = PTR_TO_WORD(data);
    return new_root;
  }
  /*NOTREACHED*/
}


/* Delete the pointer stored behind the given key.  Returns the new
   root if the deletion succeeded, the old root if the deletion
   failed.  */
ptr_t trie_delete(ptr_t trie_root, 
		  word_t key,
		  ptr_t (*f)(ptr_t, ptr_t),
		  ptr_t data)
{
  ptr_t p = trie_root, new_p, son, ap;
  word_t node_prefix, key_prefix, new_prefix;
  int i, j, number_of_sons, key_length = 32, level = 0;
  int prefix_length, son_prefix_length;
  struct {
    ptr_t node;
    int ix;
  } path[16 + 1];
 
  ap = get_allocation_point();

  /* The algorithm proceeds by allocating a new path from the root to
     the deletion point (done in the loop immediately below), and then
     by returning upwards along the same path removing empty nodes and
     compressing two prefixes wherever possible.  The path is kept in
     the array `path', which contains the pointer to the node on the
     given level and the index of the new child.

     Except for bookkeeping of `path', the loop below is almost
     identical to the one in `simple_trie_insert'.  Refer it for
     comments. */
  do { 

    if (p == NULL_PTR) {
      /* The key to be deleted doesn't exist; return the old root.
	 As an optimization, free the cells we already allocated. */
      restore_allocation_point(ap); 
      return trie_root;
    }
    assert(CELL_TYPE(p) == CELL_quad_trie);
 
    prefix_length = (p[0] >> 19) & 0x1E;
    if (prefix_length == 0) 
      new_p = allocate(5, CELL_quad_trie);
    else {
      node_prefix = p[0] & NODE_PREFIX_MASK;
      key_prefix = key >> (32 - prefix_length);
      if (node_prefix != key_prefix) {
	/* The key to be deleted doesn't exists. */
	restore_allocation_point(ap); 
	return trie_root;
      }
      new_p = allocate(5, CELL_quad_trie);
      new_p[0] = p[0];
      key_length -= prefix_length;
      key <<= prefix_length;
    }
    new_p[1] = p[1];
    new_p[2] = p[2];
    new_p[3] = p[3];
    new_p[4] = p[4];
    
    /* Append the new node to the vector `path'. */
    path[level].node = new_p;
    /* Unless `new_p' is the new root, chain `new_p' to be a child of
       the previously constructed node. */
    if (level > 0)
      /* `path[level - 1].node' is the previously constructed node and
	 `path[level - 1].ix' the index to get to the new node. */
      path[level - 1].node[path[level - 1].ix] = PTR_TO_WORD(new_p);
    
    /* Go deeper in the tree. */
    i = 1 + (key >> 30);
    p = WORD_TO_PTR(new_p[i]);
    path[level].ix = i;
    key_length -= 2;
    key <<= 2;
    level++;
  } while (key_length != 0);

  assert(key_length == 0);
  assert(key == 0);

  /* Now `p' should point to the data behind the key. */

  if (f != NULL) {
    /* Update data behind the key. */
    data = f(p, data);
    assert(data != NULL_PTR);
    level--;
    p = path[level].node;
    p[path[level].ix] = PTR_TO_WORD(data);
    return path[0].node;
  } 
  /* (Otherwise) delete the key. */
  if (p == NULL_PTR) {
    /* The key to be deleted doesn't exists. */
    restore_allocation_point(ap); 
    return trie_root;
  }
 
  /*  Go up along the path to the previous node and delete the data. */
  level--;
  p = path[level].node;
  p[path[level].ix] = NULL_WORD;
 
  number_of_sons = 0;
  for (i = 1; i < 5; i++)
    if (p[i] != NULL_WORD)
      number_of_sons++;
  
  if (number_of_sons == 0) {
    /* No sons; `p' is useless and can be deleted. */
    if (level > 0)
      path[level - 1].node[path[level - 1].ix] = NULL_WORD;
    else 
      /* The tree has become empty. */
      return NULL_PTR;
  } else 
    return path[0].node;
  
  level--;
   /* Now traverse the path back upwards and check if there exists
     useless nodes, i.e. nodes from where you can't reach any key, or
     if there are singleton nodes with excessively small prefixes. */
  while (level >= 0) {
    p = path[level].node;
    /* Count the number of sons of the node `p'.  Retain in `i' the
       index of (one of) the sons. */
    number_of_sons = 0;
    for (j = 1; j < 5; j++)
      if (p[j] != NULL_WORD) {
	number_of_sons++;
	i = j;
      }   
    
    if (number_of_sons == 0) {
      /* No sons; `p' is useless and can be deleted. */
      if (level > 0)
        path[level - 1].node[path[level - 1].ix] = NULL_WORD;
      else 
	/* The tree has become empty. */
	return NULL_PTR;
    } else if (number_of_sons == 1) {
      /* Only one son, let's try to combine that and the current node `p'.  
	 Combining will succeed only if the length of the new prefix 
	 will be less than 20. */
      prefix_length = (p[0] >> 19) & 0x1E;
      son = WORD_TO_PTR(p[i]);
      son_prefix_length = (son[0] >> 19) & 0x1E;
      /* Count the length of the new prefix and check if less or equal
         to 18.  Here two bits would be used by the index in the
         son. */
      if (prefix_length + son_prefix_length <= 18) {
	/* Combining is possible. Compute the new prefix.  First the
           current node's prefix, then the two bits that index the son
           in the current node, then the prefix of the son.*/
	new_prefix = p[0] & NODE_PREFIX_MASK;
	new_prefix <<= 2;
	new_prefix |= i - 1;
	new_prefix <<= son_prefix_length;
	new_prefix |= son[0] & NODE_PREFIX_MASK;
	/* Clear the `p[0]' from possible previous prefixes and
	   lengths.  Save the type tag. */
	p[0] &= 0xFF << 24;
	/* Store the length of the new prefix to `p[0]'. */
	p[0] |= (prefix_length + 2 + son_prefix_length) << 19;
	/* Store the new prefix to `p[0]'. */
	p[0] |= new_prefix;
	/* Copy the rest of the son to `p'. */
	p[1] = son[1];
	p[2] = son[2];
	p[3] = son[3];
	p[4] = son[4];
      }
    } else
      /* No more changes are possible during this deletion; return the
	 new root. */
      return path[0].node;

    /* Go upwards to the new path by one level. */
    level--;
  } 

  return path[0].node;
}

/* Insert (or change) the given set of pointers . Returns the new root. */
ptr_t trie_insert_set(ptr_t trie_root, 
		      word_t key[], 
		      ptr_t data[], 
		      int number_of_keys)
{
  ptr_t p = trie_root, new_p, new_old_p, ap;
  word_t current_key, node_prefix, key_prefix, new_old_prefix;
  int i, ix, prefix_length, new_old_prefix_length, key_length = 32;
  int level = 0, number_of_insertions = 0, different_bits;
  int allocate_new_node = 1;
  struct {
    ptr_t node;
    int ix;
  } path[16 + 1];

  
  /* Check that the keys are in decreasing order in the table. */
  for (i = 1; i < number_of_keys; i++)
    assert(key[i - 1] < key[i]);

  ap = get_allocation_point();   

  i = 0;
  current_key = key[0];
  while (i < number_of_keys) {
    while (key_length != 0) {

      if (allocate_new_node) {
	/* Allocate a new node deeper in the trie and chain it to be a
	   child of the previously constructed node. */
	new_p = allocate(5, CELL_quad_trie); 
	/* Append the new node to the vector `path'. */
	path[level].node = new_p;
	/* Unless `new_p' is the new root, chain `new_p' to be a child of
	   the previously constructed node. */
	if (level > 0)
	  /* `path[level - 1].node' is the previously constructed node and
	     `path[level - 1].ix' the index to get to the new node. */
	  path[level - 1].node[path[level - 1].ix] = PTR_TO_WORD(new_p);
      } else {
	new_p = p;
	allocate_new_node = 1;
      }
      assert((key_length & 0x1) == 0); /* `key_length' is always even. */
      
      if (p == NULL_PTR) {
	/* There's no old path to copy. */
	new_p[1] = new_p[2] = new_p[3] = new_p[4] = NULL_WORD;
	/* Use as much as possible as a common prefix, but leave at
	   least two bits in the key. */

	prefix_length = (key_length > 22) ? 20 : key_length - 2;
	if (prefix_length > 0) {
	  new_p[0] |= (prefix_length << 19) 
	    | (current_key >> (32 - prefix_length));
	  key_length -= prefix_length;
	  current_key <<= prefix_length;
	}
	/* Branching by the correct slot in `new_p' is done after the
	   lengthy else-part. */
      } else {
	assert(CELL_TYPE(p) == CELL_quad_trie);
	prefix_length = (p[0] >> 19) & 0x1E;
	if (prefix_length == 0) {
	  /* No prefix in `p'.  Make `new_p' a identical to the old `p'.
	     Later one of its slots is changed to refer to the next
	     `new_p' or the data.
	     
	     Note that here `p[0]' is already initialized to contain
	     only the cell type tag, there no need to clear the prefix
	     in it. */
	  new_p[1] = p[1];
	  new_p[2] = p[2];
	  new_p[3] = p[3];
	  new_p[4] = p[4];
	} else {
	  /* The old node in `p' is prefixed with `prefix_length' bits.
	     Get the node's prefix and the corresponding number of bits
	     from the key. */
	  node_prefix = p[0] & NODE_PREFIX_MASK;
	  key_prefix = current_key >> (32 - prefix_length);
	  if (node_prefix == key_prefix) {
	    /* The prefixes agree, simply make `new_p' a copy of `p',
	       this time including also `p[0]', since it contains the
	       prefix. */
	    new_p[0] = p[0];
	    new_p[1] = p[1];
	    new_p[2] = p[2];
	    new_p[3] = p[3];
	    new_p[4] = p[4];
	    /* Drop the compared prefix's length of bits away from the
	       key. */
	    key_length -= prefix_length;
	    current_key <<= prefix_length;
	  } else {
	    /* This is the hard part.
	       Make `new_old_p' the equivalent of `p' except that
	       decrease the prefix length so that it can serve as a
	       subnode of the new intermediate node, which we create in
	       `new_p'. */
	    new_old_p = allocate(5, CELL_quad_trie);
	    new_old_prefix_length = 
	      /* First bit that differs: */ 
	      highest_bit(node_prefix ^ key_prefix)
	      /* Decrease if odd: */ & ~0x1;
	    /* Take the `new_old_prefix_length' lowest bits of
	       `node_prefix' to the new old node, otherwise it is just
	       like the old node `p'. */
	    new_old_prefix = node_prefix & ((1 << new_old_prefix_length) - 1);
	    new_old_p[0] |= (new_old_prefix_length << 19) | new_old_prefix;
	    new_old_p[1] = p[1];
	    new_old_p[2] = p[2];
	    new_old_p[3] = p[3];
	    new_old_p[4] = p[4];

	    /* Take the agreeing prefix part minus 2 to the prefix of
	       `new_p'. */
	    new_p[1] = new_p[2] = new_p[3] = new_p[4] = NULL_WORD;
	    prefix_length -= 2 + new_old_prefix_length;
	    new_p[0] >>= 24;
	    new_p[0] <<= 24;
	    if (prefix_length > 0) {
	      new_p[0] |= (prefix_length << 19) 
		| (current_key >> (32 - prefix_length));
	      key_length -= prefix_length;
	      current_key <<= prefix_length;
	    }
	    /* Make `new_old_p' a child of `new_p'. */
	    new_p[1 + (0x3 & (node_prefix >> new_old_prefix_length))] =
	      PTR_TO_WORD(new_old_p);
	  }
	}
      }
      
      /* Branch according to the next two uncompared bits in the key. */
      ix = 1 + (current_key >> 30);
      p = WORD_TO_PTR(new_p[ix]);
      path[level].ix = ix;
      key_length -= 2;
      current_key <<= 2;
      level++;
    } 
    
    assert(current_key == 0);
    /* Now `p' should point to the data behind the key (in case that change 
       the the key) or be NULL_PTR if no data is assosiated to that key 
       before. Go up along the path to the previous node and insert the 
       new data. */
    level--;
    p = path[level].node;
    key_length += 2; 
    prefix_length = (p[0] >> 19) & 0x1E;
    key_length += prefix_length;
    if (p[path[level].ix] != PTR_TO_WORD(data[i])) {
      number_of_insertions++;
      p[path[level].ix] = PTR_TO_WORD(data[i]);
    }
    if (i == number_of_keys - 1) {
      /* The last key has just been inserted. */
      if (number_of_insertions == 0) {
	/* The insertion is nilpotent: retract the work and return the
	   original root. */
	restore_allocation_point(ap);
	return trie_root;
      } else
	return path[0].node;
    }
    /* The paths of the new and previos key separate in a node, here it is 
       called cross node, where, when traversing down the tree from the 
       root, the total number of bits of the previous/new key that have been 
       comparised is 32 - `different_bits'. */
    different_bits = highest_bit(key[i] ^ key[i + 1]) + 1;
    /* Climb along the path upwards node by node and search the cross 
       node. */
    while (key_length < different_bits) {
      level--;
      p = path[level].node;
      key_length += 2;
      prefix_length = (p[0] >> 19) & 0x1E;
      key_length += prefix_length;  
    }
    i++;
    current_key = key[i] << (32 - key_length);
    prefix_length = (p[0] >> 19) & 0x1E;
    if (prefix_length != 0) {
      node_prefix = p[0] & NODE_PREFIX_MASK;
      key_prefix = current_key >> (32 - prefix_length);
      if (node_prefix == key_prefix) {
	current_key <<= prefix_length;
	key_length -= prefix_length;
	/* Go deeper in the tree. */
	ix = 1 + (current_key >> 30);
	path[level].ix = ix;
	p = WORD_TO_PTR(p[ix]);
	level++;
	current_key <<= 2;
	key_length -= 2;
      } else
	allocate_new_node = 0;
    } else {
      /* Go deeper in the tree. */
      ix = 1 + (current_key >> 30);
      path[level].ix = ix;
      p = WORD_TO_PTR(p[ix]);
      level++;
      current_key <<= 2;
      key_length -= 2;
    }
  }
}

/* Delete all the keys in the given key vector `key[]'.  Return the new
   root if the deletion succeeded and the old root if the deletion
   failed.  */
ptr_t trie_delete_set(ptr_t trie_root, word_t key[], int number_of_keys)
{
  ptr_t p = trie_root, ap, son, new_p;
  word_t node_prefix, key_prefix, current_key, new_prefix;
  int prefix_length, key_length = 32, level = 0, different_bits; 
  int key_exists = 1, i, j, ix, deleted_keys = 0;
  int number_of_sons, son_prefix_length;
  struct {
    ptr_t node;
    int ix;
  } path[16 + 1];

  if (p == NULL_PTR)
    return NULL_PTR;

  /* Check that the keys are in decreasing order in the table. */
  for (i = 1; i < number_of_keys; i++)
    assert(key[i - 1] < key[i]);

  ap = get_allocation_point();

  i = 0;
  current_key = key[0];
  /* The while-loop below deletes one key at a time. */
  while (i < number_of_keys) {
    key_exists = 1;
    /* Traverse down the tree and find the data behind the key. */
    while ((key_length > 0) && (key_exists)) { 
      assert(CELL_TYPE(p) == CELL_quad_trie);
      /*      assert(cell_check_rec(p));*/
      new_p = allocate(5, CELL_quad_trie);
      new_p[1] = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      new_p[4] = p[4];
      /* Append the new node to the vector `path'. */
      path[level].node = new_p;
      /* Unless `new_p' is the new root, chain `new_p' to be a child of
	 the previously constructed node. */
      if (level > 0)
	/* `path[level - 1].node' is the previously constructed node and
	   `path[level - 1].ix' the index to get to the new node. */
	path[level - 1].node[path[level - 1].ix] = PTR_TO_WORD(new_p);
      
      prefix_length = (p[0] >> 19) & 0x1E;
      if (prefix_length == 0) {
	/* Go deeper in the tree. */
	ix = 1 + (current_key >> 30);
	if (new_p[ix] == NULL_WORD)
	  key_exists = 0;
	p = WORD_TO_PTR(new_p[ix]);
	path[level].ix = ix;
	key_length -= 2;
	current_key <<= 2;
	level++;
      } else {
	node_prefix = p[0] & NODE_PREFIX_MASK;
	key_prefix = current_key >> (32 - prefix_length);
	new_p[0] = p[0];
	if (node_prefix == key_prefix) {
	  key_length -= prefix_length;
	  current_key <<= prefix_length;
	  /* Go deeper in the tree. */
	  ix = 1 + (current_key >> 30);
	  if (new_p[ix] == NULL_WORD)
	    key_exists = 0;
	  p = WORD_TO_PTR(new_p[ix]);
	  path[level].ix = ix;
	  key_length -= 2;
	  current_key <<= 2;
	  level++;
   	} else 
	  key_exists = 0;
      }
    } 
    /* Now `p' should point to the data behind the key if such exists. Next 
       step is to delete the data and traverse up along the path to the
       node where the path of the current key `key[i]' and the path of the 
       next key `key[i + 1]' crosses. That's an optimization; all the 
       common nodes of the paths of the given keys will be allocated only 
       once. 
       If the next key doesn't exist `key_exists' gets the value 
       1 and the loop trys the next key (index i + 2) until a valid node 
       i.e. node that exists or until no more keys to delete are left. */
    do {
      key_exists = 1;
      if (i + 1 == number_of_keys) {
	/* The current key is the last key in the given key vector. 
	 After deleting this key traverse up along the path to the new root 
	 and clean up the tree. */
	if (key_length == 0) {
	  assert(current_key == 0);	
	  if (p == NULL_PTR) {
	    if (!deleted_keys) {
	      /* None of the keys existed in the tree and so no changes 
		 has been made into the tree. */
	      printf("\n\aNone of the keys existed");
	      restore_allocation_point(ap);
	      return trie_root; 
	    }
	  } else 
      	    /* The current key existed in the tree. Delete it. */
	    path[level - 1].node[path[level - 1].ix] = NULL_WORD;
	  /* Climb up by one node and... */
	  level--; 
	  p = path[level].node; 
	  /* ...check if it is useless. */
	  number_of_sons = 0;
	  for (j = 1; j < 5; j++)
	    if (p[j] != NULL_WORD) 
	      number_of_sons++;
	  if (number_of_sons == 0) 
	      /* Delete `p'. */
	      path[level - 1].node[path[level - 1].ix] = NULL_WORD;
	  level--;
	  p = path[level].node; 
	} else {
	  /* The current key doesn't exist in the tree. */
	  if (!deleted_keys) {
	    /* None of the keys existed in the tree and so no changes 
	       has been made into the tree. */
	    restore_allocation_point(ap);
	    return trie_root; 
	  }
	  if (p == NULL_PTR) {
	    level--; 
	    p = path[level].node; 
	  } else { 
	    if ((key_length - prefix_length - 2) == 0) {
	      /* `p' points to a node that has pointers to datas. Check if 
		 the node is useless i.e. all pointers are NULL-pointers. */
	      number_of_sons = 0;
	      for (j = 1; j < 5; j++)
		if (p[j] != NULL_WORD) 
		  number_of_sons++;
	      if (number_of_sons == 0) {
		if (level > 0)
		  /* Delete `p'. */
		  path[level - 1].node[path[level - 1].ix] = NULL_WORD;
		else 
		  /* The tree has become empty. */
		  return NULL_PTR;
	      }
	      level--;
	      p = path[level].node; 
	    }
	  }
	}
	/* Traverse along the path to the new root and clean up the 
	   tree. Return the new root or NULL_PTR if the tree has become 
	   empty. The loop below is same as the second loop in 
	   `trie_delete'. So all comments from below are removed. */
	while (level >= 0) {
	  assert(CELL_TYPE(p) == CELL_quad_trie);
	  number_of_sons = 0;
	  for (j = 1; j < 5; j++)
	    if (p[j] != NULL_WORD) {
	      number_of_sons++;
	      ix = j;
	    }
	  if (number_of_sons == 0) {
	    if (level > 0)
	      path[level - 1].node[path[level - 1].ix] = NULL_WORD;
	    else 
	      return NULL_PTR;
	  } else if (number_of_sons == 1) {
	    prefix_length = (p[0] >> 19) & 0x1E;
	    assert(prefix_length <= 20);
	    assert(prefix_length >= 0);
	    son = WORD_TO_PTR(p[ix]);
	    son_prefix_length = (son[0] >> 19) & 0x1E;
	    if (prefix_length + son_prefix_length <= 18) {
	      new_prefix = p[0] & NODE_PREFIX_MASK;
	      new_prefix <<= 2;
	      new_prefix |= ix - 1;
	      new_prefix <<= son_prefix_length;
	      new_prefix |= son[0] & NODE_PREFIX_MASK;
	      p[0] &= 0xFF << 24;
	      p[0] |= (prefix_length + 2 + son_prefix_length) << 19;
	      p[0] |= new_prefix;
	      p[1] = son[1];
	      p[2] = son[2];
	      p[3] = son[3];
	      p[4] = son[4];
	    }
	  } else 
	    return path[0].node;
	  level--;
	  if (level >= 0)
	    p = path[level].node;
	}
	return path[0].node;
      } else {
	/* In the key vector there is at least one more key to 
	   delete after the current key. Count the number of bits that 
	   are different in the current key and the next key. */
	different_bits = highest_bit(key[i] ^ key[i + 1]) + 1;
	if (key_length == 0) {
	  assert(current_key == 0);
	  level--;
	  p = path[level].node;
	  key_length += 2; 
	  prefix_length = (p[0] >> 19) & 0x1E; 
	  key_length += prefix_length;
	  if (p[path[level].ix] != NULL_WORD) {
	    /* The key exists in the tree. Delete it. */
	    deleted_keys = 1;
	    p[path[level].ix] = NULL_WORD;
	    /* Check if `p' is useless. */
	    number_of_sons = 0;
	    for (j = 1; j < 5; j++)
	      if (p[j] != NULL_WORD) 
		number_of_sons++;
	    if (number_of_sons == 0) {
	      /* Delete `p'. */
	      path[level - 1].node[path[level - 1].ix] = NULL_WORD;
	      level--; 
	      p = path[level].node; 
	      key_length += 2; 
	      prefix_length = (p[0] >> 19) & 0x1E; 
	      key_length += prefix_length;        
	    } else {
	      if (key_length < different_bits) {
		level --;
		p = path[level].node;
		key_length += 2;
		prefix_length = (p[0] >> 19) & 0x1E;
		key_length += prefix_length;
	      }
	    }
	  } else {
	    if (key_length < different_bits) {
	      level --;
	      p = path[level].node;
	      key_length += 2;
	      prefix_length = (p[0] >> 19) & 0x1E;
	      key_length += prefix_length;
	    }
	  }
	} else {
	  if (p == NULL_PTR) {
	    level--;
	    p = path[level].node;
	    ix = path[level].ix;
	    key_length += 2;
	    prefix_length = (p[0] >> 19) & 0x1E;
	    key_length += prefix_length;
	  } else {
	    /*	    assert(cell_check_rec(p));*/
	    if (node_prefix < key_prefix) {
	      /* In a subtree exist only smaller keys than the given key. We 
		 must go by one node upwards. */
	      level--;
	      if (level < 0) {
		/* Search failed. All the keys in the tree are smaller than 
		   `key[i]' and so also smaller than the following keys in 
		   the given key vector. */
		if (!deleted_keys) {
		  /* None of the keys existed in the tree and so no changes 
		     has been made into the tree. */
		  printf("\a");
		  restore_allocation_point(ap);
		  return trie_root;
		} else 
		  return path[0].node;
	      }
	      p = path[level].node;
	      ix = path[level].ix;
	      key_length += 2;
	      prefix_length = (p[0] >> 19) & 0x1E;
	      key_length += prefix_length;
	    } else {
	      if ((key_length - prefix_length - 2) == 0) {
		if (key_length < different_bits) {
		  level --;
		  p = path[level].node;
		  key_length += 2;
		  prefix_length = (p[0] >> 19) & 0x1E;
		  key_length += prefix_length;
		}
	      }
	    }
	  }
	}
	/* Climb along the path upwards node by node and search the cross 
	   node. */
	while (key_length < different_bits) {
	  assert(CELL_TYPE(p) == CELL_quad_trie);
	  number_of_sons = 0;
	  for (j = 1; j < 5; j++)
	    if (p[j] != NULL_WORD) {
	      number_of_sons++;
	      ix = j;
	    }
	  if (number_of_sons == 0) {
	    if (level > 0)
	      path[level - 1].node[path[level - 1].ix] = NULL_WORD;
	    else 
	      return NULL_PTR;
	  } else if (number_of_sons == 1) {
	    prefix_length = (p[0] >> 19) & 0x1E;
	    son = WORD_TO_PTR(p[ix]);
	    son_prefix_length = (son[0] >> 19) & 0x1E;
	    if (prefix_length + son_prefix_length <= 18) {
	      new_prefix = p[0] & NODE_PREFIX_MASK;
	      new_prefix <<= 2;
	      new_prefix |= ix - 1;
	      new_prefix <<= son_prefix_length;
	      new_prefix |= son[0] & NODE_PREFIX_MASK;
	      p[0] &= 0xFF << 24;
	      p[0] |= (prefix_length + 2 + son_prefix_length) << 19;
	      p[0] |= new_prefix;
	      p[1] = son[1];
	      p[2] = son[2];
	      p[3] = son[3];
	      p[4] = son[4];
	    }
	  }
	  level--;
	  p = path[level].node;
	  key_length += 2;
	  prefix_length = (p[0] >> 19) & 0x1E;
	  key_length += prefix_length;  
	}
	i++;
	current_key = key[i] << (32 - key_length);
	prefix_length = (p[0] >> 19) & 0x1E;
	assert(prefix_length <= 20);
	assert(prefix_length >= 0);
	if (prefix_length != 0) {
	  node_prefix = p[0] & NODE_PREFIX_MASK;
	  key_prefix = current_key >> (32 - prefix_length);
	  if (node_prefix == key_prefix) {
	    current_key <<= prefix_length;
	    key_length -= prefix_length;
	    /* Go deeper in the tree. */
	    ix = 1 + (current_key >> 30);
	    path[level].ix = ix;
	    p = WORD_TO_PTR(p[ix]);
	    if (p == NULL_PTR)
	      /* The key doesn't exist in the tree. */
	      key_exists = 0;
	    level++;
	    current_key <<= 2;
	    key_length -= 2;
	  } else
	    /* The key doesn't exist in the tree. */
	    key_exists = 0;
	} else {
	  /* Go deeper in the tree. */
	  ix = 1 + (current_key >> 30);
	  path[level].ix = ix;
	  p = WORD_TO_PTR(p[ix]);
	  if (p == NULL_PTR)
	    /* The key doesn't exist in the tree. */
	    key_exists = 0;
	  level++;
	  current_key <<= 2;
	  key_length -= 2;
	}
      }
    } while (!key_exists);
  }
}

