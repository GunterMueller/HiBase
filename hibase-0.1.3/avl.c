/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 * 
 * AVL tree operations.  For more information of non destructive AVL
 * trees see author's Thesis Tehokkaita tietoa tuhoamattomia
 * tietorakenteita, Helsingin yliopisto, 1998.  */
#include "includes.h"
#include "shades.h"
#include "avl.h"


#define  HEIGHT(p)            ((((WORD_TO_PTR((p)[2]))[0]) & 0xFFFFFF) > \
                              (((WORD_TO_PTR((p)[3]))[0]) & 0xFFFFFF) \
			      ? ((((WORD_TO_PTR((p)[2]))[0]) & 0xFFFFFF) + 1) \
                              : ((((WORD_TO_PTR((p)[3]))[0]) & 0xFFFFFF) + 1))
/* Define the balance of given node. The balance is 
   height of rigth son - height of left son. */     
#define BALANCE(p)            ((((WORD_TO_PTR((p)[3]))[0]) & 0xFFFFFF) - \
                              (((WORD_TO_PTR((p)[2]))[0]) & 0xFFFFFF))

/* Next four functions are "rotate" operations that balance the tree. 
 These functions are called when the AVL-property: the balance of every 
 node in the tree is -1, 0 or 1, violates. 

 In drawn examples below numbers 1, 2, 3 represents nodes and letters
 A, B, C and D subtrees. */


            
/*          2           1
 *         / \   =>    / \
 *        1   C       A   2
 *       / \             / \
 *      A   B           B   C
 */
static ptr_t simple_rotate_right(ptr_t p)
{
  ptr_t q, new_q;

  q = WORD_TO_PTR(p[2]);
  assert(q != NULL_PTR);

  p[2] = q[3];
  p[0] >>= 24;
  p[0] <<= 24;
  p[0] |= HEIGHT(p);

  new_q = allocate(4, CELL_avl_internal);
  new_q[1] = q[1];
  new_q[2] = q[2];
  new_q[3] = PTR_TO_WORD(p);
  new_q[0] |= HEIGHT(new_q);

  return new_q;
}

/*       1            2
 *      / \    =>    / \  
 *     A   2        1   C 
 *        / \      / \
 *       B   C    A   B
 */
static ptr_t simple_rotate_left(ptr_t p)
{
  ptr_t q, new_q;

  q = WORD_TO_PTR(p[3]);
  assert(q != NULL_PTR);
  
  p[3] = q[2];
  p[0] >>= 24;
  p[0] <<= 24;
  p[0] |= HEIGHT(p);

  new_q = allocate(4, CELL_avl_internal);
  new_q[1] = q[1];
  new_q[2] = PTR_TO_WORD(p);
  new_q[3] = q[3];
  new_q[0] |= HEIGHT(new_q);

  return new_q;
}

/*       3                   2
 *      / \                /   \
 *     1   D              1     3
 *    / \        =>      / \   / \
 *   A   2              A   B C   D
 *      / \
 *     B   C
 */
static ptr_t double_rotate_right(ptr_t p)
{
  ptr_t q, r, new_q, new_r;

  q = WORD_TO_PTR(p[2]);
  assert(q != NULL_PTR);
  r = WORD_TO_PTR(q[3]);
  assert(r != NULL_PTR);

  p[2] = r[3];
  p[0] >>= 24;
  p[0] <<= 24;
  p[0] |= HEIGHT(p);

  new_q = allocate(4, CELL_avl_internal);
  new_q[1] = q[1];
  new_q[2] = q[2];
  new_q[3] = r[2];
  new_q[0] |= HEIGHT(new_q);

  new_r = allocate(4, CELL_avl_internal);
  new_r[1] = r[1];
  new_r[2] = PTR_TO_WORD(new_q);
  new_r[3] = PTR_TO_WORD(p);
  new_r[0] |= HEIGHT(new_r);

  return new_r;
}

/*       1                   2 
 *      / \                /   \
 *     A   3              1     3
 *        / \     =>     / \   / \
 *       2   D          A   B C   D
 *      / \
 *     B   C
 */
static ptr_t double_rotate_left(ptr_t p)
{
  ptr_t q, r, new_q, new_r;

  q = WORD_TO_PTR(p[3]);
  assert(q != NULL_PTR);
  r = WORD_TO_PTR(q[2]);
  assert(r != NULL_PTR);

  p[3] = r[2];
  p[0] >>= 24;
  p[0] <<= 24;
  p[0] |= HEIGHT(p);

  new_q = allocate(4, CELL_avl_internal);
  new_q[1] = q[1];
  new_q[2] = r[3];
  new_q[3] = q[3];
  new_q[0] |= HEIGHT(new_q);

  new_r = allocate(4, CELL_avl_internal);
  new_r[1] = r[1];
  new_r[2] = PTR_TO_WORD(p);
  new_r[3] = PTR_TO_WORD(new_q);
  new_r[0] |= HEIGHT(new_r);

  return new_r;
}

/* Return the smallest item in tree pointed by `root'. */
ptr_t avl_find_min(ptr_t root)
{
  ptr_t p;

  if (root == NULL_PTR)
    return NULL_PTR;

  p = root;
  /* Traverse down the tree. Minimun is the first item in inorder traversal. */
  while (IS_INTERNAL_NODE(p))
  /* Always choose left son. */
    p = WORD_TO_PTR(p[2]);
  assert(CELL_TYPE(p) == CELL_avl_leaf);
  
  return WORD_TO_PTR(p[2]);
}

/* `root' points to an AVL tree. If data with key `key' exists in that
tree, return that data. Use function `item_cmp' to comparise `key'
against nodes in the tree. If there is no item with key `key' or for
some reason comparison with `item_cmp' doesn't succeed, return
NULL_PTR. */
ptr_t avl_find(ptr_t root, ptr_t key, cmp_fun_t item_cmp, void *context) 
{ 
  ptr_t p; 
  cmp_result_t cmp_result;

  if (root == NULL_PTR)
    /* Tree is empty. */
    return NULL_PTR;
  
  p = root;
  /* Traverse down the tree and find the place where item is. */
  while (IS_INTERNAL_NODE(p)) /* Items are stored in leaves. */{
    /* Compare `key' against key in `p'. */
    cmp_result = item_cmp(key, WORD_TO_PTR(p[1]), context);
    if (cmp_result == CMP_ERROR) { 
      /* For some reason the comparison didn't succeed. */
      fprintf(stderr, "avl_find: CMP_ERROR"); /*XXX*/
      return NULL_PTR;
    }
    if (cmp_result == CMP_LESS || cmp_result == CMP_EQUAL)
      /* Choose left son. */
      p = WORD_TO_PTR(p[2]);
    else
      /* Choose right son. */
      p = WORD_TO_PTR(p[3]);
  }
  assert(CELL_TYPE(p) == CELL_avl_leaf);

  /* Now `p' points to leaf. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[1]), context);
  if (cmp_result != CMP_EQUAL)
    /* Item with key `key' doesn't exist. */
    return NULL_PTR;
  else
    return WORD_TO_PTR(p[2]);
}

/* `root points to an AVL tree. When the insertion point is found, update 
   `data' using `update_fun'. 

   If `key' doesn't exist in the tree, insert `data' with key `key'. 

   If `key' already exists in that tree:
   
   a) If `data' and data in tree with key `key' are physically same, do
   nothing: restore allocation point and return original root `root'.
   
   b) Else update existing data to `data'. 
   
   Use function `item_cmp' to comparise `key' against nodes in the
   tree. If for some reason comparison with `item_cmp' doesn't succeed
   restore allocation point and return original root `root'. */
ptr_t avl_insert(ptr_t root, ptr_t key,
		 cmp_fun_t item_cmp, void *context,
                 ptr_t (*update_fun)(ptr_t, ptr_t), ptr_t data)
{
  ptr_t p, new_p, ap, item;
  int level = 0, balance, balance_of_son;
  cmp_result_t cmp_result;
  struct {
    ptr_t node;
    int son;
  } path[50];

  ap = get_allocation_point();
  
  if (root == NULL_PTR) {
    if (update_fun != NULL)
      data = update_fun(root, data);
    root = allocate(3, CELL_avl_leaf);
    root[1] = PTR_TO_WORD(key);
    root[2] = PTR_TO_WORD(data);
    return root; 
  }

  p = root;

  /* Traverse down the tree pointed by `root' and find the place where
     to insert given data `data' with key `key'. While traversing,
     store the path in table `path'. That's because after insertion we
     must go back upwards to check, if the tree has still the
     AVL-property: in all nodes p |balance(p)| < 2. */
  while(IS_INTERNAL_NODE(p)) {

    new_p = allocate(4, CELL_avl_internal);
    new_p[0] = p[0];
    new_p[1] = p[1];
    new_p[2] = p[2];
    new_p[3] = p[3];

    /* Append the new node to the vector `path'. */
    path[level].node = new_p;
    /* Unless `new_p' is the new root, chain `new_p' to be a child of
       the previously constructed node. */
    if (level > 0)
      /* `path[level - 1].node' is the previously constructed node and
	 `path[level - 1].son' the index to get to next node. */
      path[level - 1].node[path[level - 1].son] = PTR_TO_WORD(new_p);

    cmp_result = item_cmp(key, WORD_TO_PTR(p[1]), context);
    if (cmp_result == CMP_ERROR) {
      /* For some reason the comparison didn't succeed. */
      restore_allocation_point(ap);
      return root;
    }
    if (cmp_result == CMP_LESS || cmp_result == CMP_EQUAL) {
      /* Go to left subtree. */
      p = WORD_TO_PTR(p[2]);
      path[level].son = 2;
    } else {
      /* Go to right subtree. */
      p = WORD_TO_PTR(p[3]);
      path[level].son = 3;
    }
    level++;
  }
  assert(CELL_TYPE(p) == CELL_avl_leaf);
 
  cmp_result = item_cmp(key, WORD_TO_PTR(p[1]), context);
  if (cmp_result == CMP_ERROR) { 
    /* For some reason the comparison didn't succeed. */
    restore_allocation_point(ap);
    return root;
  }

  if (update_fun != NULL)
    data = update_fun(WORD_TO_PTR(p[2]), data);

  if (cmp_result == CMP_EQUAL) {
    /* Key `key' already exists in AVL_tree. */
    if (p[2] == PTR_TO_WORD(data)) {
      /* The insertion is nilpotent: retract the work and return the
	 original root. */
      restore_allocation_point(ap);
      return root;
    } else {
      /* Update data in `p' to `data'. */
      p = allocate(3, CELL_avl_leaf);
      p[1] = PTR_TO_WORD(key);
      p[2] = PTR_TO_WORD(data);
      /* Because we only updated existing node and so no path got
         longer/shorter, the tree still has AVL-property. We can now
         safely return without checking balances of nodes in
         `path'. */
      if (level > 0) {
	/* `path[level - 1].node' is the previously constructed node and
	   `path[level - 1].son' the index to get to the new node. */
	path[level - 1].node[path[level - 1].son] = PTR_TO_WORD(p);
	return path[0].node;
      } else
	return p;
    }
  }

  /* Data with key `key' doesn't exist in the tree. Insert new item
     in the tree. */

  /* Allocate new leaf node where to store given data `data' and key `key'. */
  item = allocate(3, CELL_avl_leaf);
  item[1] = PTR_TO_WORD(key);
  item[2] = PTR_TO_WORD(data);
  /* Allocate new internal node `new_p' which sons are `p' and `item'. */
  new_p = allocate(4, CELL_avl_internal);
  new_p[0] |= 1;
  if (cmp_result == CMP_LESS) {
    new_p[1] = PTR_TO_WORD(key);
    new_p[2] = PTR_TO_WORD(item);
    new_p[3] = PTR_TO_WORD(p);
  } else {
    new_p[1] = p[1];
    new_p[2] = PTR_TO_WORD(p);
    new_p[3] = PTR_TO_WORD(item);
  }

  if (level > 0)
    /* Chain `new_p' to be son of the previously constructed internal node. 
       `path[level - 1].node' is the previously constructed node and
       `path[level - 1].son' the index to get to the new node. */
    path[level - 1].node[path[level - 1].son] = PTR_TO_WORD(new_p);
  else
    return new_p;

  /* Traverse upwards along `path'. Check if tree has
     AVL-property. If in some node `p' |balance(p)| > 1, do one of
     four rotate operations and return new root. We can return because
     in insertion we need only one rotate operation to have the
     AVL-property again. */
  while (level > 0) {
    level--;
    p = path[level].node;
    balance = BALANCE(p);
    if (balance < -1) {
      balance_of_son = BALANCE(WORD_TO_PTR(p[2]));
      if (balance_of_son == -1) 
	p = simple_rotate_right(p);
      else if (balance_of_son == 1)
	p = double_rotate_right(p);
      if (level == 0)
	return p;
      path[level - 1].node[path[level - 1].son] = PTR_TO_WORD(p); 
      return path[0].node;
    } else if (balance > 1) {
      balance_of_son = BALANCE(WORD_TO_PTR(p[3]));
      if (balance_of_son == 1)
	p = simple_rotate_left(p);
      else if (balance_of_son == -1)
	p = double_rotate_left(p);
      if (level == 0)
	return p;
      path[level - 1].node[path[level - 1].son] = PTR_TO_WORD(p);
      return path[0].node;
    }
    p[0] >>= 24;
    p[0] <<= 24;
    p[0] |= HEIGHT(p);
  }
  
  return path[0].node;

}


/* `root' is a pointer to AVL tree. If data with key `key' doesn't
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
ptr_t avl_delete(ptr_t root, ptr_t key,
		 cmp_fun_t item_cmp, void *context,
                 ptr_t (*delete_fun)(ptr_t, ptr_t), ptr_t data)
{
  ptr_t ap, p, new_p;
  int level = 0, ix, balance, balance_of_son;
  cmp_result_t cmp_result;
  struct {
    ptr_t node;
    int son;
  } path[50];

  ap = get_allocation_point();
  
  if (root == NULL_PTR) 
    return NULL_PTR; 

  p = root;

  /* Traverse down the tree pointed by `root' and find the place where
     data with key `key' exists. While traversing,
     store the path in table `path'. That's because after deletion we
     must go back upwards to check, if the tree has still the
     AVL-property: in all nodes p |balance(p)| < 2. */
  while(IS_INTERNAL_NODE(p)) {
    
    new_p = allocate(4, CELL_avl_internal);
    new_p[0] = p[0];
    new_p[1] = p[1];
    new_p[2] = p[2];
    new_p[3] = p[3];

    /* Append the new node to the vector `path'. */
    path[level].node = new_p;
    /* Unless `new_p' is the new root, chain `new_p' to be a child of
       the previously constructed node. */
    if (level > 0)
      /* `path[level - 1].node' is the previously constructed node and
	 `path[level - 1].son' the index to get to the new node. */
      path[level - 1].node[path[level - 1].son] = PTR_TO_WORD(new_p);

    cmp_result = item_cmp(key, WORD_TO_PTR(p[1]), context);
    if (cmp_result == CMP_ERROR) {
      /* For some reason the comparison didn't succeed. */
      restore_allocation_point(ap);
      return root;
    }
    if (cmp_result == CMP_LESS || cmp_result == CMP_EQUAL) {
      /* Go to left subtree. */
      p = WORD_TO_PTR(p[2]);
      path[level].son = 2;
    } else {
      /* Go to right subtree. */
      p = WORD_TO_PTR(p[3]);
      path[level].son = 3;
    }
    level++;
  }
  assert(CELL_TYPE(p) == CELL_avl_leaf);

  /* Now `p' should point to leaf. Check if data with key `key' exists. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[1]), context);
  if (cmp_result != CMP_EQUAL) {
    /* Item with key `key' doesn't exist in AVL-tree. */
    restore_allocation_point(ap);
    return root;
  }

  if (delete_fun != NULL) {
    data = delete_fun(WORD_TO_PTR(p[2]), data);
    if (p[2] == PTR_TO_WORD(data)) {
      /* The insertion is nilpotent: retract the work and return the
	 original root. */
      restore_allocation_point(ap);
      return root;
    }
    /* Update data behind the key. */
    new_p = allocate(3, CELL_avl_leaf);
    new_p[1] = p[1];
    new_p[2] = PTR_TO_WORD(data);
    if (level > 0) {
      path[level - 1].node[path[level - 1].son] = PTR_TO_WORD(new_p);
      /* There is no need for rotate operations because balance of any node
	 in path didn't change. */
      return path[0].node;
    } else 
      return new_p;
  } 

  /* (Otherwise) delete the key. 
   *
   *       1       
   *      / \       =>         3       
   *     1   3   key == 1
   */
  if (level > 0) {
    level--;
    p = path[level].node;
    /* If `p[2]' is the leaf that must be deleted, then `ix' == 3.
     If `p[3]' is the leaf that must be deleted, then `ix' == 2. */ 
    ix = (path[level].son + 1) % 2 + 2;
    if (level > 0) 
      path[level - 1].node[path[level - 1].son] = p[ix];
    else 
      return WORD_TO_PTR(p[ix]);
  } else 
    return NULL_PTR;

  /* Traverse upwards along `path'. Check if tree has AVL-property. If
     in some node `p' |balance(p)| > 1, do one of four rotate
     operations, else go one node upwards without doing anything. */
  while (level > 0) {
    level--;
    p = path[level].node;
    balance = BALANCE(p);
    if (balance < -1) {
      balance_of_son = BALANCE(WORD_TO_PTR(p[2]));
      if (balance_of_son == 1)
	p = double_rotate_right(p);
      else 
	p = simple_rotate_right(p);
      if (level > 0)
	path[level - 1].node[path[level - 1].son] = PTR_TO_WORD(p); 
    } else if (balance > 1) {
      balance_of_son = BALANCE(WORD_TO_PTR(p[3]));
      if (balance_of_son == -1)
	p = double_rotate_left(p);
      else 
	p = simple_rotate_left(p);
      if (level > 0)
	path[level - 1].node[path[level - 1].son] = PTR_TO_WORD(p);
    } else {
      p[0] >>= 24;
      p[0] <<= 24;
      p[0] |= HEIGHT(p);
    }
  }
  return path[0].node;
}

/* Insert `node' in history. Also store information that from node
   `node' path from root to leaf goes to left. Return new history. */
ptr_t avl_bc_delve_left(ptr_t history, ptr_t node)
{
  ptr_t new_history;
  
  new_history = allocate(3, CELL_history);
  new_history[1] = PTR_TO_WORD(node);
  /* Store in history-node information that from `node' byte code goes to
     left. */
  new_history[0] |= 2;
  /* Insert allocated node to `history'. */
  new_history[2] = PTR_TO_WORD(history);

  return new_history;
}

/* Insert `node' in history. Also store information that from node
   `node' path from root to leaf goes to right. Return new
   history. */
ptr_t avl_bc_delve_right(ptr_t history, ptr_t node)
{
  ptr_t new_history;

  new_history = allocate(3, CELL_history);
  new_history[1] = PTR_TO_WORD(node);
  /* Store to history information that from `node'  byte code goes to right. */
  new_history[0] |= 3;
  /* Insert allocated node to `history'. */
  new_history[2] = PTR_TO_WORD(history);

  return new_history;
}

/* NOTICE! This function is called just before `avl_bc_insert', when
the key is same as the key to be inserted e.g key already exists in
the tree. */
/* Insert `node' in history. Also store
information that the key of `node' is same as key to be inserted
later. */ 
ptr_t avl_bc_delve_equal(ptr_t history, ptr_t node) 
{ 
  ptr_t new_history;

  new_history = allocate(3, CELL_history);
  new_history[1] = PTR_TO_WORD(node);
  /* Store to history information that from `node'  byte code goes nowhere. */
  new_history[0] |= 0;
  /* Insert allocated node to `history'. */
  new_history[2] = PTR_TO_WORD(history);

  return new_history;
}

/* Insert `data' with key `key' into AVL tree. First item in `history'
   is leaf node e.g. insertion point. Allocate new internal node which
   sons are that leaf and `data' with `key'. Because destructive
   updates are not allowed update fathers stored in `history'
   recursively (in the while-loop) to point newly allocated son. At
   the same time check that AVL-property still exists. */
ptr_t avl_bc_insert(ptr_t history, ptr_t key, ptr_t data)
{
  ptr_t new_path, new_path_item, p;
  int ix, balance, balance_of_son;
  
  /* Allocate new leaf node where to store given data `data' and key
     `key'. */
  new_path_item = allocate(3, CELL_avl_leaf);
  new_path_item[1] = PTR_TO_WORD(key);
  new_path_item[2] = PTR_TO_WORD(data);
  
  if (history == NULL_PTR)
    return new_path_item;

  p = WORD_TO_PTR(history[1]);
  ix = history[0] & 0xF;

  if (ix == 0) 
    /* Key exists in the AVL-tree. */
    new_path = new_path_item;
  else {
    /* Allocate new internal node `new_path' which sons are `p' and
       `new_path_item'. */
    new_path = allocate(4, CELL_avl_internal);
    new_path[0] |= 1;
    if (ix == 2) {
      new_path[1] = PTR_TO_WORD(key);
      new_path[2] = PTR_TO_WORD(new_path_item);
      new_path[3] = PTR_TO_WORD(p);
    } else {
      new_path[1] = p[1];
      new_path[2] = PTR_TO_WORD(p);
      new_path[3] = PTR_TO_WORD(new_path_item);
    }
  }
  history = WORD_TO_PTR(history[2]);
  /* Traverse upwards along `history'. Construct new path `new_path'
     while traversing.  Check that every node in new path has
     AVL-property. If |balance(new_path)| > 1, do one of four rotate
     operations, else insert an item from `history' to `new_path' as
     normal. */
  while (1) { 
    if (history != NULL_PTR) {
      p = WORD_TO_PTR(history[1]);
      ix = history[0] & 0xF;
      new_path_item = allocate(4, CELL_avl_internal);
      new_path_item[1] = p[1];
      new_path_item[ix] = PTR_TO_WORD(new_path);
      new_path_item[(ix + 1) % 2 + 2] = p[(ix + 1) % 2 + 2];
      new_path_item[0] |= HEIGHT(new_path_item);
      new_path = new_path_item;
      history = WORD_TO_PTR(history[2]);
    } else
      return new_path;
    balance = BALANCE(new_path);
    if (balance < -1) {
      balance_of_son = BALANCE(WORD_TO_PTR(new_path[2]));
      if (balance_of_son == -1)
	new_path = simple_rotate_right(new_path);
      else if (balance_of_son == 1)
	new_path = double_rotate_right(new_path);
    } else if (balance > 1) {
      balance_of_son = BALANCE(WORD_TO_PTR(new_path[3]));
      if (balance_of_son == 1)
	new_path = simple_rotate_left(new_path);
      else if (balance_of_son == -1)
	new_path = double_rotate_left(new_path);
    }
  }
}

/* Delete son of first item in `history'. Check recursively (in the
   while-loop) that fathers stored in `history' still have
   AVL-property. */
ptr_t avl_bc_delete(ptr_t history) 
{
  ptr_t new_path, new_path_item, p;
  int ix, balance, balance_of_son;
  
  if (history == NULL_PTR) 
    /* History is empty. Before deletion AVL tree is singleton node. */
    return NULL_PTR;
  
  p = WORD_TO_PTR(history[1]);
  /* If `p[2]' is the leaf that must be deleted, then `ix' = 3.
     If `p[3]' is the leaf that must be deleted, then `ix' = 2. */ 
  ix = ((history[0] & 0xF) + 1) % 2 + 2;
  
  /* Delete, e.g. replace father by its only son. 
   * 
   *       2 (<= first item of `history')      
   *      / \                               =>         3       
   *     1   3                           key == 1 
   */
  new_path = WORD_TO_PTR(p[ix]);
  history = WORD_TO_PTR(history[2]);
  
  if (history == NULL_PTR)
    return new_path;
  
  p = WORD_TO_PTR(history[1]);
  ix = history[0] & 0xF;
  
  new_path_item = allocate(4, CELL_avl_internal);
  new_path_item[1] = p[1];
  new_path_item[ix] = PTR_TO_WORD(new_path);
  new_path_item[(ix + 1) % 2 + 2] = p[(ix + 1) % 2 + 2];
  new_path_item[0] |= HEIGHT(new_path_item);
  
  new_path = new_path_item;
  history = WORD_TO_PTR(history[2]);
  /* Traverse upwards along `history'. Construct new path `new_path'
     while traversing.  Check that every node in new path has
     AVL-property. If |balance(new_path)| > 1, do one of four rotate
     operations, else insert an item from `history' to `new_path' as
     normal. */
  while (1) {
    balance = BALANCE(new_path);
    if (balance < -1) {
      balance_of_son = BALANCE(WORD_TO_PTR(new_path[2]));
      if (balance_of_son == 1)
	new_path = double_rotate_right(new_path);
      else 
	new_path = simple_rotate_right(new_path);
    } else if (balance > 1) {
      balance_of_son = BALANCE(WORD_TO_PTR(new_path[3]));
      if (balance_of_son == -1)
	new_path = double_rotate_left(new_path);
      else 
	new_path = simple_rotate_left(new_path);
    }
    if (history != NULL_PTR) {
      p = WORD_TO_PTR(history[1]);
      ix = history[0] & 0xF;

      new_path_item = allocate(4, CELL_avl_internal);
      new_path_item[1] = p[1];
      new_path_item[ix] = PTR_TO_WORD(new_path);
      new_path_item[(ix + 1) % 2 + 2] = p[(ix + 1) % 2 + 2];
      new_path_item[0] |= HEIGHT(new_path_item);

      new_path = new_path_item;
      history = WORD_TO_PTR(history[2]);
    } else
      return new_path;
  }
}

/* This is for test purpose. Return history. */
ptr_t build_history_for_insert(ptr_t root, 
			       ptr_t key, 
			       cmp_fun_t item_cmp, void *context,
			       int *result)
{
  ptr_t p, history = NULL_PTR;
  cmp_result_t cmp_result;

  if (root == NULL_PTR) 
    return NULL_PTR;

  p = root;
  while(IS_INTERNAL_NODE(p)) {

    cmp_result = item_cmp(key, WORD_TO_PTR(p[1]), context);
    if (cmp_result == CMP_ERROR) {
      fprintf(stderr, "\nbuild_history_for_insert: CMP_ERROR");
      *result = 0;
      return NULL_PTR;
    }
    if (cmp_result == CMP_LESS || cmp_result == CMP_EQUAL) {
      /* Insert `p' in `history'. */
      history = avl_bc_delve_left(history, p);
      /* Go to left subtree. */
      p = WORD_TO_PTR(p[2]);
    } else {
      /* Insert `p' in `history'. */
      history = avl_bc_delve_right(history, p);
      /* Go to right subtree. */
      p = WORD_TO_PTR(p[3]);
    }
  }
  assert(CELL_TYPE(p) == CELL_avl_leaf);
 
  cmp_result = item_cmp(key, WORD_TO_PTR(p[1]), context);
  if (cmp_result == CMP_ERROR) {
    fprintf(stderr, "\nbuild_history_for_insert: CMP_ERROR");
    *result = 0;
    return NULL_PTR;
  }

  if (cmp_result == CMP_EQUAL) {
    fprintf(stderr, "\navl_bc_delve_equal");
    /* Key `key' already exists in AVL_tree. */
    history = avl_bc_delve_equal(history, p);
    return history;
  }

  /* Data with key `key' doesn't exist in the tree. Insert `p' in
     `history'. */
  if (cmp_result == CMP_LESS) 
    history = avl_bc_delve_left(history, p);
  else 
    history = avl_bc_delve_right(history, p);

  return history;
}

ptr_t build_history_for_delete(ptr_t root, 
			       ptr_t key, 
			       cmp_fun_t item_cmp, void *context,
			       int *result)
{
  ptr_t p, history = NULL_PTR;
  cmp_result_t cmp_result;
  
  if (root == NULL_PTR) 
    return NULL_PTR; 

  p = root;

  while(IS_INTERNAL_NODE(p)) {
    
    cmp_result = item_cmp(key, WORD_TO_PTR(p[1]), context);
    if (cmp_result == CMP_ERROR) {
      fprintf(stderr, "\nbuild_history_for_delete: CMP_ERROR");
      *result = 0;
      return NULL_PTR;
    }
    if (cmp_result == CMP_LESS || cmp_result == CMP_EQUAL) {
      /* Insert `p' in `history'. */
      history = avl_bc_delve_left(history, p);
      /* Go to left subtree. */
      p = WORD_TO_PTR(p[2]);
    } else {
      /* Insert `p' in `history'. */
      history = avl_bc_delve_right(history, p);
      /* Go to right subtree. */
      p = WORD_TO_PTR(p[3]);
    }
  }
  assert(CELL_TYPE(p) == CELL_avl_leaf);

  /* Now `p' should point to leaf. Check if data with key `key' exists. */
  cmp_result = item_cmp(key, WORD_TO_PTR(p[1]), context);
  if (cmp_result != CMP_EQUAL) {
    /* Item with key `key' doesn't exist in AVL-tree. */
      *result = 0;
      return NULL_PTR;
  }

  return history;
}


static void avl_rec_make_assertions(ptr_t node)
{
  int balance;

  assert(node != NULL_PTR);
  if (IS_INTERNAL_NODE(node)) {
    avl_rec_make_assertions(WORD_TO_PTR(node[2]));
    avl_rec_make_assertions(WORD_TO_PTR(node[3]));
    assert(HEIGHT(node) <= 50);
    assert((node[0] & 0xFFFFFF) <= 50);
    assert(HEIGHT(node) == (node[0] & 0xFFFFFF));
    balance = BALANCE(node);
    assert((balance > -2) && (balance < 2));
  } else {
    assert(CELL_TYPE(node) == CELL_avl_leaf);
    assert((node[0] & 0xFFFFFF) == 0);
  }
  return;
}


void avl_make_assertions(ptr_t root)
{
  if (root == NULL_PTR) {
   fprintf(stderr, "\nEmpty AVL tree");
   return;
  } 
  avl_rec_make_assertions(root);
  return;
}
