#include "dh.h"

/* Find the smallest key in the given trie.  Assign the found key to
   `*key_p' and return pointer to the data assosiated to it. */
ptr_t dh_find_min(ptr_t root, word_t *key_p)
{
  ptr_t p = root;
  int i, cell_size;

  if (p == NULL_PTR)
    return NULL_PTR;
  
  while (1) {
    
    if (CELL_TYPE(p) == CELL_leaf) {
      
      /* Find the smallest item in `p'. */     
      *key_p = p[1];
      cell_size = ((p[0] & 0xFFFFFF) << 1) + 1;
      for (i = 3; i < cell_size; i += 2)
	if (p[i] < *key_p)
	  *key_p = p[i];
      return WORD_TO_PTR(p[4]);
    } 
    assert(CELL_TYPE(p) == CELL_internal);

    /* Always try first go to left. */
    i = 1;
    while (p[i] == NULL_WORD)
      i++;
    assert(i <= INTERNAL_NODE_SIZE);   
    p = WORD_TO_PTR(p[i]);
  } 
}

/* Return the pointer stored behind the given key, or NULL_PTR if not
   found. */
ptr_t dh_find(ptr_t root, word_t key)
{
  ptr_t p = root;
  int i, cell_size, bits_compared = 0, offset;

  offset = DO_ILOG2(INTERNAL_NODE_SIZE) - 1;  

  /* The loop below traverses through one node on each
     iteration. */
  while (1) {
    /* Check whether the data is present. */
    if (p == NULL_PTR)
      return NULL_PTR;

    if (CELL_TYPE(p) == CELL_leaf) {
      
      cell_size = ((p[0] & 0xFFFFFF) << 1) + 1;
      for (i = 1; i < cell_size; i += 2)
	if (p[i] == key) 
	  return WORD_TO_PTR(p[i + 1]);
      return NULL_PTR;
    } 
    assert(CELL_TYPE(p) == CELL_internal);

    p = WORD_TO_PTR(p[((key << bits_compared) >> (32 - offset)) + 1]);
    bits_compared += offset;
  }
}


ptr_t dh_insert(ptr_t root, word_t key, ptr_t (*f)(ptr_t, ptr_t), ptr_t data)
{
  word_t new_root = NULL_WORD;
  ptr_t ap, p = root, new_pp, new_p, new_leaf;
  ptr_t pp = &new_root;
  int i, j, ix, bits_compared = 0, number_of_keys, cell_size;
  int new_overflow, offset, items[INTERNAL_NODE_SIZE];
  
  offset = DO_ILOG2(INTERNAL_NODE_SIZE) - 1;
  ap = get_allocation_point();

  for (i = 0; i < INTERNAL_NODE_SIZE; i++)
    items[i] = 0;

  /* The loop below traverses through one node on each
     iteration. */
  while (1) {

    if (p == NULL_PTR) {
      if (f != NULL)
	data = f(NULL_PTR /* == `p' */, data);

      p = allocate(3, CELL_leaf);
      p[0] |= 0x1;
      p[1] = key;
      p[2] = PTR_TO_WORD(data);
      *pp = PTR_TO_WORD(p);
      return WORD_TO_PTR(new_root);
    }

    if (CELL_TYPE(p) == CELL_internal) {
      new_p = allocate(INTERNAL_NODE_SIZE + 1, CELL_internal);
      for (i = 1; i <= INTERNAL_NODE_SIZE; i++)
	new_p[i] = p[i];
      *pp = PTR_TO_WORD(new_p);
      i = ((key << bits_compared) >> (32 - offset)) + 1;
      pp = &(new_p[i]);
      p = WORD_TO_PTR(new_p[i]);
      bits_compared += offset;
    } else if (CELL_TYPE(p) == CELL_leaf) {
      
      number_of_keys = p[0] & 0xFFFFFF;
      cell_size = (number_of_keys << 1) + 1;
      
      for (i = 1; i < cell_size; i += 2) {
	if (p[i] == key) { 
	  if (f != NULL)
	    data = f(WORD_TO_PTR(p[i + 1]), data);
	  if (p[i + 1] == PTR_TO_WORD(data)) {	
	    /* The insertion is nilpotent: retract the work and return the
	       original root. */
	    restore_allocation_point(ap);
	    return root;
	  } else {
	    /* Update */
	    new_p = allocate(cell_size, CELL_leaf);
	    new_p[0] = p[0];
	    for (j = 1; j < cell_size; j += 2) {
	      new_p[j] = p[j];
	      new_p[j + 1] = p[j + 1];
	    }
	    new_p[i + 1] = PTR_TO_WORD(data);
	    *pp = PTR_TO_WORD(new_p);
	    return WORD_TO_PTR(new_root);
	  }
	}
      }
      
      /* Data with key `key' doesn't exist in tree. */
      if (number_of_keys < MAX_KEYS_IN_LEAF) {
	/* There's room for key in leaf `p'. */
	new_p = allocate(cell_size + 2, CELL_leaf);
	new_p[0] |= number_of_keys + 1;
        for (i = 1; i < cell_size; i += 2) {
	  new_p[i] = p[i];
	  new_p[i + 1] = p[i + 1];
	}
	new_p[i] = key;
	new_p[i + 1] = PTR_TO_WORD(data);
		
	*pp = PTR_TO_WORD(new_p);
	return WORD_TO_PTR(new_root);
      }

      assert(cell_size == (MAX_KEYS_IN_LEAF << 1) + 1);
      /* Overflow. There's room for `MAX_KEYS_IN_LEAF' items in leaf
	 cell atmost. Leaf must be replaced by an internal node, because more
	 bits of hash value must be taken in use. */
      /* Create new internal node. */
      new_pp = allocate(INTERNAL_NODE_SIZE + 1, CELL_internal);
      *pp = PTR_TO_WORD(new_pp);
      /* In some cases new internal node is not enough to distinguish
	 keys.  Then it is necessary add more internal nodes.
	 `new_overflow' is 1 in that case. If it is 0, just created
	 internal node `new_pp' is enough to distinguish keys. Let's
	 assume it is 1. */
      new_overflow = 1;
      if (((key << bits_compared) >> (32 - offset)) 
	  != ((p[1] << bits_compared) >> (32 - offset)))
	new_overflow = 0;
      else 
	for (i = 3; i < cell_size; i += 2)
	  if (((p[i - 2] << bits_compared) >> (32 - offset)) 
	      != ((p[i] << bits_compared) >> (32 - offset)))
	    new_overflow = 0;
      if (new_overflow) {
	/* All items in overflow cell branch to same cell again. Add
           new internal node. */
/*fprintf(stderr, "NEW OVERFLOW");*/
	for (i = 1; i <= INTERNAL_NODE_SIZE; i++) 
	  new_pp[i] = NULL_WORD;
	i = ((key << bits_compared) >> (32 - offset)) + 1;
	new_pp[i] = PTR_TO_WORD(p);
	pp = &(new_pp[i]);
	bits_compared += offset;
      } else {
	/* `new_pp' is enough to distinguish keys in `p' and key
	 `key'.  Next step is to find which keys branch to which
	 subtrees of `new_pp'. */
/*fprintf(stderr, "\nITEMS++");*/
	if (f != NULL)
	  data = f(NULL_PTR, data);
	
	items[(key << bits_compared) >> (32 - offset)]++;
	for (i = 1; i < cell_size; i += 2)
	  items[(p[i] << bits_compared) >> (32 - offset)]++;
	
	for (i = 0; i < INTERNAL_NODE_SIZE; i++) {
	  if (items[i] == 0)
	    new_pp[i + 1] = NULL_WORD;
	  else {
	    new_leaf = allocate((items[i] << 1) + 1, CELL_leaf);
	    new_leaf[0] |= items[i];
	    ix = 1;
	    if (((key << bits_compared) >> (32 - offset)) == i) {
	      new_leaf[1] = key;
	      new_leaf[2] = PTR_TO_WORD(data);
	      ix = 3;
	    }
	    for (j = 1; j < cell_size; j += 2)
	      if (((p[j] << bits_compared) >> (32 - offset)) == i) {
		new_leaf[ix] = p[j];
		new_leaf[ix + 1] = p[j + 1];
		ix += 2;
	      }
	    new_pp[i + 1] = PTR_TO_WORD(new_leaf);    
	  }
	}
	return WORD_TO_PTR(new_root);
      }
    }
  }
  /* Control doesn't reach here. */
} 

ptr_t dh_delete(ptr_t root, word_t key)
{
  return NULL_PTR;
}


void dh_make_assertions(ptr_t p, int bits_compared, word_t prefix)
{
  int i, cell_size, offset;

  offset = DO_ILOG2(INTERNAL_NODE_SIZE) - 1;
  assert(bits_compared <= 32);

  if (p == NULL_PTR)
    return;

  if (CELL_TYPE(p) == CELL_internal) {
    prefix <<= offset;
    bits_compared += offset;
    for (i = 1; i <= INTERNAL_NODE_SIZE; i++) {
      assert((WORD_TO_PTR(p[i]) == NULL_PTR) 
	     || ((CELL_TYPE(WORD_TO_PTR(p[i])) == CELL_internal) 
	     || (CELL_TYPE(WORD_TO_PTR(p[i])) == CELL_leaf)));
      dh_make_assertions(WORD_TO_PTR(p[i]), bits_compared, prefix | (i - 1));
    }
  } else {
    assert(CELL_TYPE(p) == CELL_leaf);
    cell_size = ((p[0] & 0xFFFFFF) << 1) + 1;
    for (i = 1; i < cell_size; i += 2)
    if (bits_compared > 0)
      assert((p[i] >> (32 - bits_compared)) == prefix);
  }
  return;
}


