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

/* This is one version of 2-3-4 tree. The tree consists of
 * `CELL_ist234's: 
 *
 * If p[0] & 0xFFFFFF == 2, then it contains two pointers to
 * subtrees and one key:
 *
 * p[0]:   8 highest bits are typetag. 
 * p[1]:   A pointer to left subtree.
 * p[2]:   A pointer to key. Usually key is `CELL_word_vector'.
 * p[3]:   A pointer to data assosiated to key.
 * p[4]:   A pointer to right subtree.
 *
 * If p[0] & 0xFFFFFF == 3, then it contains three pointers to
 * subtrees and two keys:
 *
 * p[0]:   8 highest bits are typetag. 
 * p[1]:   A pointer to 1st subtree.
 * p[2]:   A pointer to 1st key.
 * p[3]:   A pointer to data assosiated to 1st key.
 * p[4]:   A pointer to 2nd subtree.
 * p[5]:   A pointer to 2nd key. 
 * p[6]:   A pointer to data assosiated to 2nd key.
 * p[7]:   A pointer to 3rd subtree.
 *
 * If p[0] & 0xFFFFFF == 4, then it contains four pointers to
 * subtrees and three keys:
 *
 * p[0]:   8 highest bits are typetag. 
 * p[1]:   A pointer to 1st subtree.
 * p[2]:   A pointer to 1st key.
 * p[3]:   A pointer to data assosiated to 1st key.
 * p[4]:   A pointer to 2nd subtree.
 * p[5]:   A pointer to 2nd key.
 * p[6]:   A pointer to data assosiated to 2nd key .
 * p[7]:   A pointer to 3rd subtree.
 * p[8]:   A pointer to 3rd key.
 * p[9]:   A pointer to data assosiated to 3rd key. 
 * p[10]:  A pointer to 4th subtree.
 *
 * General rule:
 *
 * Let i be index of a key in node `p' of `CELL_ist234'. Then:
 * i+1 denotes data associated to the key.
 * i-1   -"-   the left-hand subtree, with smaller or equal keys
 * i+2   -"-   the right-hand subtree, with greater keys
 *
 * Because this 2-3-4 tree contains data and keys, it spends more
 * memory than version, whose only data is key. On the other hand this
 * kind of a version can be used in more situations than
 * traditional 2-3-4 tree.
 * 
 * For more information of 2-3-4 trees, see for example 
 * Sedgewick's Algortithms (Addison-Wesley 1988).
 * */
#define ist234_IS_EMPTY(root)  ((root) == NULL_PTR)

/* Return smallest key in the tree, or NULL_PTR if the tree is empty. */
ptr_t ist234_find_min(ptr_t root);
 
/* `root' points to an 2-3-4 tree. If data with key `key' exists in the
   tree, return the data. Use function `item_cmp' to comparise `key'
   against keys of the nodes. If the `key' doesn't exist in the tree, or
   for some reason comparison with `item_cmp' doesn't succeed, return
   NULL_PTR. */
ptr_t ist234_find(ptr_t root, ptr_t key, cmp_fun_t item_cmp, void *context); 

/* Let T be an 2-3-4 tree.  It can be easily proved the distance from
 * any leaf to the root is same for all leaves.  Let d denote the
 * distance, n the number of nodes in T, and N the number of keys in
 * T. Notice that there are 1-3 keys per node in T. Therefore:
 *    
 *  n <= N <= 3*n. 
 *
 * 
 * N <= 2^32 in our 32-bit system => n <= 2^32.
 * 
 * d <= log n 
 * => d <= 32
 * 
 * => Amount of nodes that insert, or delete operation needs to
 * allocate is at most 32 * 11 (max size of `CELL_ist234') == 352.  */
#define IST234_MAX_ALLOCATION_IN_INSERT  (352)
#define IST234_MAX_ALLOCATION_IN_DELETE  (3*352)
/* `root points to an 2-3-4 tree. If `key' already exists in that
   tree:

   If key already exits:

   a) Update existing data to `data'. 

   b) If `data' and data in tree with key `key' are physically same, do
   nothing: return initial root.

   If `key' doesn't exist, insert `data' with key `key' as normal. 

   Use function `item_cmp' to comparise `key' against nodes in the
   tree. If for some reason comparison with `item_cmp' doesn't succeed
   return original root. */
ptr_t ist234_insert(ptr_t root, ptr_t key,
		 cmp_fun_t item_cmp, void *context,
                 ptr_t (*update_fun)(ptr_t, ptr_t), ptr_t data); 

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
                 ptr_t (*delete_fun)(ptr_t, ptr_t), ptr_t data);

