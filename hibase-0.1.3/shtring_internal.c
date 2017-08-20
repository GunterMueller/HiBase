/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Lars Wirzenius <liw@iki.fi>
 */
 
/* Tools used internally by the shtring implementation. */


#include "shtring_internal.h"


/* Routines for checking the sanity of data structures. 

   These routines are defined only when debugging. When not
   debugging, speed is of essence, and checking is costly. */

/* Print complaint to stderr. */
#define COMPLAIN(msg)	fprintf(stderr, "COMPLAIN: %s\n", (msg))


/* Make sanity checks on a shtring_chunk. Return 0 if there were any problems,
   1 if all seems fine. */
int shtring_check_chunk(ptr_t chunk)
{
  word_t len;

  if (chunk == NULL_PTR || CELL_TYPE(chunk) != CELL_shtring_chunk)
    return 0;
  len = SHTRING_CHUNK_LENGTH(chunk);
  if (len == 0 || len > SHTRING_CHUNK_MAX)
    return 0;
  return 1;
}


/* Make sanity checks on a shtring_node. Return 0 if there were any problems,
   1 if all seems fine. */
int shtring_check_node(ptr_t node)
{
  word_t h, i, n, len;
  ptr_t child;
  
  if (node == NULL_PTR || CELL_TYPE(node) != CELL_shtring_node)
    return 0;

  n = SHTRING_NODE_CHILDREN(node);
  h = SHTRING_NODE_HEIGHT(node);
  if (n == 0 || n > MAX_SHTRING_NODE_CHILDREN || h < 2)
    return 0;
  if (h == 2) {
    len = 0;
    for (i = 1; i <= n; ++i) {
      child = SHTRING_NODE_CHILD(node, i);
      if (shtring_check_chunk(child) == 0)
	return 0;
      len += SHTRING_CHUNK_LENGTH(child);
    }
  } else {
    len = 0;
    for (i = 1; i <= n; ++i) {
      child = SHTRING_NODE_CHILD(node, i);
      if (shtring_check_node(child) == 0)
	return 0;
      if (h != SHTRING_NODE_HEIGHT(child) + 1)
	return 0;
      len += SHTRING_NODE_LENGTH(child);
    }
  }

  if (len != SHTRING_NODE_LENGTH(node))
    return 0;

  return 1;
}


/* Make sanity checks on a shtring. Return 0 if there were any problems,
   1 if all seems fine. */
int shtring_check_shtring(ptr_t shtr)
{
  ptr_t tree;
  word_t h, tree_len, shtr_len, offset;
  
  if (shtr == NULL_PTR) {
    COMPLAIN("shtring: is a NULL_PTR");
    return 0;
  }

  if (CELL_TYPE(shtr) != CELL_shtring) {
    COMPLAIN("shtring: not a shtring");
    return 0;
  }

  tree = SHTRING_CHUNK_TREE(shtr);
  h = SHTRING_HEIGHT(shtr);
  if (h == 0) {
    if (tree != NULL_PTR) {
      COMPLAIN("shtring: height is zero, but tree is not NULL_PTR");
      return 0;
    }
    tree_len = 0;
  } else if (h == 1) {
    if (shtring_check_chunk(tree) == 0) {
      COMPLAIN("shtring: shtring_check_chunk failed");
      return 0;
    }
    tree_len = SHTRING_CHUNK_LENGTH(tree);
  } else {
    if (shtring_check_node(tree) == 0)
      return 0;
    if (h != SHTRING_NODE_HEIGHT(tree)) {
      COMPLAIN("shtring: height is > 1, but subtree height is wrong");
      return 0;
    }
    tree_len = SHTRING_NODE_LENGTH(tree);
  }

  shtr_len = SHTRING_LENGTH(shtr);
  offset = SHTRING_OFFSET(shtr);
  if (offset > tree_len || offset + shtr_len > tree_len) {
    COMPLAIN("shtring: offset or length in shtring are bad");
    return 0;
  }
  
  return 1;
}





/* Miscellaneous internal utility routines. */

/* Count the number of size `b' blocks needed to hold `a' items. */
word_t shtring_idiv(word_t a, word_t b)
{
  assert(b > 0);
  return (a+b-1)/b;
}


/* Return sum of lengths of all chunks in a tree. */
word_t shtring_tree_length(ptr_t p)
{
  if (p == NULL_PTR)
    return 0;
  heavy_assert(cell_check(p));
  if (CELL_TYPE(p) == CELL_shtring_node)
    return SHTRING_NODE_LENGTH(p);
  assert(CELL_TYPE(p) == CELL_shtring_chunk);
  return SHTRING_CHUNK_LENGTH(p);
}


/* Return height of tree. */
word_t shtring_tree_height(ptr_t p)
{
  if (p == NULL_PTR)
    return 0;
  if (CELL_TYPE(p) == CELL_shtring_node)
    return SHTRING_NODE_HEIGHT(p);
  assert(CELL_TYPE(p) == CELL_shtring_chunk);
  return 1;
}

/* Call fun for each chunk in chunk tree (in the desired order) that contains
   the character positions start .. start+len-1. If fun returns -1, stop.
   Return whatever the last call to fun returned, or 0 if fun wasn't called.
   fun gets pointer to chunk, offset of first interesting character in
   chunk, offset after start of the interesting character, length
   of interesting part of chunk, and arg as arguments. (Interesting is
   the part of the chunk that is within start .. start+len-1.) */

int shtring_monkey(ptr_t root, word_t start, word_t len, word_t *pos,
                  int (*fun)(ptr_t, word_t, word_t, word_t, void *),
		  shtring_monkey_order_t order, void *arg)
{
  int ret;
  word_t i, chunk_len, off_in_chunk, off_in_tree, interesting_len, root_len;

  if (root == NULL_PTR)
    return 0;

  assert(CELL_TYPE(root) == CELL_shtring_node ||
         CELL_TYPE(root) == CELL_shtring_chunk);

  if (*pos == SHTRING_MONKEY_START_POS) {
    switch (order) {
    case left_to_right:
      *pos = 0;
      break;
    case right_to_left:
      *pos = shtring_tree_length(root);
      break;
    }
  }

  ret = 0;
  if (CELL_TYPE(root) == CELL_shtring_node) {
    heavy_assert(shtring_check_node(root));
    root_len = SHTRING_NODE_LENGTH(root);
    switch (order) {
    case left_to_right:
      if (*pos >= start + len || *pos + root_len <= start) {
	*pos += root_len;
	return 0;
      }
      for (i = 1; ret != -1 && i <= SHTRING_NODE_CHILDREN(root); ++i)
	ret = shtring_monkey(SHTRING_NODE_CHILD(root, i), start, len, pos, fun, 
			     order, arg);
      break;
    case right_to_left:
      assert(*pos >= root_len);
      if (*pos - root_len >= start + len || *pos <= start) {
	*pos -= root_len;
	return 0;
      }
      for (i = SHTRING_NODE_CHILDREN(root); ret != -1 && i >= 1; --i)
	ret = shtring_monkey(SHTRING_NODE_CHILD(root, i), start, len, pos, fun, 
			     order, arg);
      break;
    }
  } else {
    heavy_assert(shtring_check_chunk(root));
    chunk_len = SHTRING_CHUNK_LENGTH(root);
    if (order == right_to_left)
      *pos -= chunk_len;
    if (*pos < start + len && *pos + chunk_len > start) {
      if (*pos >= start) {
        off_in_chunk = 0;
        off_in_tree = *pos - start;
      } else {
        off_in_chunk = start - *pos;
        off_in_tree = 0;
      }
      if (*pos + chunk_len <= start + len)
        interesting_len = chunk_len - off_in_chunk;
      else
        interesting_len = start + len - (*pos + off_in_chunk);
      ret = fun(root, off_in_chunk, off_in_tree, interesting_len, arg);
    }

    if (order == left_to_right)
      *pos += chunk_len;
  }
  
  return ret;
}



/* Allocate and initialize various types of cells needed for shtrings. */

/* Allocate a new chunk for `n' characters. Do not initialize the
   characters in the chunk. Assume `can_allocate(SHTRING_CHUNK_WORDS_NEEDED(n))'. */
ptr_t shtring_new_chunk(word_t n)
{
  ptr_t p;

  assert(n > 0);
  p = allocate(SHTRING_CHUNK_WORDS_NEEDED(n), CELL_shtring_chunk);
  p[0] |= n;	/* store number of characters in first word */
  /* The rest of the words are set in build_chunk. */
  heavy_assert(shtring_check_chunk(p));
  return p;
}


/* Create a new node, initializing all pointers to NULL_PTR.
   Assume `can_allocate(3 + children)'. */
ptr_t shtring_new_node(int children, int height)
{
  ptr_t p;
  int i;
  
  assert(children > 0);
  assert(height > 1);

  p = allocate(3 + children, CELL_shtring_node);
  p[0] |= children;			/* store number of children */
  p[1] = 0;				/* sum of lengths of chunks in tree */
  p[2] = height;
  for (i = 0; i < children; ++i)
    p[3+i] = PTR_TO_WORD(NULL_PTR);

  return p;
}


/* Add a single child to the end of a node. */
void shtring_add_child(ptr_t p, ptr_t kid, int *next_unass, word_t len)
{
  int i;
  
  assert(p != NULL_PTR);
  assert(kid != NULL_PTR);
  assert(CELL_TYPE(p) == CELL_shtring_node);
  assert(SHTRING_NODE_HEIGHT(p) > 1);
  if (SHTRING_NODE_HEIGHT(p) == 2)
    heavy_assert(shtring_check_chunk(kid));
  else
    heavy_assert(shtring_check_node(kid));
  assert(shtring_tree_height(kid) + 1 == SHTRING_NODE_HEIGHT(p));
  assert(*next_unass >= 0);

  i = *next_unass;
  assert(i <= (int) SHTRING_NODE_CHILDREN(p));
  p[2 + i] = PTR_TO_WORD(kid);
  p[1] += len;
  *next_unass = i + 1;
}


/* Add children i to j of p to q. */
void shtring_add_children(ptr_t q, int *next_unass, ptr_t p, int i, int j)
{
  ptr_t kid;

  assert(p != NULL_PTR);
  assert(CELL_TYPE(p) == CELL_shtring_node);
  assert(i <= j);
  assert(i >= 1);
  assert(j <= (int) SHTRING_NODE_CHILDREN(p));
  for (; i <= j; ++i) {
    kid = SHTRING_NODE_CHILD(p, i);
    shtring_add_child(q, kid, next_unass, shtring_tree_length(kid));
  }
}


/* Create a new CELL_shtring. */
ptr_t shtring_new_shtring(word_t offset, word_t len, word_t height, ptr_t root)
{
  ptr_t p;

  heavy_assert(root == NULL_PTR || shtring_check_chunk(root) || shtring_check_node(root));  
  assert(height == shtring_tree_height(root));
  assert(offset <= shtring_tree_length(root));
  assert(offset + len <= shtring_tree_length(root));
  
  p = allocate(SHTRING_WORDS, CELL_shtring);
  p[1] = offset;
  p[2] = len;
  p[3] = height;
  p[4] = PTR_TO_WORD(root);

  heavy_assert(shtring_check_shtring(p));
  return p;
}


#if 0 /* use newer cursors instead */


/* Set cursor to a position in a shtring. */
int shtring_cursor_init(shtring_cursor_t *cur, ptr_t root, word_t start, 
			word_t len)
{
  int i;
  
  cur->height = shtring_tree_height(root);
  if (cur->height > MAX_CURSOR_HEIGHT)
    return -1;
  
  if (cur->height == 0) {
    cur->height = -1;
    return 0;
  }

  for (i = 0; i < cur->height - 1; ++i) {
    heavy_assert(shtring_check_node(root));
    cur->ptr[i] = root;
    cur->child[i] = 1;
    root = SHTRING_NODE_CHILD(root, 1);
  }
  cur->chunk = root;
  heavy_assert(shtring_check_chunk(cur->chunk));

  cur->start = start;
  cur->max_pos = start + len;
  cur->chunk_pos = 0;
  cur->chunk_length = SHTRING_CHUNK_LENGTH(root);
  
  return 0;
}

/* Move the cursor forward to position `pos', and return the chunk
   at a the new position. `pos' may be the current position of the
   cursor, but not before it. 

   `*chstart' will be set to the number of characters that need to be
   skipped from the beginning of the chunk to get at the interesting
   part of the text. Similarly for `*chend', but from the end of the
   chunk.
   
   */
ptr_t shtring_cursor_get_chunk(shtring_cursor_t *cur, word_t pos, 
			      word_t *chstart, word_t *chend)
{
  int i, j;

  if (pos >= cur->max_pos)
    cur->height = -1;

  if (cur->height == -1)
    return NULL_PTR;

  assert(cur->height > 0);	/* Empty trees are checked specially. No
  				   need for this function to handle them. */

  /* Is the new position within the current chunk? If not, move the pointers
     forward, starting at the leafmost node and going rootwards, until we
     find a level whose pointer can be moved. Then set the pointers at
     levels towards the leafs to the beginning of each node. */
/* xxx does this work at all if the new pos is further than one node forward?*/
  if (cur->chunk_pos + cur->chunk_length <= pos) {
    for (i = cur->height - 2; i >= 0; --i) {
      if (cur->child[i] < (int) SHTRING_NODE_CHILDREN(cur->ptr[i])) { 
	++cur->child[i];
	for (j = i; j < cur->height-2; ++j) {
	  cur->child[j+1] = 1;
	  cur->ptr[j+1] = SHTRING_NODE_CHILD(cur->ptr[j], cur->child[j]);
	}
	assert(cur->height > 0);
	j = cur->height - 2;
	cur->chunk = SHTRING_NODE_CHILD(cur->ptr[j], cur->child[j]);
	cur->chunk_pos += cur->chunk_length;
	cur->chunk_length = SHTRING_CHUNK_LENGTH(cur->chunk);
	assert(cur->chunk_pos + cur->chunk_length > pos);
	break;
      }
    }
    
    /* If there was a new chunk, i is >= 0. If i < 0, then we've exhausted
       the chunks, and need to quit. */
    if (i < 0) {
      cur->height = -1;
      return NULL_PTR;
    }
  }

  assert(cur->chunk_pos + cur->start < cur->max_pos);
  assert(cur->chunk_pos <= pos);
  assert(pos < cur->chunk_pos + cur->chunk_length);

  if (cur->chunk_pos < cur->start)
    *chstart = cur->start - cur->chunk_pos;
  else
    *chstart = 0;
  if (cur->chunk_pos + cur->chunk_length > cur->max_pos)
    *chend = cur->chunk_pos + cur->chunk_length - cur->max_pos;
  else
    *chend = 0;

  assert(*chstart + *chend <= cur->chunk_length);
  heavy_assert(shtring_check_chunk(cur->chunk));
  return cur->chunk;
}

#endif
