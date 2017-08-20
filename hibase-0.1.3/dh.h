/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 */

/* Dynamic hash operations 
 */

#ifndef INCL_DH_H
#define INCL_DH_H 1


#include "includes.h"
#include "shades.h"

#define INTERNAL_NODE_SIZE 4

#define MAX_KEYS_IN_LEAF  32
/* In dynamic hashing, data structure is d-ary tree with arbitrary
 * sized leaf cells. 
 *
 * Thus, dynamic hash tree has types of nodes: internal nodes,
 * which just contain links to other nodes, and external nodes
 * e.g. leaves, which contain keys and links to datas with that key.
 *
 * Internal nodes are of size d + 1. d is referred as and defined in
 * `INTERNAL_NODE_SIZE' (see `cells-def.h).  Type of internal node is
 * `CELL_internal'.
 * 
 * Leaf cells contain only hash values e.g. keys and pointers to data
 * with that key. There are at most `MAX_KEYS_IN_LEAF' keys per
 * leaf. Type of leaf cell is `CELL_leaf'. Number of keys in leaf `p'
 * is stored in 3 lowest bytes of p[0]. Size of `p' is 2 * (p[0] *
 * 0xFFFFFF) + 1. */

/* Let d be `INTERNAL_NODE_SIZE'. Then `DH_MAX_ALLOCATION' is 
 *  
 *      (d + 1) * h + `MAX_CELL_SIZE', where h (height of tree) = 32 / log d 
 *   => `DH_MAX_ALLOCATION' = (d + 1) * 32 / log d + `MAX_KEYS_IN_LEAF'.
 * 
 *  
 * Let's calculate it, first without `MAX_KEYS_IN_LEAF'. Let's
 * assume that d = 2, 4, 8, or 16. (It can be shown that those are
 * optimal size of d in most circumstancies. Theoretically d can be
 * any power of two between 2 and 2^32.)
 * 
 *
 *   (d + 1) * 32 / log d = 
 *
 *    a) (2 + 1) * 32 / 1  =  96, when d = 2
 *    b) (4 + 1) * 32 / 2  =  80, when d = 4
 *    c) (8 + 1) * 32 / 3  =  99, when d = 8
 *    d) (16 + 1) * 32 / 4 = 136, when d = 16
 *  
 * As we can see, when d is 16 or bigger result grows dramatically. 
 * Thus, the recommended size of d is 2, 4, or 8.
 *
 * What's optimal size of `MAX_KEYS_IN_LEAF'?
 * 
 * I don't know the answer (yet). I suppose 4. 

 * So the maximum allocation is */
#define  DH_MAX_ALLOCATION ((((INTERNAL_NODE_SIZE + 1) * 32) / (DO_ILOG2(INTERNAL_NODE_SIZE) - 1)) + (4 * 2) + 1)

/*                     Notice!
 *
 * When you want to change `INTERNAL_NODE_SIZE', you have to remember,
 * that ´it must be power of two. Prefer numbers 2, 4, 8, or 16.  
*/

 /* Return smallest item. */
ptr_t dh_find_min(ptr_t root, word_t *key_p);

/* Return data behind key `key' if exists, else NULL_PTR. */
ptr_t dh_find(ptr_t root, word_t key);

/* Insert `data' with key `key', if doesn't exist. Return new_root. */
ptr_t dh_insert(ptr_t root, word_t key, ptr_t (*f)(ptr_t, ptr_t), ptr_t data);

/* Delete data with key `key'. Return new root. */
ptr_t dh_delete(ptr_t root, word_t key);

#endif /* INCL_DH_H */




