/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Lars Wirzenius <liw@iki.fi>
 */


/* Shtring cursors. An extended comment is forthcoming. XXX */



#include "shtring_internal.h"
#include "list.h"


/* Size of the shtring cursor node, sans stack, in words. */
#define SHTRING_CURSOR_MAX_ALLOCATION	5


/* Get child number from stack element. */
#define CHILD_NUMBER(stack)	((stack)[0] & 0xFFFFFF)


/* XXX */
#define STACK_NODE(stack)	(WORD_TO_PTR(CAR(stack)))
#define STACK_DOWN(stack)	(WORD_TO_PTR(CDR(stack)))



/* Return the chunk at the cursor. */
#define SHTRING_CURSOR_CHUNK(cur)	\
	(WORD_TO_PTR(CAR(SHTRING_CURSOR_STACK(cur))))


#define SHTRING_CURSOR_SHTRING(cur)	(WORD_TO_PTR((cur)[1]))
#define SHTRING_CURSOR_POS_OF_CHUNK(cur)	((cur)[2])
#define SHTRING_CURSOR_OFFSET_INTO_CHUNK(cur)	((cur)[3])
#define SHTRING_CURSOR_STACK(cur)	(WORD_TO_PTR((cur)[4]))


/* Validate a shtring cursor node. */
static int shtring_check_cursor(ptr_t cur)
{
  ptr_t shtr, chunk, stack;
  word_t pos_of_chunk, offset_into_chunk;

  if (cur == NULL_PTR || CELL_TYPE(cur) != CELL_shtring_cursor)
    return 0;

  shtr = SHTRING_CURSOR_SHTRING(cur);
  if (shtring_check_shtring(shtr) == 0)
    return 0;
  
  pos_of_chunk = SHTRING_CURSOR_POS_OF_CHUNK(cur);
  if (pos_of_chunk >= SHTRING_OFFSET(shtr) + SHTRING_LENGTH(shtr))
    return 0;

  stack = SHTRING_CURSOR_STACK(cur);
  if (CELL_TYPE(stack) != CELL_list)
    return 0;
  if ((word_t) list_length(stack) != SHTRING_HEIGHT(shtr))
    return 0;

  chunk = SHTRING_CURSOR_CHUNK(cur);
  if (shtring_check_chunk(chunk) == 0)
    return 0;

  offset_into_chunk = SHTRING_CURSOR_OFFSET_INTO_CHUNK(cur);
  if (offset_into_chunk >= SHTRING_CHUNK_LENGTH(chunk))
    return 0;
  if (pos_of_chunk + offset_into_chunk 
  		>= SHTRING_OFFSET(shtr) + SHTRING_LENGTH(shtr))
    return 0;

  return 1;
}


/* Allocate and initialize new shtring cursor node. Assume
   `can_allocate(SHTRING_CURSOR_MAX_ALLOCATION)'. */
static ptr_t new_cursor_node(ptr_t shtr, word_t pos_of_chunk,
			     word_t offset_into_chunk, ptr_t stack)
{
  ptr_t p;

  p = allocate(SHTRING_CURSOR_MAX_ALLOCATION, CELL_shtring_cursor);
  p[1] = PTR_TO_WORD(shtr);
  p[2] = pos_of_chunk;
  p[3] = offset_into_chunk;
  p[4] = PTR_TO_WORD(stack);
  heavy_assert(shtring_check_cursor(p));
  return p;
}



/* Allocate and initialize a new shtring cursor stack node. Assume
   `can_allocate(3)' (the list elements use 3 words). */
static ptr_t new_stack_node(word_t childno, ptr_t node, ptr_t stack)
{
  ptr_t p;

  assert(stack == NULL_PTR || CELL_TYPE(stack) == CELL_list);
  assert(CELL_TYPE(node) == CELL_shtring_node ||
         CELL_TYPE(node) == CELL_shtring_chunk);
  if (CELL_TYPE(node) == CELL_shtring_node) {
    assert(childno >= 1);
    assert(childno <= SHTRING_NODE_CHILDREN(node));
  } else {
    assert(childno == 1);
  }
  
  p = cons(node, stack);
  p[0] |= childno;
  return p;
}


/* Compute the maximum number of words that a shtring cursor operation may
   need if it allocates any memory at all. */

word_t shtring_cursor_max_allocation(ptr_t shtr)
{
  return SHTRING_CURSOR_MAX_ALLOCATION +
  	shtring_tree_height(SHTRING_CHUNK_TREE(shtr)) * CONS_MAX_ALLOCATION;
}



/* Create a shtring cursor that points at the beginning of the shtring.
   Return pointer to the cursor, or NULL_PTR if shtring is empty.  Assume
   `can_allocate(shtring_cursor_max_allocation(shtr))'. */
ptr_t shtring_cursor_create(ptr_t shtr)
{
  word_t i, child, children, child_length, offset_to_chunk, shtring_offset;
  ptr_t p, stack;
  
  heavy_assert(shtring_check_shtring(shtr));

  if (SHTRING_LENGTH(shtr) == 0)
    return NULL_PTR;

  /* Build the stack. */
  stack = NULL_PTR;
  p = SHTRING_CHUNK_TREE(shtr);
  offset_to_chunk = 0;
  shtring_offset = SHTRING_OFFSET(shtr);
  for (i = shtring_tree_height(p); i > 1; --i) {
    assert(CELL_TYPE(p) == CELL_shtring_node);
    children = SHTRING_NODE_CHILDREN(p);
    for (child = 1; child < children; ++child) {
      child_length = SHTRING_NODE_LENGTH(SHTRING_NODE_CHILD(p, child));
      if (offset_to_chunk + child_length > shtring_offset)
        break;
      offset_to_chunk += child_length;
    }

    stack = cons(p, stack);
    stack[0] |= child;
    p = SHTRING_NODE_CHILD(p, child);
  }

  /* Push a node with the chunk to top of stack. */
  assert(i == 1);
  stack = cons(p, stack);
  
  return new_cursor_node(shtr, offset_to_chunk,
			 SHTRING_OFFSET(shtr) - offset_to_chunk, stack);
}



/* Return the position of the cursor (as offset from the beginning of the
   shtring). */
word_t shtring_cursor_get_position(ptr_t cur)
{
  heavy_assert(shtring_check_cursor(cur));
  return SHTRING_CURSOR_POS_OF_CHUNK(cur) +
         SHTRING_CURSOR_OFFSET_INTO_CHUNK(cur) -
	 SHTRING_OFFSET(SHTRING_CURSOR_SHTRING(cur));
}



static int move_to_next_chunk_in_place(ptr_t stack)
{
  ptr_t node, down, down_node;
  word_t down_child;

  if (stack == NULL_PTR)
    return -1;

  assert(CELL_TYPE(stack) == CELL_list);
  node = STACK_NODE(stack);
  assert(CELL_TYPE(node) == CELL_shtring_node ||
         CELL_TYPE(node) == CELL_shtring_chunk);

  if (CELL_TYPE(node) == CELL_shtring_chunk) {
    down = STACK_DOWN(stack);
    if (move_to_next_chunk_in_place(down) == -1)
      return -1;
    down_node = STACK_NODE(down);
    down_child = CHILD_NUMBER(down);
    stack[1] = PTR_TO_WORD(SHTRING_NODE_CHILD(down_node, down_child));
    return 0;
  } else if (CHILD_NUMBER(stack) < SHTRING_NODE_CHILDREN(node)) {
    stack[0] = (stack[0] & ~0xFFFFFF) | (CHILD_NUMBER(stack) + 1);
    return 0;
  } else {
    down = STACK_DOWN(stack);
    if (move_to_next_chunk_in_place(down) == -1)
      return -1;
    stack[0] = (stack[0] & ~0xFFFFFF) | 1;
    down_node = STACK_NODE(down);
    down_child = CHILD_NUMBER(down);
    stack[1] = PTR_TO_WORD(SHTRING_NODE_CHILD(down_node, down_child));
    return 0;
  }
  /*NOTREACHED*/
}


static int move_forward_in_place(ptr_t cur, word_t nchars)
{
  word_t pos_of_chunk, offset_into_chunk, desired_pos, chunk_length;
  ptr_t shtr, stack, chunk;

  heavy_assert(shtring_check_cursor(cur));

  pos_of_chunk = SHTRING_CURSOR_POS_OF_CHUNK(cur);
  offset_into_chunk = SHTRING_CURSOR_OFFSET_INTO_CHUNK(cur);
  stack = SHTRING_CURSOR_STACK(cur);
  assert(stack != NULL_PTR);

  desired_pos = pos_of_chunk + offset_into_chunk + nchars;
  shtr = SHTRING_CURSOR_SHTRING(cur);
  if (desired_pos >= SHTRING_OFFSET(shtr) + SHTRING_LENGTH(shtr))
    return -1;

  chunk = NULL_PTR;
  for (;;) {
    chunk = STACK_NODE(stack);
    chunk_length = SHTRING_CHUNK_LENGTH(chunk);
    assert(pos_of_chunk <= desired_pos);
    if (pos_of_chunk + chunk_length > desired_pos)
      break;
    if (move_to_next_chunk_in_place(stack) == -1)
      return -1;
    assert(chunk != STACK_NODE(stack));
    pos_of_chunk += chunk_length;
  }

  SHTRING_CURSOR_POS_OF_CHUNK(cur) = pos_of_chunk;
  SHTRING_CURSOR_OFFSET_INTO_CHUNK(cur) = desired_pos - pos_of_chunk;
  heavy_assert(shtring_check_cursor(cur));
  return 0;
}


static ptr_t copy_stack(ptr_t stack)
{
  ptr_t below, node;
  
  if (stack == NULL_PTR)
    return NULL_PTR;
  below = copy_stack(STACK_DOWN(stack));
  node = STACK_NODE(stack);
  if (CELL_TYPE(node) == CELL_shtring_chunk)
    return new_stack_node(1, node, below);
  else
    return new_stack_node(CHILD_NUMBER(stack), node, below);
}


static ptr_t copy_cursor(ptr_t cur)
{
  heavy_assert(shtring_check_cursor(cur));
  return new_cursor_node(SHTRING_CURSOR_SHTRING(cur),
  			 SHTRING_CURSOR_POS_OF_CHUNK(cur),
			 SHTRING_CURSOR_OFFSET_INTO_CHUNK(cur),
			 copy_stack(SHTRING_CURSOR_STACK(cur)));
}



/* Create a new cursor at `nchars' after the position of `cur'; `cur'
   is not moved (GC removes it, unless used for something). Return pointer
   to new cursor. Assume `can_allocate(shtring_cursor_max_allocation(...))'.

   If the desired position is at or after the end of the shtring, return
   NULL_PTR.  */
ptr_t shtring_cursor_move_forward(ptr_t cur, word_t nchars)
{
  assert(0);
  return NULL_PTR;
}



/* Get chunk (and related information) at cursor. */
static void shtring_cursor_get_chunk(ptr_t cur, ptr_t *chunk,
				     word_t *offset_into_chunk,
				     word_t *useful_length_of_chunk)
{
  ptr_t shtring;
  word_t chunk_offset, chunk_length, shtring_end;

  heavy_assert(shtring_check_cursor(cur));

  *chunk = SHTRING_CURSOR_CHUNK(cur);
  *offset_into_chunk = SHTRING_CURSOR_OFFSET_INTO_CHUNK(cur);

  chunk_offset = SHTRING_CURSOR_POS_OF_CHUNK(cur);
  chunk_length = SHTRING_CHUNK_LENGTH(*chunk);

  shtring = SHTRING_CURSOR_SHTRING(cur);
  shtring_end = SHTRING_OFFSET(shtring) + SHTRING_LENGTH(shtring);

  if (chunk_offset + chunk_length <= shtring_end)
    *useful_length_of_chunk = chunk_length - *offset_into_chunk;
  else {
    assert(shtring_end >= chunk_offset + *offset_into_chunk);
    *useful_length_of_chunk = shtring_end - (chunk_offset + *offset_into_chunk);
  }
}



word_t shtring_cmp_piecemeal_max_allocation(ptr_t cur1, ptr_t cur2)
{
  return 2 * SHTRING_CURSOR_MAX_ALLOCATION +
         SHTRING_HEIGHT(cur1) * CONS_MAX_ALLOCATION +
         SHTRING_HEIGHT(cur2) * CONS_MAX_ALLOCATION;
}

cmp_result_t shtring_cmp_piecemeal(ptr_t *cur1, ptr_t *cur2, word_t maxchars)
{
  word_t off1, off2, len1, len2, len, full_words, excess_bytes, w1, w2;
  word_t i;
  ptr_t chunk1, chunk2;
  int result;
  char *p1, *p2;
  
  /* Note that the following will break, if sizeof(word_t) isn't 4. */
  word_t mask[sizeof(word_t)] = {
    0x0, 0xFF000000, 0xFFFF0000, 0xFFFFFF00,
  };

  heavy_assert(*cur1 == NULL_PTR || shtring_check_cursor(*cur1));
  heavy_assert(*cur2 == NULL_PTR || shtring_check_cursor(*cur2));
  
  if (*cur1 != NULL_PTR)
    *cur1 = copy_cursor(*cur1);
  if (*cur2 != NULL_PTR)
    *cur2 = copy_cursor(*cur2);

  if (*cur1 == NULL_PTR && *cur2 == NULL_PTR)
    return CMP_EQUAL;
  if (*cur1 == NULL_PTR)
    return CMP_LESS;
  if (*cur2 == NULL_PTR)
    return CMP_GREATER;
  
  shtring_cursor_get_chunk(*cur1, &chunk1, &off1, &len1);
  shtring_cursor_get_chunk(*cur2, &chunk2, &off2, &len2);

  if (len1 < len2)
    len = len1;
  else
    len = len2;
  if (len > maxchars)
    len = maxchars;

  p1 = SHTRING_CHUNK_CHARPTR(chunk1, SHTRING_CURSOR_OFFSET_INTO_CHUNK(*cur1));  
  p2 = SHTRING_CHUNK_CHARPTR(chunk2, SHTRING_CURSOR_OFFSET_INTO_CHUNK(*cur2));  

#define BYTEAT(chunk,off) \
  (((ptr_t) (chunk+1))[(off) / sizeof(word_t)] & mask[(off) % sizeof(word_t)])

  /* XXX the following loop could be improved a lot, but at the moment I
     don't have the time. */	
  i = 0;
  do {
    w1 = BYTEAT(chunk1, off1 + i);
    w2 = BYTEAT(chunk2, off2 + i);
  } while (w1 == w2 && i++ < len);
  if (w1 == w2)
    result = 0;
  else if (w1 < w2)
    result = -1;
  else
    result = 1;

  if (*cur1 != NULL_PTR && move_forward_in_place(*cur1, len) == -1)
    *cur1 = NULL_PTR;
  if (*cur2 != NULL_PTR && move_forward_in_place(*cur2, len) == -1)
    *cur2 = NULL_PTR;

  if (result < 0 || (result == 0 && *cur1 == NULL_PTR && *cur2 != NULL_PTR))
    return CMP_LESS;
  if (result > 0 || (result == 0 && *cur1 != NULL_PTR && *cur2 == NULL_PTR))
    return CMP_GREATER;
  assert(result == 0);
  if (*cur1 == NULL_PTR && *cur2 == NULL_PTR)
    return CMP_EQUAL;
  
  assert(*cur1 != NULL_PTR);
  assert(*cur2 != NULL_PTR);
  return CMP_PLEASE_CONTINUE;
}
