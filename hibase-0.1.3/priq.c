/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 *
 * Priority queue operations. The priority queue implemented here is
 * binomial heap.
 *
 * For more information of destructive binomial heap see Cormen,
 * Leiserson, Rivest: Introduction to Algotrithms, The MIT Press,
 * (1991), pages 400-416.
 *
 * For more information of this non destructive implementation 
 * see author's Thesis Tietorakenteiden tietoa tuhoamaton 
 * toteutus, Helsingin yliopisto, 1998.  
 */

#include "includes.h"
#include "priq.h"

static char *rev_id = "$Id: priq.c,v 1.22 1998/03/02 15:45:34 sirkku Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

/* Binomial heap is a list of heap ordered binomial trees.

   Binomial trees are inductively defined as follows: 
   o A binomial tree of height 0 is a singleton node.  
   o A binomial tree of height h + 1 is formed by linking two
     binomial trees of height h, making one tree the rightmost
     child of the other. 

     Binomial trees consists of `CELL_priq_item's 

       DECLARE_PTR(p[1]);          Item. 
       DECLARE_PTR(p[2]);          The rightmost son.
       DECLARE_PTR(p[3]);          Pointer to the next tree. 
*/

#define HEIGHT(p)  ((p)[0] & 0xFFFFFF)
#define ITEM(p)  WORD_TO_PTR((p)[1])
#define SON(p)  WORD_TO_PTR((p)[2])
#define NEXT(p)  WORD_TO_PTR((p)[3])

/* Binomial heap can be represented by a binary
   number. For example a binomial heap with 101011 items (as binary
   number) contains trees of height 5, 3, 1 and 0. So the ones in
   binary representation stand for the trees in a corresponding
   binomial heap.
   
   Binomial heap supports operations `find_min', `insert', `delete'
   and `merge' in O(logn) worst case time. */

/* `find_min' and `merge' below are two basic functions used by all
   operations: `priq_find_min', `priq_insert', `priq_delete' and
   `priq_merge'. */

/* Find the minimum tree of the priority queue pointed by
   `priq'. Minimum tree is a tree that contains the minimum item. */
static inline ptr_t find_min(ptr_t priq,
			     cmp_fun_t item_cmp,
			     void *context)
{
  ptr_t p, minimum_tree;
  cmp_result_t cmp_result;
 
  if (priq == NULL_PTR)
    return NULL_PTR;
  p = minimum_tree = priq;
  
  while (1) {
    p = NEXT(p);
    if (p == NULL_PTR)
      return minimum_tree;
    cmp_result = item_cmp(ITEM(minimum_tree), ITEM(p), context);
    assert(cmp_result != CMP_ERROR);
    if (cmp_result == CMP_GREATER)
      /* New minimum found. */
      minimum_tree = p;
  }
}

/* Merge two priroty queues. Return NULL_PTR if the items of the
   queues were not comparable together. Otherwise return the new
   priority queue. */
static inline ptr_t merge(ptr_t pq_1, 
			  ptr_t pq_2,
			  cmp_fun_t item_cmp,
			  void *context)
{
  ptr_t new_priq, p, tmp, last, son, root;
  cmp_result_t cmp_result;
  int new_height;

  assert((pq_1 != NULL_PTR) || (pq_2 != NULL_PTR));
  if (pq_1 == NULL_PTR)
    return pq_2;
  if (pq_2 == NULL_PTR)
    return pq_1;
  /* Meld `pq_1' with `pq_2' and store the result in `p'. */
  p = NULL_PTR;
  last = NULL_PTR;
  while (pq_1 != NULL_PTR && pq_2 != NULL_PTR) {
    /* Step trough the trees of both queues in increasinq order of height, 
       linking trees with equal heights as mentioned earlier. As a result the
       new queue contains no trees with equal height. */
    if (pq_1[3] != NULL_WORD && HEIGHT(pq_1) == HEIGHT(WORD_TO_PTR(pq_1[3]))) {
      /* This is a special case. `pq_1' contains two trees with equal 
	 heights. Thats because the memory item, that is left from the 
	 previous turn (two ones makes zero plus a memory. Link the the trees 
	 by making the smaller root the root of the new tree height 
	 HEIGHT(pq_1) + 1. */
      cmp_result = item_cmp(ITEM(pq_1), ITEM(WORD_TO_PTR(pq_1[3])), context);
      if (cmp_result == CMP_ERROR) {
	/* For some reason the comparison didn't succeed. */
	return NULL_PTR;
      }
      tmp = allocate(4, CELL_priq_item);
      if (cmp_result == CMP_GREATER) {
	/* Make the first tree the son. */
	son = pq_1;
	pq_1 = WORD_TO_PTR(pq_1[3]);
	son[3] = pq_1[2];
	tmp[0] |= HEIGHT(pq_1) + 1;
	tmp[1] = pq_1[1];
	tmp[2] = PTR_TO_WORD(son);
	tmp[3] = pq_1[3];
	pq_1 = tmp;
      } else {
	/* Make the second tree the son. */
	son = WORD_TO_PTR(pq_1[3]);
	tmp[0] |= HEIGHT(son);
	tmp[1] = son[1];
	tmp[2] = son[2];
	tmp[3] = pq_1[2];
	new_height = HEIGHT(pq_1) + 1;
	pq_1[0] &= 0xFF << 24;
	pq_1[0] |= new_height;
	pq_1[2] = PTR_TO_WORD(tmp);
	pq_1[3] = son[3];
      }
    } else if (HEIGHT(pq_1) < HEIGHT(pq_2)) {
      tmp = cell_copy(pq_1);
      if (last == NULL_PTR)
	p = tmp;
      else
	last[3] = PTR_TO_WORD(tmp);
      last = tmp;
      pq_1 = WORD_TO_PTR(pq_1[3]);
    } else if (HEIGHT(pq_1) > HEIGHT(pq_2)) {
      tmp = cell_copy(pq_2); 
      if (last == NULL_PTR)
	p = tmp;
      else 
	last[3] = PTR_TO_WORD(tmp);
      last = tmp;
      pq_2 = WORD_TO_PTR(pq_2[3]);
    } else/* HEIGHT(pq_1) == HEIGHT(pq_2) */{
      /* Link the the trees by making the smaller root as root of the new 
	 height HEIGHT(pq_2) + 1 tree. */
      cmp_result = item_cmp(ITEM(pq_1), ITEM(pq_2), context);
      if (cmp_result == CMP_ERROR) {
	/* For some reason the comparison didn't succeed. */
	return NULL_PTR;
      }
      root = allocate(4, CELL_priq_item);
      son = allocate(4, CELL_priq_item);
      if (cmp_result == CMP_GREATER) {
	tmp = pq_1;
	pq_1 = pq_2;
	pq_2 = tmp;
      }
      /* Make `pq_1' the root. */
      son[0] |= HEIGHT(pq_2);
      son[1] = pq_2[1];
      son[2] = pq_2[2];
      son[3] = pq_1[2];
      root[0] |= HEIGHT(pq_1) +1;
      root[1] = pq_1[1];
      root[2] = PTR_TO_WORD(son);
      root[3] = pq_1[3];
      pq_1 = root;
      pq_2 = WORD_TO_PTR(pq_2[3]);    
    }
  }
  /* The queues may not be equal in lengths. If `pq_1' has been 
     traversed trough and is so NULL_PTR, no memory item must be considered 
     and the rest of the `pq_2' may be inserted in the end of 
     the new queue. */
  if (pq_1 == NULL_PTR) {
    if (last == NULL_PTR)
      p = pq_2;
    else 
      last[3] = PTR_TO_WORD(pq_2);
  } else {
    /* `pq_1' is not NULL_PTR. As long as the memory item exists
       (`pq_1' contains two trees equal height) step trough
       `pq_1' and add the trees with equal height together. */
    while (pq_1 != NULL_PTR
	   && pq_1[3] != NULL_WORD
	   && HEIGHT(pq_1) == HEIGHT(WORD_TO_PTR(pq_1[3]))) {
      cmp_result = item_cmp(ITEM(pq_1), ITEM(WORD_TO_PTR(pq_1[3])), context);
      if (cmp_result == CMP_ERROR) 
	/* For some reason the comparison didn't succeed. */
	return NULL_PTR;
      tmp = allocate(4, CELL_priq_item);
      if (cmp_result == CMP_GREATER) {
	/* Make the first tree the son. */
	son = pq_1;
	pq_1 = WORD_TO_PTR(pq_1[3]);
	son[3] = pq_1[2];
	tmp[0] |= HEIGHT(pq_1) + 1;
	tmp[1] = pq_1[1];
	tmp[2] = PTR_TO_WORD(son);
	tmp[3] = pq_1[3];
	pq_1 = tmp;
      } else {
	/* Make the second tree the son. */
	son = WORD_TO_PTR(pq_1[3]);
	tmp[0] |= HEIGHT(son);
	tmp[1] = son[1];
	tmp[2] = son[2];
	tmp[3] = pq_1[2];
	new_height = HEIGHT(pq_1) + 1;
	pq_1[0] &= 0xFF << 24;
	pq_1[0] |= new_height;
	pq_1[2] = PTR_TO_WORD(tmp);
	pq_1[3] = son[3];
      }
    }
    /* Link the rest of the `pq_1' in the end of the new queue. */
    if (last == NULL_PTR)
      p = pq_1;
    else 
      last[3] = PTR_TO_WORD(pq_1);
  }
  return p;
}

/* Find the minimum item of `priq' or NULL_PTR if such doesn't exist. */
ptr_t priq_find_min(ptr_t priq, cmp_fun_t item_cmp, void *context)
{
  ptr_t p = find_min(priq, item_cmp, context);
  
  if (p == NULL_PTR) 
    return NULL_PTR;
  return ITEM(p); 
}

/* Insert `item' into `priq'. Return the new priority queue (or old
   one if insertion failed). Assumes
   `CAN_ALLOCATE(PRIQ_MAX_ALLOCATION)'.  */
ptr_t priq_insert(ptr_t priq,
		  ptr_t item,
		  cmp_fun_t item_cmp,
		  void *context)
{
  ptr_t ap = get_allocation_point();
  ptr_t new_priq_item = allocate(4, CELL_priq_item);
  ptr_t new_priq;

  /* Initialize the new priority queue item. */
  new_priq_item[1] = PTR_TO_WORD(item);
  new_priq_item[2] = NULL_WORD;
  new_priq_item[3] = NULL_WORD;
  
  /* Build new priority queue `new_priq' by melding
     `priq' with `new_priq_item'. */
  new_priq = merge(priq, new_priq_item, item_cmp, context);
  if (new_priq == NULL_PTR) {
    /* For some reason the comparison didn't succeed in `merge'. */
    restore_allocation_point(ap);
    return priq;
  }
  return new_priq;
}

/* Delete the minimum item of `priq'. Return the new priority queue
   (or old one if the deletion failed). Assumes
   `CAN_ALLOCATE(PRIQ_MAX_ALLOCATION)'. */
ptr_t priq_delete(ptr_t priq,
		  cmp_fun_t item_cmp,
		  void *context)
{
  ptr_t p = priq, min_tree, new_priq;
  ptr_t priq_except_min, last, rest_of_min, ap, new_p;
  cmp_result_t cmp_result;
 
  if (p == NULL_PTR)
    return NULL_PTR;
  min_tree = find_min(p, item_cmp, context);

  ap = get_allocation_point();

  /* Remove minimum tree from the "middle" of the priority queue. */
  if (p == min_tree) 
    /* The minimun tree is the first tree of the queue. */
    priq_except_min = NEXT(p);
  else {
    new_p = allocate(4, CELL_priq_item);
    new_p[0] = p[0];
    new_p[1] = p[1];
    new_p[2] = p[2];
    priq_except_min = new_p;
    last = new_p;
    p = NEXT(p);
    while (p != min_tree) {
      new_p = allocate(4, CELL_priq_item);
      new_p[0] = p[0];
      new_p[1] = p[1];
      new_p[2] = p[2];
      last[3] = PTR_TO_WORD(new_p);
      last = new_p;
      p = NEXT(p);
    }
    /* Jump over the old minimum. */
    last[3] = PTR_TO_WORD(NEXT(p));    
  }     
  /* `priq_except_min' points to a list of binomial trees with 
     all other trees except the tree that contains the minimun item. 
     Save the sub-queue below that minimum item to `rest_of_min'. */
  p = SON(min_tree);
  rest_of_min = NULL_PTR;
  while (p != NULL_PTR) {
    new_p = allocate(4, CELL_priq_item);
    new_p[0] = p[0];
    new_p[1] = p[1];
    new_p[2] = p[2];
    new_p[3] = PTR_TO_WORD(rest_of_min);
    rest_of_min = new_p;
    p = NEXT(p);
  }
  if ((priq_except_min == NULL_PTR) && (rest_of_min == NULL_PTR)) 
    /* The priroty queue contained only singleton node. Deletion it
       makes the priority queue empty. */
    return NULL_PTR;

  /* Meld `rest_of_min' with `priq_except_min' and store the result in 
     `new_priq[1]'. */
  new_priq = merge(rest_of_min,
		   priq_except_min,
		   item_cmp,
		   context);
  if (new_priq == NULL_PTR) {
    /* For some reason the comparison didn't succeed in `merge'. */
    restore_allocation_point(ap);
    return priq;
  }
  return new_priq;
}

/* Merge two priority queues. Return NULL_PTR if both priority queues
   were empty or the items of the queues were not comparable
   together. Otherwise return the new priority queue. */
ptr_t priq_merge(ptr_t pq_1, 
		 ptr_t pq_2,
		 cmp_fun_t item_cmp,
		 void *context)
{
  ptr_t new_priq, p, ap;
  cmp_result_t cmp_result;
  int new_height;

  if ((pq_1 == NULL_PTR) && (pq_2 == NULL_PTR))
    return NULL_PTR;
  
  ap = get_allocation_point();
  new_priq = merge(pq_1, pq_2, item_cmp, context);
  if (new_priq == NULL_PTR) {
    /* For some reason the comparison didn't succeed in `merge'. */
    restore_allocation_point(ap);
    return NULL_PTR;
  }
  return new_priq;
}


void priq_make_assertions(ptr_t priq)
{
  ptr_t p, previous;
  int priq_length = 0;

  if (PRIQ_IS_EMPTY(priq)) 
    return;
  p = priq;
  previous = NULL_PTR;
  while (p != NULL_PTR) {
    priq_length++;
    if (previous != NULL_PTR)
      assert(HEIGHT(previous) < HEIGHT(p));
    previous = p;
    p = WORD_TO_PTR(p[3]);
  } 
  if (priq_length > 32)
    fprintf(stderr, "\npriq_length == %d, highest height == %ld", 
	    priq_length, HEIGHT(previous));
  /*assert(priq_length <= 32);*/
  return;
}












