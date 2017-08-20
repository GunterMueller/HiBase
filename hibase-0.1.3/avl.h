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

/* This is one version of AVL tree. The tree consists of two types of
 * nodes: `CELL_avl_internal' for internal nodes and `CELL_avl_leafs' 
 * for external nodes e.g. leaves.  
 *
 * If type of `p' is CELL_avl_internal, then:
 *
 * p[0]:   8 highest bit are typetag and 24 lowest the height of `p'
 * p[1]:   A pointer to key. Usually key is `CELL_word_vector'.
 * p[2]:   A pointer to left son. 
 * p[3]:   A pointer to right son
 *
 * If type of `p' is CELL_avl_leaf, then:
 *
 * p[0]:   8 highest bit are typetag and 24 lowest bits are 0s because
 *         the height of leaf is always 0.
 * p[1]:   See p[1] in `CELL_avl_internal'. Keys must be same type.
 * p[2]:   A pointer to data. So this AVL tree may contain data with the key.
 *
 * Because this AVL tree contains data and keys, it spends more memory
 * than version with only keys as data. On the other hand this AVL
 * tree can be used in more situations than traditional AVL tree.
 * 
 * For more information of AVL trees, see for example 
 * Introduction to Algorithms of Cormen, Leiserson, Rivest.
 *
 */
#define  IS_INTERNAL_NODE(p)  (CELL_TYPE(p) == CELL_avl_internal)  

#define AVL_IS_EMPTY(root)  ((root) == NULL_PTR)

/* Return the smallest item in tree pointed by `root'. */
ptr_t avl_find_min(ptr_t root);

/* `root' points to an AVL tree. If item with key `key' exists in that
   tree, return that item. Use function `item_cmp' to comparise `key'
   against nodes in the tree. If there is no item with key `key' or
   for some reason comparison with `item_cmp' doesn't succeed, return
   NULL_PTR. */
ptr_t avl_find(ptr_t root, ptr_t key, cmp_fun_t item_cmp, void *context); 

/* Let T be an AVL tree. It can be proved that number of leaves in T
 * is approx. 1.170*1.618^h, where h is the height of T. Then because
 * number of nodes n > number of leaves: 
 * 
 * n > 1.170*1.618^h    =>
 * n > 1.618^h          =>
 * h < log_(1.618) n    => 
 * h < log_(1.618) 2^32 =>
 * h < 48
 * 
 * Amount of `CELL_avl's that insertion needs in worst-case:
 *
 *  48           Construct path from root to tree              
 *   1           New internal node
 *   2           Make one rotation
 *  --
 *  51
 *
 * Amount of `CELL_avl_item's that insertion needs is 1 (new item).
 *
 * => Total amount of memory in `word_t's is 51*4 + 1*3 = 207 during
 * inserting operation.
 *   
 * Amount of `CELL_avl's that deletion needs in worst-case:
 *
 *   48           Construct path from root to tree              
 *    1           New internal node
 *   96           Make h rotations
 *  ---
 *  145
 *
 * Amount of `CELL_avl_item's that deletion needs is 1 (new item).
 *
 * => Total amount of memory in `word_t's is 145*4 + 1*3 = 583 during
 * delete operation.
 *
 * 
 *  
 */
#define AVL_MAX_ALLOCATION_IN_INSERT  (207)
#define AVL_MAX_ALLOCATION_IN_DELETE  (583)

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
                 ptr_t (*update_fun)(ptr_t, ptr_t), ptr_t data); /* vrt. trie*/

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
                 ptr_t (*delete_fun)(ptr_t, ptr_t), ptr_t data);

/*ptr_t AVL_BC_GET_KEY(node)  */

/* delve-functions traverse from the root of AVL-tree to the leaf -
   to the next insertion/deletion point. The path is stored into
   `history' while traversing.

   If you want to insert a key then insert into history also the leaf
   e.g. insertion point. If key is smaller than key in the leaf,
   insert leaf into history with `delve_left', else insert leaf with
   `delve_right'. After this last delve-function, `history' has knowledge
   enough for inserting the key.

   If you want to delete some leaf, store only internal nodes of the
   path into history. If the key that will be deleted is the left son
   of the last internal node then insert that last internal node into
   history with `delve_left', otherwise insert it with `delve_right'. */ 

/* Amount of memory that delve operations need is one CELL_history. */
#define AVL_BC_DELVE_MAX_ALLOCATION  (3)


/* `history' is a list of `CELL_history's. 
   if CELL_TYPE(p) = CELL_history then
   
   lowest 
*/

/* Insert `node' in history. Also store information that from node
   `node' path from root to leaf goes to left. Return new history. */ 
ptr_t avl_bc_delve_left(ptr_t history, ptr_t node); 
/* Insert `node' in history. Also store information that from node
   `node' path from root to leaf goes to right. Return new
   history. */
ptr_t avl_bc_delve_right(ptr_t history, ptr_t node);


/* NOTICE! This function is called just before `avl_bc_insert', when
the key is same as the key to be inserted e.g key already exists in
the tree. */
/* Insert `node' in history. Also store
information that the key of `node' is same as key to be inserted
later. */ 
ptr_t avl_bc_delve_equal(ptr_t history, ptr_t node); 

/* Return new root. Amounts of allocation are same as in 
   `avl_insert' and `avl_delete'. */
ptr_t avl_bc_insert(ptr_t history, ptr_t key, ptr_t data);
ptr_t avl_bc_delete(ptr_t history); 

/* For testing. */
void avl_make_assertions(ptr_t root);
ptr_t build_history_for_delete(ptr_t root, ptr_t key, 
			       cmp_fun_t item_cmp, void *context, int *result);
ptr_t build_history_for_insert(ptr_t root, ptr_t key, 
			       cmp_fun_t item_cmp, void *context, int *result);













