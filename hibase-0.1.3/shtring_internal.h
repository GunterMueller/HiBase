/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Lars Wirzenius <liw@iki.fi>
 */

/* Declarations internal to the implementation of shtrings. */

#ifndef INCL_SHTRING_INTERNAL_H
#define INCL_SHTRING_INTERNAL_H

#include "shtring.h"


/* Maximum number of children for one interior node. Must be at least 2. */
#define MAX_SHTRING_NODE_CHILDREN	8

/* Offset from start of chunk sequence where shtring contents begin. */
#define SHTRING_OFFSET(shtr)	((shtr)[1])

/* Length of shtring text. */
#define SHTRING_LENGTH(shtr)	((shtr)[2])

/* Height of chunk tree in shtring. */
#define SHTRING_HEIGHT(shtr)	((shtr)[3])

/* Root of chunk tree (may be a chunk or a node). */
#define SHTRING_CHUNK_TREE(shtr)	WORD_TO_PTR((shtr)[4])

/* Number of children in a node. */
#define SHTRING_NODE_CHILDREN(p)	((p)[0] & 0xFF)

/* Size of a node, in words. */
#define SHTRING_NODE_WORDS(p)		(3 + SHTRING_NODE_CHILDREN(p))

/* Size of a node, in words, if there are n children. */
#define SHTRING_NODE_WORDS_NEEDED(n)	(3 + (n))

/* Return height of (sub)tree for which node p is root. */
#define SHTRING_NODE_HEIGHT(p)	((p)[2])

/* Return length of (sub)tree for which node p is root. */
#define SHTRING_NODE_LENGTH(p)	((p)[1])

/* The i'th child of a node. First one is 1. */
#define SHTRING_NODE_CHILD(p,i)		WORD_TO_PTR((p)[2 + (i)])

/* The leftmost child of a node. Node must have at least one child. */
#define SHTRING_NODE_LEFTMOST(p)	SHTRING_NODE_CHILD(p, 1)

/* The rightmost child of a node. Node must have at least one child. */
#define SHTRING_NODE_RIGHTMOST(p)	SHTRING_NODE_CHILD(p, SHTRING_NODE_CHILDREN(p))

/* Number of characters in a chunk. */
#define SHTRING_CHUNK_LENGTH(p)	((p)[0] & 0xFFFFFF)

/* Size of chunk, in words, if it contains `n' characters. */
#define SHTRING_CHUNK_WORDS_NEEDED(n)	(1 + (n + sizeof(word_t) - 1) / sizeof(word_t))

/* Size of existing chunk, in words. */
#define SHTRING_CHUNK_WORDS(p)	SHTRING_CHUNK_WORDS_NEEDED(SHTRING_CHUNK_LENGTH(p))

/* Pointer to i'th character in a chunk. */
#define SHTRING_CHUNK_CHARPTR(chunk, i)	\
		((char *) ((chunk) + 1) + (i))



int shtring_check_chunk(ptr_t chunk);
int shtring_check_node(ptr_t node);
int shtring_check_shtring(ptr_t shtr);


typedef enum {
  left_to_right, right_to_left
} shtring_monkey_order_t;

#define SHTRING_MONKEY_START_POS	((word_t) -1)

int shtring_monkey(ptr_t root, word_t start, word_t len, word_t *pos,
                  int (*fun)(ptr_t, word_t, word_t, word_t, void *),
		  shtring_monkey_order_t order, void *arg);

word_t shtring_idiv(word_t a, word_t b);
word_t shtring_tree_length(ptr_t p);
word_t shtring_tree_height(ptr_t p);

ptr_t shtring_new_chunk(word_t n);
ptr_t shtring_new_node(int children, int height);
void shtring_add_child(ptr_t p, ptr_t kid, int *next_unass, word_t len);
void shtring_add_children(ptr_t q, int *next_unass, ptr_t p, int i, int j);
ptr_t shtring_new_shtring(word_t offset, word_t len, word_t height, ptr_t root);

#if 0

/* A shtring cursor gives the position within the logical portion of a
  shtring (i.e., not in the leading or trailing parts of the chunk tree
  that are not part of the shtring's value).  Cursors are used for
  scanning a shtring, when shtring_monkey is not practical. Cursors can
  only be moved forwards, at the moment. */

#define MAX_CURSOR_HEIGHT 100
struct shtring_cursor {
  ptr_t ptr[MAX_CURSOR_HEIGHT];
  int child[MAX_CURSOR_HEIGHT];

  /* Height of tree (>= 1), or -1 if tree has been exhausted. */
  int height;

  ptr_t chunk;
  word_t start, max_pos, chunk_pos, chunk_length;
};
typedef struct shtring_cursor shtring_cursor_t;

/* Initialize a cursor to the beginning of a shtring chunk tree. Return
   -1 if the tree is too high (more than MAX_CURSOR_HEIGHT), 0 otherwise.
   
   `root' is the chunk tree, `start' is the offset at which the shtring 
   begins, and `len' is its length. */
int shtring_cursor_init(shtring_cursor_t *cur, ptr_t root, word_t start,
			word_t len);

/* Move the cursor to offset `pos' in the shtring (after cur->start).
   Note that `pos' _must_ be at or after the current position. Return
   a pointer to the chunk at the new position. Use cur->chunk_pos to
   see where the chunk begins; the desired position is at offset
   pos - cur->chunk_pos in the chunk. 
   
   Return NULL_PTR if `pos' after the end of the shtring. */
ptr_t shtring_cursor_get_chunk(shtring_cursor_t *cur, word_t pos, 
			      word_t *chstart, word_t *chend);

#endif

#endif
