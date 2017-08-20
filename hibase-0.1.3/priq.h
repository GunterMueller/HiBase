/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajenen <sirkku@iki.fi>
 *
 * Priroty queue operations. The pririty queue implemented here is
 * binomial heap.
 *
 * For more information of destructive binomial heap see Cormen,
 * Leiserson, Rivest: Introduction to Algotrithms, The MIT Press,
 * (1991), pages 400-416.
 *
 * For more information of this non destructive implementation see
 * author's Thesis Tietorakenteiden tietoa tuhoamaton toteutus,
 * Helsingin yliopisto, 1998.  */

#ifndef INCL_PRIQ
#define INCL_PRIQ

#include "includes.h"
#include "shades.h"
#include "cells.h"

/* The amount memory needed is  4 (new item in insert)
   + at most 3 * root list.  */
#define PRIQ_MAX_ALLOCATION  (4 + 3 * (4 * 32))

/* Is the given priority queue empty?  Empty priority queues are
   `NULL_PTR's. */
#define PRIQ_IS_EMPTY(priq)  ((priq) == NULL_PTR)

/* Find the minimum item of `priq' or NULL_PTR if such doesn't
   exist. Doesn't allocate anything. */
ptr_t priq_find_min(ptr_t priq, cmp_fun_t item_cmp, void *context);

/* Insert `item' into `priq'. Return the new priority queue 
   (or old if insertion failed).  Assumes 
   `CAN_ALLOCATE(PRIQ_MAX_ALLOCATION)'. */
ptr_t priq_insert(ptr_t priq, ptr_t item,
		  cmp_fun_t item_cmp, void *context);

/* Delete the minimum item from `priq'. Return the new priority queue
   (or old if deletion failed). Assumes
   `CAN_ALLOCATE(PRIQ_MAX_ALLOCATION)'. */
ptr_t priq_delete(ptr_t priq, cmp_fun_t item_cmp, void *context);

/* Merge two priority queues. Return NULL_PTR if both priority queues
   were empty or the items of the queues were not comparable with each
   other. Otherwise return the new priority queue. */
ptr_t priq_merge(ptr_t priq_1, ptr_t priq_2,
		 cmp_fun_t item_cmp, void *context);

void priq_make_assertions(ptr_t priq);

#endif /* INCL_PRIQ */
