/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Sirkku-Liisa Paajanen <sirkku@iki.fi>
 */

/* A logarithmic queue. For more information see author's thesis
   Tietorakenteiden tietoa tuhoamaton toteutus. */

#include "includes.h"
#include "lq.h"

#define NEXT(p)     (WORD_TO_PTR((p)[2]))
#define HEIGHT(p)   (((p)[0]) & 0xFFFFFF)


/* Return the first item in the given queue. Supposes the given 
   queue isn't empty.  This function does not allocate memory. */
ptr_t lq_get_first(ptr_t queue)
{
  ptr_t p;
  assert(queue != NULL_PTR);
  assert(queue[2] != NULL_WORD);
  p = WORD_TO_PTR(queue[2]);
  assert((CELL_TYPE(WORD_TO_PTR(queue[2])) == CELL_lq_item00) ||
	 (CELL_TYPE(WORD_TO_PTR(queue[2])) == CELL_lq_item01) ||
	 (CELL_TYPE(WORD_TO_PTR(queue[2])) == CELL_lq_item10) ||
	 (CELL_TYPE(WORD_TO_PTR(queue[2])) == CELL_lq_item11));
  /* Find the rightmost of the `queue[2]'. */ 
loop:

  if (CELL_TYPE(p) == CELL_lq_item10) {
    p = WORD_TO_PTR(p[2]);
    goto loop;    
  }
  if (CELL_TYPE(p) == CELL_lq_item11) {
    p = WORD_TO_PTR(p[3]);
    goto loop;    
  } 
  return WORD_TO_PTR(p[1]);
}

#if 0
/* Return the last item in the given queue. Supposes the given queue
   queue isn't empty. */
ptr_t lq_get_last(ptr_t queue)
{
  assert(queue != NULL_PTR);
  return NULL_PTR;
}
#endif

/* Create a new queue with the given data item inserted last into the
   queue.  Returns the new queue.  Assumes
   `can_allocate(LQ_INSERT_LAST_MAX_ALLOCATION)'. */
ptr_t lq_insert_last(ptr_t queue, ptr_t data)
{
  
  ptr_t new_queue, new_item, first_tree, second_tree, left_son, right_son;
  
  if (queue == NULL_PTR) {
    new_item = allocate(2, CELL_lq_item00);
    new_item[1] = PTR_TO_WORD(data);
    new_queue = allocate(3, CELL_lq_entry_point);
    new_queue[1] = PTR_TO_WORD(new_item);
    new_queue[2] = NULL_WORD;
    return new_queue;
  } 
  if (queue[2] == NULL_WORD) {
    assert(queue[1] != NULL_WORD);
    new_item = allocate(2, CELL_lq_item00);
    new_item[1] = PTR_TO_WORD(data);
    new_queue = allocate(3, CELL_lq_entry_point);
    new_queue[1] = queue[1];
    new_queue[2] = PTR_TO_WORD(new_item);
    return new_queue;
  }
  
  assert((CELL_TYPE(WORD_TO_PTR(queue[2])) == CELL_lq_item00) ||
	 (CELL_TYPE(WORD_TO_PTR(queue[2])) == CELL_lq_item01) ||
	 (CELL_TYPE(WORD_TO_PTR(queue[2])) == CELL_lq_item10) ||
	 (CELL_TYPE(WORD_TO_PTR(queue[2])) == CELL_lq_item11));
  
  switch (CELL_TYPE(WORD_TO_PTR(queue[2]))) {
  case CELL_lq_item00: case CELL_lq_item10: 
    new_item = allocate(3, CELL_lq_item01);
    new_item[1] = PTR_TO_WORD(data);
    new_item[2] = queue[2];
    break;
  case CELL_lq_item01:
    /* The tail contains at least two trees. */
    first_tree = WORD_TO_PTR(queue[2]);
    second_tree = NEXT(first_tree);
    switch (CELL_TYPE(second_tree)) {
    case CELL_lq_item10: case CELL_lq_item11:
      new_item = allocate(3, CELL_lq_item01);
      new_item[1] = PTR_TO_WORD(data);
      new_item[2] = queue[2];
      break;
    case CELL_lq_item00:
      /* Tail contains of two singleton nodes. Link them to be sons
         of the new item. */
      new_item = allocate(3, CELL_lq_item10);
      new_item[0] |= 1;
      new_item[1] = PTR_TO_WORD(data);
      left_son = allocate(2, CELL_lq_item00);
      left_son[1] = first_tree[1];	
      right_son = allocate(3, CELL_lq_item01);
      right_son[1] = second_tree[1];	
      right_son[2] = PTR_TO_WORD(left_son);
      new_item[2] = PTR_TO_WORD(right_son);
      break;
    case CELL_lq_item01:
      /* Tail contains of two singleton nodes in the head. Link them
         to be sons of the new item. */
      new_item = allocate(4, CELL_lq_item11);
      new_item[0] |= 1;
      new_item[1] = PTR_TO_WORD(data);
      new_item[2] = second_tree[2];
      left_son = allocate(2, CELL_lq_item00);
      left_son[1] = first_tree[1];	
      right_son = allocate(3, CELL_lq_item01);
      right_son[1] = second_tree[1];	
      right_son[2] = PTR_TO_WORD(left_son);
      new_item[3] = PTR_TO_WORD(right_son);
      break;
    default: 
      assert(0);
    }
    break;
  case CELL_lq_item11:
    /* The tail contains at least two trees. */
    first_tree = WORD_TO_PTR(queue[2]);
    assert(HEIGHT(WORD_TO_PTR(first_tree[3])) == HEIGHT(first_tree) - 1);
    assert(HEIGHT(first_tree) > 0);
    second_tree = NEXT(first_tree);
    assert((CELL_TYPE(second_tree) == CELL_lq_item10) ||
	   (CELL_TYPE(second_tree) == CELL_lq_item11));
    assert(HEIGHT(second_tree) >= HEIGHT(first_tree));
    if (HEIGHT(first_tree) != HEIGHT(second_tree)) {
      new_item = allocate(3, CELL_lq_item01);
      new_item[1] = PTR_TO_WORD(data);
      new_item[2] = queue[2];
      break;
    }    
    left_son = allocate(3, CELL_lq_item10);
    left_son[0] |= HEIGHT(first_tree); 
    left_son[1] = first_tree[1];	
    left_son[2] = first_tree[3];	
    right_son = allocate(4, CELL_lq_item11);
    right_son[0] |= HEIGHT(second_tree); 
    right_son[1] = second_tree[1];	
    right_son[2] = PTR_TO_WORD(left_son);
    if (CELL_TYPE(second_tree) == CELL_lq_item10) {
      new_item = allocate(3, CELL_lq_item10);
      new_item[0] |= HEIGHT(first_tree) + 1;
      new_item[1] = PTR_TO_WORD(data);
      right_son[3] = second_tree[2];
      new_item[2] = PTR_TO_WORD(right_son);
    } else { /*    case CELL_lq_item11:     */
      new_item = allocate(4, CELL_lq_item11);
      new_item[0] |= HEIGHT(first_tree) + 1;
      new_item[1] = PTR_TO_WORD(data);
      new_item[2] = second_tree[2];
      right_son[3] = second_tree[3];
      new_item[3] = PTR_TO_WORD(right_son);
    }
  }
  
  /* Make new queue header. */
  new_queue = allocate(3, CELL_lq_entry_point);
  new_queue[1] = queue[1];
  new_queue[2] = PTR_TO_WORD(new_item);
  return new_queue;
  
}

/* Create a new queue with first item removed. */
ptr_t lq_remove_first(ptr_t queue)
{
  ptr_t new_queue, tree, p, brother;

  if (queue == NULL_PTR) 
    return NULL_PTR;
  if (queue[1] != NULL_WORD) {
    switch (CELL_TYPE(WORD_TO_PTR(queue[1]))) {
    case CELL_lq_item00:
      if (queue[2] == NULL_WORD)
	return NULL_PTR;
      new_queue = allocate(3, CELL_lq_entry_point);
      new_queue[1] = NULL_WORD;
      new_queue[2] = queue[2];
      return new_queue;
    case CELL_lq_item01:
      new_queue = allocate(3, CELL_lq_entry_point);
      new_queue[1] = (WORD_TO_PTR(queue[1]))[2];
      new_queue[2] = queue[2];
      return new_queue;
    case CELL_lq_item10:
      /* The first item in the given queue is a tree. Before the
         deletion can be made, the tree must be unpacked. */
      tree = WORD_TO_PTR(queue[1]);/* `tree' points to the tree. */
      assert(HEIGHT(tree) > 0);
      /* The item to be deleted is the last item in `tree' in _innerorder_.  
	 Before the deletion can be made, the tree must be unpacked. */
      /* Start unpacking. */
      p = allocate(2, CELL_lq_item00);
      p[1] = tree[1];
      /* Go to the right son. */
      tree = WORD_TO_PTR(tree[2]);
      break;
    case CELL_lq_item11:
      /* The first item in the given queue is a tree. Before the
         deletion can be made, the tree must be unpacked. */
      tree = WORD_TO_PTR(queue[1]);/* `tree' points to the tree. */
      assert(HEIGHT(tree) > 0);
      /* The item to be deleted is the last item in `tree' in _innerorder_.  
	 Before the deletion can be made, the tree must be unpacked. */
      /* Start unpacking. */
      p = allocate(3, CELL_lq_item01);
      p[1] = tree[1];
      p[2] = tree[2];
      /* Go to the right son. */
      tree = WORD_TO_PTR(tree[3]);
      break;
    default:
      assert(0);
    }
    new_queue = allocate(3, CELL_lq_entry_point);
    /* Insert `p' to the new head. */
    new_queue[1] = PTR_TO_WORD(p);
    new_queue[2] = queue[2];     
    goto unpack;
  }
    
  /* ELSE */

  /* `queue[1]' == NULL_PTR. Before the deletion can be made,
     `queue[2]' must be reversed. */
  assert(queue[2] != NULL_WORD);
  switch (CELL_TYPE(WORD_TO_PTR(queue[2]))) {
  case CELL_lq_item00:
    /* The tail is a singleton node. Deletion makes the the head empty. */
    return NULL_PTR;
  case CELL_lq_item10:
    /* `queue[2]' consists only of a single tree. */
    tree = WORD_TO_PTR(queue[2]);/* `tree' points to the tree. */
    assert(HEIGHT(tree) > 0);
    /* The item to be deleted is the last item in `tree' in _innerorder_.  
       Before the deletion can be made, the tree must be unpacked. */
    /* Start unpacking. */
    p = allocate(2, CELL_lq_item00);
    p[1] = tree[1];
    /* Go to the right son. */
    tree = WORD_TO_PTR(tree[2]);
    new_queue = allocate(3, CELL_lq_entry_point);
    /* Insert `p' to the new head. */
    new_queue[1] = PTR_TO_WORD(p);
    new_queue[2] = NULL_WORD;     
    goto unpack;
  case CELL_lq_item01: 
    /* Before the deletion can be made, `queue[2]' must be reversed first. */
    tree = WORD_TO_PTR(queue[2]);/* `tree' points to the first tree. */
    p = allocate(2, CELL_lq_item00);
    p[1] = tree[1];
    break;
  case CELL_lq_item11:
    /* Before the deletion can be made, `queue[2]' must be reversed first. */
    tree = WORD_TO_PTR(queue[2]);/* `tree' points to the first tree. */
    assert(HEIGHT(tree) > 0);
    p = allocate(3, CELL_lq_item10);
    p[0] |= HEIGHT(tree);
    p[1] = tree[1];
    p[2] = tree[3];
    break;
  default:
    assert(0);
  }

  new_queue = allocate(3, CELL_lq_entry_point);
  /* Reverse operation moves all trees from tail to head. */
  new_queue[2] = NULL_WORD;     
    
reverse: /* REVERSE */

  /* Insert reversed tree pointed by `p' to the new head. */
  new_queue[1] = PTR_TO_WORD(p);
  /* Go to the next tree. */
  tree = NEXT(tree);

  switch (CELL_TYPE(tree)) {
  case CELL_lq_item00: 
    return new_queue;     
  case CELL_lq_item10:
    assert(HEIGHT(tree) > 0);
    /* Now `tree' should point to the last tree in `queue[2]'. */
    /* The item to be deleted is the last item in `tree' in _innerorder_.  
       Before the deletion can be made, the tree must be unpacked. */
    /* Start unpacking. */
    p = allocate(3, CELL_lq_item01);
    p[1] = tree[1];
    /* Insert `p' to the front of new head. */
    p[2] = new_queue[1];
    new_queue[1] = PTR_TO_WORD(p);
    assert(HEIGHT(WORD_TO_PTR(tree[2])) == HEIGHT(tree) - 1);
    /* Go to the right son. */
    tree = WORD_TO_PTR(tree[2]);
    goto unpack;  
  case CELL_lq_item01: 
    /* Move next tree from `queue[2]' to the new head, e.g. insert
       `tree' in the front of new head, and go to the next tree in
       `queue[2]'. */
    p = allocate(3, CELL_lq_item01);
    p[1] = tree[1];
    p[2] = new_queue[1];
    goto reverse;
  case CELL_lq_item11:
    assert(HEIGHT(tree) > 0);
    /* Move next tree from `queue[2]' to the new head, e.g. insert
       `tree' in the front of new head, and go to the next tree in
       `queue[2]'. */
    p = allocate(4, CELL_lq_item11);
    p[0] |= HEIGHT(tree);
    p[1] = tree[1];
    p[2] = new_queue[1];
    p[3] = tree[3];
    goto reverse;
  default:
    assert(0);
  }

unpack: /* UNPACK THE LAST TREE OF `queue[2]' (OR FIRST TREE OF
           `queue[1]')! */

  /* `tree' is a pointer to the tree that must be unpacked. Before
     unpacking the tree, insert the brother of `tree' to the front of
     new head. This is part of the unpacking process. */
  assert(new_queue[1] != NULL_WORD);
  switch (CELL_TYPE(tree)) {
  case CELL_lq_item01: 
    /* The tree has been unpacked now. `tree' should point to the item
       that must be deleted. The last step left is to insert the
       brother of `tree' to the front of new head. */
    assert(HEIGHT(tree) == 0);
    tree = NEXT(tree); /* Go to the brother. */
    assert(HEIGHT(tree) == 0);
    p = allocate(3, CELL_lq_item01);
    p[1] = tree[1];
    p[2] = new_queue[1];
    new_queue[1] = PTR_TO_WORD(p); 
    return new_queue;
  case CELL_lq_item11:
    /* The first step is to insert the
       brother of `tree' to the front of new head. */
    brother = NEXT(tree);
    assert(CELL_TYPE(brother) == CELL_lq_item10);
    p = allocate(4, CELL_lq_item11);
    p[0] |= HEIGHT(brother);    
    p[1] = brother[1];
    p[2] = new_queue[1];
    p[3] = brother[2];
    new_queue[1] = PTR_TO_WORD(p);
    /* Next step is to insert the root of `tree' to the front of new head. */
    p = allocate(3, CELL_lq_item01);
    p[1] = tree[1];
    p[2] = new_queue[1];
    new_queue[1] = PTR_TO_WORD(p);
    assert(HEIGHT(WORD_TO_PTR(tree[3])) == HEIGHT(tree) - 1);
    /* Go to the right son. */
    tree = WORD_TO_PTR(tree[3]);
    goto /* back to */unpack;
  default:
    assert(0);
  }
}


