/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 * 
 * 2-3-4 internal search tree operations.         
 *
 * For more information of destructive 2-3-4 tree see Sedgewick:
 * Leiserson, Rivest: Algotrithms, Addison-Wesley, 2nd edition, (1989),
 * pages 216-219.
 *
 * For more information of this non destructive implementation see
 * author's Thesis Tietorakenteiden tietoa tuhoamaton toteutus,
 * Helsingin yliopisto, 1998.  */

#include "includes.h"
#include "shades.h"
#include "ist234.h"

/* General rule to remember:
 *
 * Let i be index of a key in node `p' of `CELL_ist234'.
 * Let number_of_keys be `p[0]' & 0xFFFFFF - 1.
 * Then 
 *      -index of the data of the key is number_of_keys + i.
 *      -index of the pointer to the nearest leftXXX subtree of the key 
 *       2 * number_of_keys + i.
 *      -index of the pointer to the nearest rightXXX subtree of the key is  
 *       2 * number_of_keys + i + 1.
 */

#define NODE_TYPE(p) (((p)[0]) & 0xFFFFFF)
#define LEAF(p)      ((p)[1] == NULL_WORD)

/* Return smallest key in the tree, or NULL_PTR if the tree is empty. */
ptr_t ist234_find_min(ptr_t root) 
{
  ptr_t p;

  if (root == NULL_PTR)
    return NULL_PTR;

  p = root;

  while (p[1] != NULL_WORD) {
    /* Always go to the left subtree */
    assert(CELL_TYPE(p) == CELL_ist234);
    p = WORD_TO_PTR(p[1]);
  }

  return WORD_TO_PTR(p[3]);
}
   
/* `root' points to an 2-3-4 tree. If data with key `key' exists in the
   tree, return the data. Use function `item_cmp' to comparise `key'
   against keys of the nodes. If the `key' doesn't exist in the tree, or
   for some reason comparison with `item_cmp' doesn't succeed, return
   NULL_PTR. */
ptr_t ist234_find(ptr_t root, ptr_t key, 
		  cmp_fun_t item_cmp, void *context) 
{ 
  ptr_t p; 
  cmp_result_t cmp_result;
  int number_of_keys, i;
  
  p = root;
  /* If `key' is stored in `p' return it. Otherwise go to a
     subtree. */
loop:
  
  if (p == NULL_PTR)
    /* `key' doesn't exist in the tree. */  
    return NULL_PTR;

  assert(CELL_TYPE(p) == CELL_ist234);
  /* Compare the first key in `p' against the `key'. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[2]), context);
  if (cmp_result == CMP_ERROR) {
    /* For some reason the comparison didn't succeed. */
    fprintf(stderr, "ist234_find: CMP_ERROR"); 
    return NULL_PTR;
  }
  if (cmp_result == CMP_EQUAL) 
    /* `key' exists. Return data. */
    return WORD_TO_PTR(p[3]);
  if (cmp_result == CMP_LESS) { 
    p = WORD_TO_PTR(p[1]);
    goto loop;
  }
  if (NODE_TYPE(p) == 2) {
    p = WORD_TO_PTR(p[4]);
    goto loop;
  }
  /* Compare the second key in `p' against the `key'. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[5]), context);
  if (cmp_result == CMP_ERROR) {
    /* For some reason the comparison didn't succeed. */
    fprintf(stderr, "ist234_find: CMP_ERROR"); 
    return NULL_PTR;
  }
  if (cmp_result == CMP_EQUAL) 
    /* `key' exists. Return data. */
    return WORD_TO_PTR(p[6]);
  if (cmp_result == CMP_LESS) { 
    p = WORD_TO_PTR(p[4]);
    goto loop;
  }
  if (NODE_TYPE(p) == 3) {
    p = WORD_TO_PTR(p[7]);
    goto loop;
  }
  /* Compare third key in `p' against the `key'. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[8]), context);
  if (cmp_result == CMP_ERROR) {
    /* For some reason the comparison didn't succeed. */
    fprintf(stderr, "ist234_find: CMP_ERROR"); 
    return NULL_PTR;
  }
  if (cmp_result == CMP_EQUAL) 
    /* `key' exists. Return data. */
    return WORD_TO_PTR(p[9]);
  if (cmp_result == CMP_LESS)  
    p = WORD_TO_PTR(p[7]);
  else 
    p = WORD_TO_PTR(p[10]);
  goto loop;
}


/* `root points to an 2-3-4 tree. If `key' already exists in that
   tree:

   If key already exits:

   a) Update existing data to `data'. 

   b) If `data' and data in tree with key `key' are physically same, do
   nothing: return initial root.

   If `key' doesn't exist, insert `data' with key `key' as normal, and
   return new root.

   Use function `item_cmp' to comparise `key' against nodes in the
   tree. If for some reason comparison with `item_cmp' doesn't succeed
   return original root. 

   The algorithm consists of three phases: loop, insert, and update. 
   
   * First phase, the loop: In the loop phase the tree will be traversed
   from root to leaf. If the key exits, then go to update
   phase. Otherwise, go to insert phase.  
   
   All allocations are done only
   in the insert, and update phases. There the tree will be traversed
   backwards - from leaf to root.  
   
   * Second phase, insert: Here the key will be inserted. If there's no
   room in the leaf, e.g. it's 4-node, then split the leaf, and pass
   one of the keys and data of it to the parent. Otherwise, insert
   the key by creating a node one bigger than before. After the insertion 
   is done, go to the update phase. 
   
   * Third phase, update: Here the data will be updated. After updating, 
   update nodes of the rest of the path to point their updated son. 
   
   The actual code is very long, but also very logical. Most on the
   lengthness of the code becomes from almost similar
   switch-case statements.  */
ptr_t ist234_insert(ptr_t root, ptr_t key,
		    cmp_fun_t item_cmp, void *context,
		    ptr_t (*update_fun)(ptr_t, ptr_t), ptr_t data)
{
  ptr_t p, new_p, p_1, p_2, new_p_1, new_p_2;
  cmp_result_t cmp_result;
  struct {
    ptr_t node;
    int i; 
  } path[32];
  int level = 0, i, cell_size;

  p = root;
  /* If `key' is stored in `p' update it. Otherwise go to a
     subtree. If `key' doesn't exist in a hole tree, insert it. */
loop:
  
  if (p == NULL_PTR) {
    /* `key' doesn't exist in the tree. Next step is to insert `key'.*/  
    p_1 = NULL_PTR;
    p_2 = NULL_PTR;
    goto insert;
  }

  assert(CELL_TYPE(p) == CELL_ist234);
  
  /* Compare the first key in `p' against the `key'. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[2]), context);
  if (cmp_result == CMP_ERROR) {
    /* For some reason the comparison didn't succeed. */
    fprintf(stderr, "ist234_insert: CMP_ERROR"); 
    return root;
  }
  if (cmp_result == CMP_EQUAL) {
    /* `key' already exists in 2-3-4 tree. Update the data of it. */
    if (update_fun != NULL)
      data = update_fun(WORD_TO_PTR(p[3]), data);
    if (WORD_TO_PTR(p[3]) == data)
      /* The insertion is nilpotent: retract the work and return the
	 original root. */
      return root;
    cell_size = 3 * NODE_TYPE(p) - 1;
    /* Update data in `p' to `data'. */
    new_p = allocate(cell_size, CELL_ist234);
    for (i = 0; i < cell_size; i++)
      new_p[i] = p[i];
    new_p[3] = PTR_TO_WORD(data);  
    goto update;
  } if (cmp_result == CMP_LESS) {
    path[level].node = p;
    path[level].i = 1;
    level++;
    p = WORD_TO_PTR(p[1]);
    goto loop;
  }
  if (NODE_TYPE(p) == 2) {
    /* 2-node */
    path[level].node = p;
    path[level].i = 4;
    level++;    
    p = WORD_TO_PTR(p[4]);
    goto loop;
  }
  /* Compare the second key in `p' against the `key'. */

  cmp_result = item_cmp(key, WORD_TO_PTR(p[5]), context);
  if (cmp_result == CMP_ERROR) {
    /* For some reason the comparison didn't succeed. */
    fprintf(stderr, "ist234_insert: CMP_ERROR"); 
    return root;
  }
  if (cmp_result == CMP_EQUAL) { 
    /* `key' already exists in 2-3-4 tree. Update the data of it. */
    if (update_fun != NULL)
      data = update_fun(WORD_TO_PTR(p[6]), data);
    if (WORD_TO_PTR(p[3]) == data)
      /* The insertion is nilpotent: retract the work and return the
	 original root. */
      return root;
    cell_size = 3 * NODE_TYPE(p) - 1;
    /* Update data in `p' to `data'. */
    new_p = allocate(cell_size, CELL_ist234);
    for (i = 0; i < cell_size; i++)
      new_p[i] = p[i];
    new_p[6] = PTR_TO_WORD(data);
    goto update;
  } 
  if (cmp_result == CMP_LESS) {
    path[level].node = p;
    path[level].i = 4;
    level++;
    p = WORD_TO_PTR(p[4]);
    goto loop;
  }
  if (NODE_TYPE(p) == 3) {
    /* 3-node */
    path[level].node = p;
    path[level].i = 7;
    level++;
    p = WORD_TO_PTR(p[7]);
    goto loop;
  }
  /* The only possibility is that `p' is 4-node. */

  /* Compare third key in `p' against the `key'. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[8]), context);
  if (cmp_result == CMP_ERROR) {
    /* For some reason the comparison didn't succeed. */
    fprintf(stderr, "ist234_insert: CMP_ERROR"); 
    return root;
  }
  if (cmp_result == CMP_EQUAL) {
    /* `key' already exists in 2-3-4 tree. Update the data of it. */
    if (update_fun != NULL)
      data = update_fun(WORD_TO_PTR(p[9]), data);
    if (WORD_TO_PTR(p[9]) == data)
      /* The insertion is nilpotent: retract the work and return the
	 original root. */
      return root;
    /* Update data in `p' to `data'. */
    new_p = allocate(11, CELL_ist234);
    for (i = 0; i < 11; i++)
      new_p[i] = p[i];
    new_p[9] = PTR_TO_WORD(data);  
    goto update;
  }
  path[level].node = p;  
  if (cmp_result == CMP_LESS) {
    path[level].i = 7;
    p = WORD_TO_PTR(p[7]);
  } else {
    path[level].i = 10;
    p = WORD_TO_PTR(p[10]);
  }
  level++;  
  goto loop;
  
  /* END OF THE LOOP */

insert:
  
  /* `p' == NULL_PTR - `key' doesn't exist in the tree. If there's
     room in the upper level's node e.g node isn't 4-node, insert
     `key' to that node. Otherwise split the node and pass one of the
     keys to the parent of the node. Continue this splitting and
     passing as long as we'll find a node that isn't 4-node. 
     
     Notice that the height of the tree becomes one heigher only when
     all the nodes in the path are 4-nodes. 
     In such case we'll split the root as normal
     and pass one of the keys to "parent" by creating a 2-node that
     contains the key and pointers to the splitted root. */
  if (level == 0) {
    new_p = allocate(5, CELL_ist234);
    new_p[0] |= 2;
    new_p[1] = PTR_TO_WORD(p_1);
    new_p[2] = PTR_TO_WORD(key);
    new_p[3] = PTR_TO_WORD(data);
    new_p[4] = PTR_TO_WORD(p_2);
    return new_p;
  }

  level--;
  p = path[level].node;
  switch (NODE_TYPE(p)) {
  case 4:
    /* 4-node. There's no room for another key. Split `p' to `new_p_1'
       and `new_p_2'. `new_p_1' is a 3-node and `new_p_2'
       2-node. Later key, and data between `new_p_1' and `new_p_2'
       will be passed to the parent with `new_p_1', and
       `new_p_2'. */
    new_p_1 = allocate(8, CELL_ist234);
    new_p_1[0] |= 3;
    new_p_2 = allocate(5, CELL_ist234);
    new_p_2[0] |= 2;
    switch (path[level].i) {
    case 1:
      new_p_1[1] = PTR_TO_WORD(p_1); 
      new_p_1[2] = PTR_TO_WORD(key);
      new_p_1[3] = PTR_TO_WORD(data);
      new_p_1[4] = PTR_TO_WORD(p_2); 
      new_p_1[5] = p[2]; /* First key in `p'. */
      new_p_1[6] = p[3];
      new_p_1[7] = p[4];
      new_p_2[1] = p[7]; 
      new_p_2[2] = p[8]; 
      new_p_2[3] = p[9]; 
      new_p_2[4] = p[10];
      /* Pass one of the keys to the parent. */
      key = WORD_TO_PTR(p[5]);
      data = WORD_TO_PTR(p[6]);
      p_1 = new_p_1;
      p_2 = new_p_2;
      goto insert;
    case 4:
      new_p_1[1] = p[1]; 
      new_p_1[2] = p[2];
      new_p_1[3] = p[3];
      new_p_1[4] = PTR_TO_WORD(p_1);
      new_p_1[5] = PTR_TO_WORD(key);
      new_p_1[6] = PTR_TO_WORD(data);
      new_p_1[7] = PTR_TO_WORD(p_2);
      new_p_2[1] = p[7]; 
      new_p_2[2] = p[8]; 
      new_p_2[3] = p[9]; 
      new_p_2[4] = p[10];
      /* Pass one of the keys to the parent. */
      key = WORD_TO_PTR(p[5]);
      data = WORD_TO_PTR(p[6]);
      p_1 = new_p_1;
      p_2 = new_p_2;
      goto insert;
    case 7:
      /* In this case will `key' and `data' be passed to the parent. */    
      new_p_1[1] = p[1]; 
      new_p_1[2] = p[2];
      new_p_1[3] = p[3];
      new_p_1[4] = p[4]; 
      new_p_1[5] = p[5];
      new_p_1[6] = p[6];
      new_p_1[7] = PTR_TO_WORD(p_1); 
      new_p_2[1] = PTR_TO_WORD(p_2); 
      new_p_2[2] = p[8]; 
      new_p_2[3] = p[9]; 
      new_p_2[4] = p[10];
      p_1 = new_p_1;
      p_2 = new_p_2;
      goto insert;
    case 10:
      new_p_1[1] = p[1]; 
      new_p_1[2] = p[2];
      new_p_1[3] = p[3];
      new_p_1[4] = p[4]; 
      new_p_1[5] = p[5];
      new_p_1[6] = p[6];
      new_p_1[7] = p[7];
      new_p_2[1] = PTR_TO_WORD(p_1);
      new_p_2[2] = PTR_TO_WORD(key); 
      new_p_2[3] = PTR_TO_WORD(data); 
      new_p_2[4] = PTR_TO_WORD(p_2);
      /* Pass one of the keys to the parent. */
      key = WORD_TO_PTR(p[8]);
      data = WORD_TO_PTR(p[9]);
      p_1 = new_p_1;
      p_2 = new_p_2;
      goto insert;  
    }
  case 3:
    new_p = allocate(11, CELL_ist234);
    new_p[0] |= 4;
    switch (path[level].i) {
    case 1:
      new_p[1] = PTR_TO_WORD(p_1);
      new_p[2] = PTR_TO_WORD(key);
      new_p[3] = PTR_TO_WORD(data);
      new_p[4] = PTR_TO_WORD(p_2);
      new_p[5] = p[2];
      new_p[6] = p[3];
      new_p[7] = p[4];
      new_p[8] = p[5];
      new_p[9] = p[6];
      new_p[10] = p[7];
      goto update;
    case 4:
      new_p[1] = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      new_p[4] = PTR_TO_WORD(p_1);
      new_p[5] = PTR_TO_WORD(key);
      new_p[6] = PTR_TO_WORD(data);
      new_p[7] = PTR_TO_WORD(p_2);
      new_p[8] = p[5];
      new_p[9] = p[6];
      new_p[10] = p[7];
      goto update;
    case 7:
      new_p[1] = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      new_p[4] = p[4];
      new_p[5] = p[5];
      new_p[6] = p[6];
      new_p[7] = PTR_TO_WORD(p_1);
      new_p[8] = PTR_TO_WORD(key);
      new_p[9] = PTR_TO_WORD(data);
      new_p[10] = PTR_TO_WORD(p_2);
      goto update;
    }
  case 2:
    new_p = allocate(8, CELL_ist234);
    new_p[0] |= 3;
    switch (path[level].i) {
    case 1:
      new_p[1] = PTR_TO_WORD(p_1);
      new_p[2] = PTR_TO_WORD(key);
      new_p[3] = PTR_TO_WORD(data);
      new_p[4] = PTR_TO_WORD(p_2);
      new_p[5] = p[2];
      new_p[6] = p[3];
      new_p[7] = p[4];
      goto update;
    case 4:
      new_p[1] = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      new_p[4] = PTR_TO_WORD(p_1);
      new_p[5] = PTR_TO_WORD(key);
      new_p[6] = PTR_TO_WORD(data);
      new_p[7] = PTR_TO_WORD(p_2);
      goto update;
    }
  }

  /* END OF THE INSERT PHASE */

update:
  
  if (level == 0) 
    return new_p;
  
  /* Next step is to recursively update the father of `p' to point
     to `data'. */
  level--;
  cell_size = 3 * NODE_TYPE(path[level].node) - 1;
  p = allocate(cell_size, CELL_ist234);
  for (i = 0; i < cell_size; i++)
    p[i] = path[level].node[i];
  p[path[level].i] = PTR_TO_WORD(new_p);
  new_p = p;
  goto update;

  /* END OF THE UPDATE PHASE */
}

static inline ptr_t copy_and_transform(ptr_t node, int *ix, int *i_of_key,
				       int *n_of_copied_levels)
{
  ptr_t new_node, p, new_p, brother, left_subtree, right_subtree;
  
  /* Type of the root of the subtree, where the key might exist, is 2-node.
     Now we must make step 1a) or 1c) to change the type to 3-node or 4-node.
     1a)
     1b)*/
  *n_of_copied_levels = 2;
  *i_of_key = 2;
  
  p = WORD_TO_PTR(node[*ix]);
  assert(NODE_TYPE(p) == 2);
  if (*ix == 1) {
    brother = WORD_TO_PTR(node[4]);
    if (NODE_TYPE(brother) == 2) {
      new_p = allocate(11, CELL_ist234);
      new_p[0] |= 4;
      new_p[1] = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      new_p[4] = p[4];
      new_p[5] = node[2];
      new_p[6] = node[3];
      new_p[7] = brother[1];
      new_p[8] = brother[2];
      new_p[9] = brother[3];
      new_p[10] = brother[4];
      switch(NODE_TYPE(node)) {
      case 4:
        new_node = allocate(8, CELL_ist234);
        new_node[0] |= 3;
        new_node[1] = PTR_TO_WORD(new_p);
        new_node[2] = node[5];
        new_node[3] = node[6];
        new_node[4] = node[7];
        new_node[5] = node[8];
        new_node[6] = node[9];
        new_node[7] = node[10];
        break;
      case 3:
        new_node = allocate(5, CELL_ist234);
        new_node[0] |= 2;
        new_node[1] = PTR_TO_WORD(new_p);
        new_node[2] = node[5];
        new_node[3] = node[6];
        new_node[4] = node[7];
        break;
      case 2:
        *n_of_copied_levels = 1;
        return new_p;
      }
      return new_node;
    }
    /* Type of the sibling `brother' is 3-node or 4-node. */
    left_subtree = allocate(8, CELL_ist234);
    left_subtree[0] |= 3;
    left_subtree[1] = p[1];
    left_subtree[2] = p[2];
    left_subtree[3] = p[3];
    left_subtree[4] = p[4];
    left_subtree[5] = node[2];
    left_subtree[6] = node[3];
    left_subtree[7] = brother[1];
    right_subtree = allocate(3 * NODE_TYPE(brother) - 4, CELL_ist234);
    right_subtree[0] |= NODE_TYPE(brother) - 1;
    right_subtree[1] = brother[4];
    right_subtree[2] = brother[5];
    right_subtree[3] = brother[6];
    right_subtree[4] = brother[7];
    if (NODE_TYPE(brother) == 4) {
      right_subtree[5] = brother[8];
      right_subtree[6] = brother[9];
      right_subtree[7] = brother[10];
    }
    new_node = allocate(3 * NODE_TYPE(node) - 1, CELL_ist234);
    new_node[0] |= NODE_TYPE(node);
    new_node[1] = PTR_TO_WORD(left_subtree);
    new_node[2] = brother[2];
    new_node[3] = brother[3];
    new_node[4] = PTR_TO_WORD(right_subtree);
    if (NODE_TYPE(node) > 2) {
      new_node[5] = node[5];
      new_node[6] = node[6];
      new_node[7] = node[7];
    }
    if (NODE_TYPE(node) > 3) {
      new_node[8] = node[8];
      new_node[9] = node[9];
      new_node[10] = node[10];
    }
    return new_node;
  }
  brother = WORD_TO_PTR(node[*ix - 3]);
  if (NODE_TYPE(brother) == 2) {
    new_p = allocate(11, CELL_ist234);
    new_p[0] |= 4;
    new_p[1] = brother[1];
    new_p[2] = brother[2];
    new_p[3] = brother[3];
    new_p[4] = brother[4];
    new_p[5] = node[*ix - 2];
    new_p[6] = node[*ix - 1];
    new_p[7] = p[1];
    new_p[8] = p[2];
    new_p[9] = p[3];
    new_p[10] = p[4];
    *i_of_key = 8;
    switch(NODE_TYPE(node)) {
    case 4:
      new_node = allocate(8, CELL_ist234);
      new_node[0] |= 3;
      switch (*ix) {
      case 4:
        new_node[1] = PTR_TO_WORD(new_p);
        new_node[2] = node[5];
        new_node[3] = node[6];
        new_node[4] = node[7];
        new_node[5] = node[8];
        new_node[6] = node[9];
        new_node[7] = node[10];
        break;
      case 7:
        new_node[1] = node[1];
        new_node[2] = node[2];
        new_node[3] = node[3];
        new_node[4] = PTR_TO_WORD(new_p);
        new_node[5] = node[8];
        new_node[6] = node[9];
        new_node[7] = node[10];
        break;
      case 10:
        new_node[1] = node[1];
        new_node[2] = node[2];
        new_node[3] = node[3];
        new_node[4] = node[4];
        new_node[5] = node[5];
        new_node[6] = node[6];
        new_node[7] = PTR_TO_WORD(new_p);
        break;
      }
      break;
    case 3:
      new_node = allocate(5, CELL_ist234);
      new_node[0] |= 2;
      switch (*ix) {
      case 4:                         
	new_node[1] = PTR_TO_WORD(new_p);
	new_node[2] = node[5];
        new_node[3] = node[6];
        new_node[4] = node[7];
        break;
      case 7:
        new_node[1] = node[1];
        new_node[2] = node[2];
        new_node[3] = node[3];
        new_node[4] = PTR_TO_WORD(new_p);
        break;
      }
       break;
    case 2:
      *n_of_copied_levels = 1;
      return new_p;
    }
    *ix = *ix - 3;
    return new_node;
  }
  /* Type of the sibling `brother' is 3-node or 4-node. */
  left_subtree = allocate(3 * NODE_TYPE(brother) - 4, CELL_ist234);
  left_subtree[0] |= NODE_TYPE(brother) - 1;
  left_subtree[1] = brother[1];
  left_subtree[2] = brother[2];
  left_subtree[3] = brother[3];
  left_subtree[4] = brother[4];
  if (NODE_TYPE(brother) > 3) {
    left_subtree[5] = brother[5];
    left_subtree[6] = brother[6];
    left_subtree[7] = brother[7];
  }
  right_subtree = allocate(8, CELL_ist234);
  right_subtree[0] |= 3;
  right_subtree[1] = brother[3 * NODE_TYPE(brother) - 2];
  right_subtree[2] = node[*ix - 2];
  right_subtree[3] = node[*ix - 1];
  right_subtree[4] = p[1];
  right_subtree[5] = p[2];
  right_subtree[6] = p[3];
  right_subtree[7] = p[4];
  *i_of_key = 5;
  new_node = allocate(3 * NODE_TYPE(node) - 1, CELL_ist234);
  new_node[0] |= NODE_TYPE(node);
  switch (*ix) {
  case 4:
    new_node[1] = PTR_TO_WORD(left_subtree);
    new_node[2] = brother[3 * NODE_TYPE(brother) - 4];
    new_node[3] = brother[3 * NODE_TYPE(brother) - 3];
    new_node[4] = PTR_TO_WORD(right_subtree);
    if (NODE_TYPE(node) > 2) {
    new_node[5] = node[5];
    new_node[6] = node[6];
    new_node[7] = node[7];
    }
    if (NODE_TYPE(node) > 3) {
      new_node[8] = node[8];
      new_node[9] = node[9];
      new_node[10] = node[10];
    }
    break;
  case 7:
    new_node[1] = node[1];
    new_node[2] = node[2];
    new_node[3] = node[3];
    new_node[4] = PTR_TO_WORD(left_subtree);
    new_node[5] = brother[3 * NODE_TYPE(brother) - 4];
    new_node[6] = brother[3 * NODE_TYPE(brother) - 3];
    new_node[7] = PTR_TO_WORD(right_subtree);
    if (NODE_TYPE(node) > 3) {
      new_node[8] = node[8];
      new_node[9] = node[9];
      new_node[10] = node[10];
    }
    break;
  case 10:
    new_node[1] = node[1];
    new_node[2] = node[2];
    new_node[3] = node[3];
    new_node[4] = node[4];
    new_node[5] = node[5];
    new_node[6] = node[6];
    new_node[7] = PTR_TO_WORD(left_subtree);
    new_node[8] = brother[3 * NODE_TYPE(brother) - 4];
    new_node[9] = brother[3 * NODE_TYPE(brother) - 3];
    new_node[10] = PTR_TO_WORD(right_subtree);
  }
  return new_node;
}

static inline ptr_t copy_node(ptr_t p) {
  
  ptr_t new_p = allocate(3 * NODE_TYPE(p) - 1, CELL_ist234);  
  new_p[0] = p[0];
  new_p[1] = p[1];
  new_p[2] = p[2];
  new_p[3] = p[3];
  new_p[4] = p[4];
  if (NODE_TYPE(p) > 2) {
    new_p[5] = p[5];
    new_p[6] = p[6];
    new_p[7] = p[7];
  }
  if (NODE_TYPE(p) > 3) {
    new_p[8] = p[8];
    new_p[9] = p[9];
    new_p[10] = p[10];
  }
  return new_p;
}

/* `root' is a pointer to 2-3-4 tree. If data with key `key' doesn't
   exist in that tree do nothing: restore allocation point and return
   original root `root'.  If key `key' exists in tree:

   If function `delete_fun' is not NULL_PTR:

   a) If `data' and data in tree with key `key' are physically same, do
      nothing: restore allocation point and return original root `root'.

   b) Else update existing data to `data'.

   If `delete_fun' is NULL_PTR, delete `data' with key `key' as normal.

   Use function `item_cmp' to comparise `key' against nodes in the
   tree. If for some reason comparison with `item_cmp' doesn't succeed
   restore allocation point and return original root `root'. */
ptr_t ist234_delete(ptr_t root, ptr_t key,
		    cmp_fun_t item_cmp, void *context,
		    ptr_t (*delete_fun)(ptr_t, ptr_t), ptr_t data)
{
  ptr_t p, ap, pp, new_p, left_subtree, right_subtree, joint, new_root;
  ptr_t brother;
  int i, ix, i_of_key, cell_size, is_not_copied_yet = 1;
  int number_of_copied_levels;
  cmp_result_t cmp_result;

  ap = get_allocation_point();
  new_root = NULL_PTR;

  p = root;
find:

  if (p == NULL_PTR) {
    /* `key' doesn't exist in the tree. */
    restore_allocation_point(ap);
    return root;
  }
  assert(CELL_TYPE(p) == CELL_ist234);
  
  /* Compare the first key in `p' against the `key'. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[2]), context);
  if (cmp_result == CMP_EQUAL) {
    i_of_key = 2;
    goto delete_key;
  }
  if (cmp_result == CMP_LESS) {
    ix = 1;
    goto copy;
  }
  if (cmp_result == CMP_ERROR) {
    /* For some reason the comparison didn't succeed. */
    fprintf(stderr, "ist234_delete: CMP_ERROR");
    restore_allocation_point(ap);
    return root;
  }
  if (NODE_TYPE(p) == 2) {
    ix = 4;
    goto copy;
  }
  /* Compare the second key in `p' against the `key'. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[5]), context);
  if (cmp_result == CMP_EQUAL) {
    i_of_key = 5;
    goto delete_key;
  }
  if (cmp_result == CMP_LESS) {
    ix = 4;
    goto copy;
  }
  if (cmp_result == CMP_ERROR) {
    /* For some reason the comparison didn't succeed. */
    fprintf(stderr, "ist234_delete: CMP_ERROR");
    restore_allocation_point(ap);
    return root;
  }
  if (NODE_TYPE(p) == 3) {
    ix = 7;
    goto copy;
  }
  /* Compare third key in `p' against the `key'. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[8]), context);
  if (cmp_result == CMP_EQUAL) {
    i_of_key = 8;
    goto delete_key;
  }
  if (cmp_result == CMP_ERROR) {
    /* For some reason the comparison didn't succeed. */
    fprintf(stderr, "ist234_delete: CMP_ERROR");
    restore_allocation_point(ap);
    return root;
  }
  if (cmp_result == CMP_LESS)
    ix = 7;
  else
    ix = 10;
  
copy:
  
  if (p[ix] == NULL_WORD) {
    /* `key' doesn't exist in the tree. */
    restore_allocation_point(ap);
    return root;
  }
  if (NODE_TYPE(WORD_TO_PTR(p[ix])) > 2) {
    if (is_not_copied_yet) {
      new_p = copy_node(p);
      if ((new_root == NULL_PTR) || (new_root == p))
	new_root = new_p;
      else
	*pp = PTR_TO_WORD(new_p);
    } else {
      new_p = p;
      is_not_copied_yet = 1;
    }
    pp = &new_p[ix];
    p = WORD_TO_PTR(new_p[ix]);
    goto find;
  }
  /* Type of the root of the subtree, where the key might exist, is 2-node. */
  new_p = copy_and_transform(p, &ix, &i_of_key,	&number_of_copied_levels);
  if ((new_root == NULL_PTR) || (new_root == p))
    new_root = new_p;
  else
    *pp = PTR_TO_WORD(new_p);
  if (number_of_copied_levels == 1)  
    p = new_p;
  else {
    p = WORD_TO_PTR(new_p[ix]);
    pp = &new_p[ix];
  }
  is_not_copied_yet = 0;
  /* Compare the key in `p' against the `key'. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[i_of_key]), context);
  if (cmp_result == CMP_EQUAL)
    goto delete_key;
  if (cmp_result == CMP_ERROR) {
    /* For some reason the comparison didn't succeed. */
    fprintf(stderr, "ist234_delete: CMP_ERROR");
    restore_allocation_point(ap);
    return root;
  }
  if (cmp_result == CMP_LESS) 
    ix = i_of_key - 1;
  else
    ix = i_of_key + 2;
  goto copy;
    
delete_key:

  /* The path is updated now. Next step is to upadate the data if
     `delete_fun != NULL' or delete the key and data. */
  if (delete_fun != NULL) {
    /* Now `p[i_of_key]' should point to key and `p[i_of_key + 1]'
       to data. */
    data = delete_fun(WORD_TO_PTR(p[i_of_key + 1]), data);
    if (WORD_TO_PTR(p[i_of_key + 1]) == data)
      /* The insertion is nilpotent: retract the work and return the
	 original root. */
      return root;
    if (is_not_copied_yet) {
      new_p = copy_node(p);
      if ((new_root == NULL_PTR) || (new_root == p))
	new_root = new_p;
      else
	*pp = PTR_TO_WORD(new_p);
      p = new_p;
    }
    p[i_of_key + 1] = PTR_TO_WORD(data);
    return new_root;
  }

  if (LEAF(p)) 
    goto delete_leaf;

  assert((new_root == NULL_PTR) || (NODE_TYPE(p) > 2));
     
delete_internal_node:

  if (NODE_TYPE(WORD_TO_PTR(p[i_of_key + 2])) > 2) {
    /* Next step is to replace the key with its successor
       in inner order. */
    if (is_not_copied_yet) {
      new_p = copy_node(p);
      if ((new_root == NULL_PTR) || (new_root == p))
	new_root = new_p;
      else
	*pp = PTR_TO_WORD(new_p);
      p = new_p;
    } else
      is_not_copied_yet = 1;
    
    key = &p[i_of_key];
    data = &p[i_of_key+1];
    pp = &p[i_of_key + 2];
    p = WORD_TO_PTR(p[i_of_key + 2]);
    ix = 1;
    
    while(!LEAF(p)) {
      if (NODE_TYPE(WORD_TO_PTR(p[ix])) > 2) {
	if (is_not_copied_yet) {
	  p = copy_node(p);
	  *pp = PTR_TO_WORD(p);
	} else 
	  is_not_copied_yet = 1;
      } else {
	p = copy_and_transform(p, &ix, &i_of_key, 
			       &number_of_copied_levels);
	assert(ix == 1);
	*pp = PTR_TO_WORD(p);
	is_not_copied_yet = 0;
	assert(number_of_copied_levels == 2);
      } 
      pp = &p[ix];
      p = WORD_TO_PTR(p[ix]);
    }
    assert(NODE_TYPE(p) > 2);
    *key = p[2];
    *data = p[3];
    i_of_key = 2;
    goto delete_leaf;
  } else if (NODE_TYPE(WORD_TO_PTR(p[i_of_key - 1])) > 2) {
    /* Next step is to replace the key with its predecessor
       in inner order. */
    if (is_not_copied_yet) {
      new_p = copy_node(p);
      if ((new_root == NULL_PTR) || (new_root == p))
	new_root = new_p;
      else
	*pp = PTR_TO_WORD(new_p);
      p = new_p;
    } else
      is_not_copied_yet = 1;
    
    key = &p[i_of_key];
    data = &p[i_of_key+1];
    pp = &p[i_of_key - 1];
    p = WORD_TO_PTR(p[i_of_key - 1]);

    while(!LEAF(p)) {
      ix = 3 * NODE_TYPE(p) - 2;
      if (NODE_TYPE(WORD_TO_PTR(p[ix])) > 2) {
	if (is_not_copied_yet) {
	  p = copy_node(p);
	  *pp = PTR_TO_WORD(p);
	} else 
	  is_not_copied_yet = 1;
	pp = &p[ix];
	p = WORD_TO_PTR(p[ix]);
      } else {
	p = copy_and_transform(p, &ix, &i_of_key, 
			       &number_of_copied_levels);
	*pp = PTR_TO_WORD(p);
	is_not_copied_yet = 0;
	assert(number_of_copied_levels == 2); 
	pp = &p[ix];
	p = WORD_TO_PTR(p[ix]);
      }
    }
    assert(NODE_TYPE(p) > 2); 
    ix = 3 * NODE_TYPE(p) - 2;
    *key = p[ix - 2];
    *data = p[ix - 1];
    i_of_key = ix - 2;
    goto delete_leaf;
  }
  
  left_subtree = WORD_TO_PTR(p[i_of_key - 1]);
  right_subtree = WORD_TO_PTR(p[i_of_key + 2]);
  /* Both the left and the right subtree are 2-nodes.  Delete key
     from `p' and pass the key downwards and try to delete it there
     along this algorithm. */
  
  if (NODE_TYPE(p) > 2) {
    /* Next step is to remove key and data from `p'. Notice that then we
       have to update `pp' to point to `new_p'. */
    new_p = allocate(3 * NODE_TYPE(p) - 4, CELL_ist234);
    new_p[0] |= NODE_TYPE(p) - 1;
    switch (i_of_key) {
    case 2:
      /* new_p[1] = PTR_TO_WORD(joint); */
      new_p[2] = p[5];
      new_p[3] = p[6];
      new_p[4] = p[7];
      if (NODE_TYPE(p) == 4) {
	new_p[5] = p[8];
	new_p[6] = p[9];
	new_p[7] = p[10];
      }
      break;
    case 5:
      new_p[1] = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      /* new_p[4] = PTR_TO_WORD(joint); */
      if (NODE_TYPE(p) == 4) {
	new_p[5] = p[8];
	new_p[6] = p[9];
	new_p[7] = p[10];
      }
      break;
    case 8:
      new_p[1] = p[1];
      new_p[2] = p[2];
      new_p[3] = p[3];
      new_p[4] = p[4];
      new_p[5] = p[5];
      new_p[6] = p[6];
      /* new_p[7] = PTR_TO_WORD(joint); */
    break;
    }
    if ((new_root == NULL_PTR) || (new_root == p))
      new_root = new_p;
    else
      *pp = PTR_TO_WORD(new_p);  
    p = new_p;
    pp = &p[i_of_key - 1];
  }
 
  /* Next step is to join `left_subtree' and `right_subtree' to `joint'. */
  while ((LEAF(left_subtree)) || 
	 ((NODE_TYPE(WORD_TO_PTR(left_subtree[4])) == 2) && 
	  (NODE_TYPE(WORD_TO_PTR(right_subtree[1])) == 2))) {
    joint = allocate(8, CELL_ist234);
    joint[0] |= 3;
    joint[1] = left_subtree[1];
    joint[2] = left_subtree[2];
    joint[3] = left_subtree[3];
    /* The value of `joint[4]' isn't known yet. */
    joint[5] = right_subtree[2];
    joint[6] = right_subtree[3];
    joint[7] = right_subtree[4];
    if (new_root == NULL_PTR) {
      new_root = joint;
    } else
      *pp = PTR_TO_WORD(joint);
    p = joint;
    pp = &p[4];
    if (LEAF(p)) {
      *pp = NULL_WORD;
      return new_root;
    }
    left_subtree = WORD_TO_PTR(left_subtree[4]);
    right_subtree = WORD_TO_PTR(right_subtree[1]);
  }

  joint = allocate(11, CELL_ist234);
  joint[0] |= 4;
  joint[1] = left_subtree[1];
  joint[2] = left_subtree[2];
  joint[3] = left_subtree[3];
  joint[4] = left_subtree[4];
  /* Key and data e.g. values of `joint[5]' and `joint[6]' aren't
     known yet. */
  joint[7] = right_subtree[1];
  joint[8] = right_subtree[2];
  joint[9] = right_subtree[3];
  joint[10] = right_subtree[4];
  if (new_root == NULL_PTR) {
    new_root = joint;
  } else
    *pp = PTR_TO_WORD(joint);
  p = joint;
  i_of_key = 5;
  is_not_copied_yet = 0;
  goto delete_internal_node;
  
delete_leaf:
  
  /* The key to be deleted is in leaf. If leaf is 4-node or 3-node,
     remove key from the leaf by decreasing the size of the node. If
     the leaf is 2-node, try to join the brother of the leaf and the
     guidepost to the leaf. */
  switch (NODE_TYPE(p)) {
  case 4:
    /* 4-node. Redude it to 3-node. */
    new_p = allocate(8, CELL_ist234);
    new_p[0] |= 3;
    assert(p[1] == NULL_WORD);
    new_p[1] = NULL_WORD;
    assert(p[4] == NULL_WORD);
    new_p[4] = NULL_WORD;
    assert(p[7] == NULL_WORD);
    new_p[7] = NULL_WORD;
    switch (i_of_key) {
    case 2:
      new_p[2] = p[5];
      new_p[3] = p[6];
      new_p[5] = p[8];
      new_p[6] = p[9];
      break;
    case 5:
      new_p[2] = p[2];
      new_p[3] = p[3];
      new_p[5] = p[8];
      new_p[6] = p[9];
      break;
    case 8:
      new_p[2] = p[2];
      new_p[3] = p[3];
      new_p[5] = p[5];
      new_p[6] = p[6];
      break;
    }
    break;
  case 3:
    /* 3-node. Redude it to 2-node. */
    new_p = allocate(5, CELL_ist234);
    new_p[0] |= 2;
    new_p[1] = NULL_WORD;
    assert(p[1] == NULL_WORD);
    new_p[4] = NULL_WORD;
    assert(p[4] == NULL_WORD);
    switch (i_of_key) {
    case 2:
      new_p[2] = p[5];
      new_p[3] = p[6];
      break;
    case 5:
      new_p[2] = p[2];
      new_p[3] = p[3];
      break;
    }
    break;
  case 2:
    assert(new_root == NULL_PTR);
    return NULL_PTR;
  }
  if ((new_root == NULL_PTR) || (new_root == p))
    new_root = new_p;
  else
    *pp = PTR_TO_WORD(new_p);
  return new_root;
}

static void make_assertions(ptr_t node, int max_level, int level)
{
  ptr_t key, key2, key3, data;
  int node_type;

  if (node == NULL_PTR) {
    assert(level == max_level);
    return;
  }
  assert(CELL_TYPE(node) == CELL_ist234);
  node_type = node[0] & 0xFFFFFF;
  if (node[1] == NULL_WORD) {
    assert(node[4] == NULL_WORD);
    if (node_type > 2)
      assert(node[7] == NULL_WORD);
    if (node_type > 3)
      assert(node[10] == NULL_WORD);
  } else {
    assert(CELL_TYPE(WORD_TO_PTR(node[1])) == CELL_ist234);
    assert(CELL_TYPE(WORD_TO_PTR(node[4])) == CELL_ist234);
    if (node_type > 2)
      assert(CELL_TYPE(WORD_TO_PTR(node[7])) == CELL_ist234);
    if (node_type > 3)
      assert(CELL_TYPE(WORD_TO_PTR(node[10])) == CELL_ist234);
  }

  make_assertions(WORD_TO_PTR(node[1]), max_level, level+1);
  key = WORD_TO_PTR(node[2]);
  assert(CELL_TYPE(key) == CELL_word_vector);
  data = WORD_TO_PTR(node[3]);
  assert(CELL_TYPE(data) == CELL_word_vector);
  make_assertions(WORD_TO_PTR(node[4]), max_level, level+1);
  if (node_type == 2)
    return;
  key2 = WORD_TO_PTR(node[5]);
  assert(CELL_TYPE(key2) == CELL_word_vector);
  assert(key[1] < key2[1]);
  data = WORD_TO_PTR(node[6]);
  assert(CELL_TYPE(data) == CELL_word_vector);
  make_assertions(WORD_TO_PTR(node[7]), max_level, level+1);
  if (node_type == 3)
    return;
  key3 = WORD_TO_PTR(node[8]);
  assert(CELL_TYPE(key3) == CELL_word_vector);
  assert(key2[1] < key3[1]);
  data = WORD_TO_PTR(node[9]);
  assert(CELL_TYPE(data) == CELL_word_vector);
  make_assertions(WORD_TO_PTR(node[10]), max_level, level+1);
  return;
}

void ist234_make_assertions(ptr_t node)
{
  ptr_t p = node;
  int level = 0;

  while (p != NULL_PTR) {
    p = WORD_TO_PTR(p[1]);
    level++;
  }
  make_assertions(node, level, 0);
  return;
}

