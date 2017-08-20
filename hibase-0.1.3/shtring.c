/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Lars Wirzenius <liw@iki.fi>
 */
 

/* Implementation of Shades's strings, shtrings.

   The Shades memory model frowns on huge objects, so a shtring
   is divided into a number of relatively small chunks. This also
   makes some operations faster.
   
   The chunks are put into a tree, with chunks all being at the
   same level in the tree. Interior nodes are of different size,
   up to a smallish arbitrary limit. The shtring itself is
   represented by a descriptor node, which contains the root
   pointer of the chunk tree.
   
   Removing characters from the beginning or end of a shtring
   is a common operation. The shtring descriptor contains an
   offset into the chunk sequence, and a length. These two
   fields allow the shtring to skip characters from the beginning
   and end of the chunk sequences without having to copy chunks
   or modify the chunk tree -- only a new shtring descriptor
   node is necessary.
   
   Concatenation of shtrings is also common, and can also
   be done quite fast. In principle, it is enough to create
   a new node, which gets the chunk trees of the two existing
   shtrings as its children, and Bob's your uncle. However,
   since the last chunk in the first shtring and the first
   chunk in the second shtring may not be completely in use,
   it may be necessary to copy those two chunks and the interior
   nodes on the paths to the roots of the trees. This is still
   quite fast compared to copying everything in a long shtring.
   
   To avoid filling the first generation area in Shades when
   building a shtring, shtring_create restricts the size of
   the shtrings it builds to half of the first generation.
   It is still possible to build larger shtrings, by building
   smaller shtrings and concatenating them.
   
   Chunks consist of 32-bit words. Bytes are stored in the
   words in big-endian order to let them survive a migration
   between different-endian systems. It also makes it trivial to
   compare shtrings for byte-wise order.

   Conversions between text in C character arrays and chunks
   can be done as a simple memory copy on big-endian systems.
   On little-endian systems the order of the bytes needs to
   be corrected, which is slower. The implementation automatically
   does the Right Thing based on the pre-processor macro
   WORDS_BIGENDIAN, which is defined by the configuration script. 
   
   Such parts of the code that depend on the sizes of bytes and
   words are marked with suitable asserts.
   
   */

/* Note: This file is not serious enough. */   

#include <limits.h>
   
#include "shtring.h"
#include "shtring_internal.h"
#include "triev2.h"


/* This implementation assumes that there are 8 bits per character. */
#if CHAR_BIT != 8
#error CHAR_BIT != 8, but code assumes eight bits per character.
#endif

/* Bit mask for the bits in a character (i.e., CHAR_BIT one bits at bottom). */
#define CHAR_MASK	(~(~0 << CHAR_BIT))


/* Is a pointer aligned to access objects of type T? */
/* Note: This is an unportable hack, but it works on most platforms. */
#define IS_ALIGNED(p,T)	((((unsigned long) (p)) % sizeof(T)) == 0)


/* Initial value for computing hash function for shtrings. I have no
   idea why this value is good, but it seems to be. */
#define SHTRING_HASH_START_VALUE 5381	/* ain't magic numbersh fun? */


/* Length of shtring hash value, in bits. */
#define SHTRING_HASH_BITS	32





/* Build a shtring from a C character array. */

/* Build a chunk out of `cstr'. Assume `can_allocate(enough)'. Return
   pointer to chunk. Advance `*cstr' (and decrement `*len') past the 
   converted part.
   
   The code below has been optimized a bit, by using loop unrolling:
   instead of doing only one thing per loop iteration, we do many
   things. This reduces the overhead caused by the loop (testing and
   incrementing the loop variable, jumping to beginning of loop),
   and has a significant impact on the speed of this function.  */
static ptr_t build_chunk(char **cstr, size_t *len)
{
  word_t i, n, loops;
  ptr_t p, q;
  unsigned char *ucstr;
  word_t *wstr;
  
  assert(cstr != NULL);
  assert(len != NULL);
  assert(*len > 0);

  n = *len;
  if (n > SHTRING_CHUNK_MAX)
    n = SHTRING_CHUNK_MAX;
  p = shtring_new_chunk(n);
  
#if WORDS_BIGENDIAN
  memcpy(p+1, *cstr, n);
  *cstr += n;
  *len -= n;
#else
  /* The code below assumes that word_t is 32 bits, but is trivial to
     extend to other word sizes. */

  i = 1;
  ucstr = *(unsigned  char **) cstr;
  *cstr += n;
  *len -= n;
  
  assert(sizeof(word_t) == 4);

  if (n >= 4 && IS_ALIGNED(cstr, word_t)) {
    wstr = (word_t *) ucstr;
    q = p + i;
    loops = n / 4;
    n %= 4;

    /* This loop unrolling helps a bit on long inputs. */
    while (loops > 8) {
      *q++ = SWAP_BYTES(*wstr++);
      *q++ = SWAP_BYTES(*wstr++);
      *q++ = SWAP_BYTES(*wstr++);
      *q++ = SWAP_BYTES(*wstr++);
      *q++ = SWAP_BYTES(*wstr++);
      *q++ = SWAP_BYTES(*wstr++);
      *q++ = SWAP_BYTES(*wstr++);
      *q++ = SWAP_BYTES(*wstr++);
      loops -= 8;
    }

    while (loops--)
      *q++ = SWAP_BYTES(*wstr++);

    i = q - p;
    ucstr = (unsigned char *) wstr;
  } else {
    while (n >= 4) {
      p[i] = (ucstr[0] << 24) | (ucstr[1] << 16) | (ucstr[2] << 8) | ucstr[3];
      ucstr += 4;
      n -= 4;
      ++i;
    }
  }
  assert((char *) ucstr + n == *cstr);

  switch (n) {
  case 3:
    p[i] = (ucstr[0] << 24) | (ucstr[1] << 16) | (ucstr[2] << 8);
    break;
  case 2:
    p[i] = (ucstr[0] << 24) | (ucstr[1] << 16);
    break;
  case 1:
    p[i] = (ucstr[0] << 24);
    break;
  }
#endif

  heavy_assert(shtring_check_chunk(p));
  return p;  
}


/* Build a chunk tree out of `cstr'. Assume `can_allocate(enough)'.
   Height of tree will be `h'. Return pointer to root. Advance `*cstr'
   (and decrement `*len') past the converted part. */
static ptr_t build_tree(word_t height, char **cstr, size_t *len)
{
  int i, j, unass;
  word_t plen;
  ptr_t p, child;
  ptr_t tab[MAX_SHTRING_NODE_CHILDREN];

  assert(cstr != NULL);
  assert(*cstr != NULL);
  assert(len != NULL);
  assert(height > 0 || (height == 0 && *len == 0));

  if (height == 0)
    return NULL_PTR;

  if (height == 1)
    return build_chunk(cstr, len);

  for (i = 0; i < MAX_SHTRING_NODE_CHILDREN && *len > 0; ++i)
    tab[i] = build_tree(height-1, cstr, len);

  p = shtring_new_node(i, height);
  unass = 1;
  if (height == 2) {
    for (j = 0; j < i; ++j)
      shtring_add_child(p, tab[j], &unass, SHTRING_CHUNK_LENGTH(tab[j]));
  } else {
    for (j = 0; j < i; ++j)
      shtring_add_child(p, tab[j], &unass, SHTRING_NODE_LENGTH(tab[j]));
  }

  heavy_assert(shtring_check_node(p));    
  return p;
}


/* Given a C string of `len' characters to be converted to a chunk tree,
   compute minimum number of chunks, height of tree, and the minimum total
   number of words needed to store the tree. 
   
   This could probably be computed without a loop. I should take some
   classes in mathemagics. */
static void shtring_tree_size(size_t len, word_t *chunks, word_t *height, word_t *words)
{
  word_t i, n;

  assert(chunks != NULL);
  assert(height != NULL);
  assert(words != NULL);

  *chunks = shtring_idiv(len, SHTRING_CHUNK_MAX);
  *words = shtring_idiv(len, sizeof(word_t)) + *chunks;
  *height = (*chunks > 0);
  for (i = *chunks; i > 1; i = n, *height += 1) {
    n = shtring_idiv(i, MAX_SHTRING_NODE_CHILDREN);
    *words += i + n * 3;
  }
  *words += SHTRING_WORDS;
}


word_t shtring_create_max_allocation(size_t len)
{
  word_t chunks, height, total;
  shtring_tree_size(len, &chunks, &height, &total);
  return total;
}


/* Create a new shtring and initialize it to a C byte array. Return
   -1 for permanent error (no use re-trying), or 1 (operation successful, 
   pointer to new shtring stored in `*shtr'). Assumes that caller has
   done `can_allocate(shtring_create_max_allocation(len))'. */
int shtring_create(ptr_t *shtr, char *cstr, size_t len)
{
  word_t chunks, height, total;
  size_t len2;

  assert(shtr != NULL);
  assert(cstr != NULL);

  if (len > (word_t) first_generation_size/2)
    return -1;

  shtring_tree_size(len, &chunks, &height, &total);
  len2 = len;
  *shtr = shtring_new_shtring(0, len2, height, build_tree(height, &cstr, &len));
  return 1;
}



/* Catenate two shtrings. 

   In theory, catenation is trivial: just create a new root that
   has the two input trees as its children. However, this would
   make the trees grow very high very soon, so a better, but more
   complicated and slower algorithm is used.
   
   The principle is still simple: avoid making the resulting tree
   higher by joining two trees at a non-root level, by combining
   the rightmost and leftmost nodes on the same level in the first
   and second trees, respectively. If this is impossible, the tree
   is made higher. The details of the implementation are tiresomely
   tricky, however; not difficult, they just crave for attention.
   For example, the rightmost and leftmost chunks in the first and
   second tree, respectively, need to be combined, or at least any
   unused text in them needs to be discarded, since the chunk
   sequence may not contain holes.
   
   Catenation also tries to keep the trees small by using only
   those parts of the input trees that are needed for the shtring.
   Unused chunks at ends are removed from the result, if possible.  */


/* Count number of words needed to make `p' to be at least of height `h'.
   `p' must point at a node, not a chunk. */
static word_t needed_for_height(ptr_t p, word_t h)
{
  word_t current;

  current = shtring_tree_height(p);
  assert(current <= h);
  if (current == h)
    return 0;
  return (h - current) * SHTRING_NODE_WORDS_NEEDED(1);
}


/* Pessimistic approximation of words needed to perform
   `trim_ends(p, start, end)'. Probably too pessimistic. */
static word_t needed_for_trim(ptr_t p, word_t start, word_t end)
{
  word_t h, chunk, node;

  if (p == NULL_PTR)
    return 0;
  assert(start + end <= shtring_tree_length(p));
  if (start == 0 && end == 0)
    return 0;
  chunk = SHTRING_CHUNK_WORDS_NEEDED(SHTRING_CHUNK_MAX);
  node = SHTRING_NODE_WORDS_NEEDED(MAX_SHTRING_NODE_CHILDREN);
  return 2 * chunk + shtring_tree_height(p) * node * 2;
}


/* Compute the number of words to perform `shtring_cat(shtr1, shtr2)'. */
word_t shtring_cat_max_allocation(ptr_t shtr1, ptr_t shtr2)
{
  word_t h, needed;
  ptr_t p1, p2;

  heavy_assert(shtring_check_shtring(shtr1));
  heavy_assert(shtring_check_shtring(shtr2));

  h = SHTRING_HEIGHT(shtr1);
  if (h < SHTRING_HEIGHT(shtr2))
    h = SHTRING_HEIGHT(shtr2);
  ++h;

  /* The following calculations are somewhat pessimistic. */
  p1 = SHTRING_CHUNK_TREE(shtr1);
  p2 = SHTRING_CHUNK_TREE(shtr2);
  needed = needed_for_height(p1, h) + needed_for_height(p2, h);
  needed += h * (MAX_SHTRING_NODE_CHILDREN + 1);
  needed += needed_for_trim(p1, SHTRING_OFFSET(shtr1), 1);
  needed += needed_for_trim(p2, SHTRING_OFFSET(shtr2), 1);
  needed += SHTRING_WORDS;
  
  return needed;
}


/* Make sure a chunk tree is of height `h', by adding new
   levels of nodes. Return new root. Assume
   `can_allocate(needed_for_height(p,h))'.  `p' must point at
   a node, not a chunk. */
static ptr_t make_of_height(ptr_t p, word_t h)
{
  ptr_t new;
  word_t len, current;
  int unass;

  current = shtring_tree_height(p);
  assert(current <= h);

  len = shtring_tree_length(p);
  while (current < h) {
    ++current;
    new = shtring_new_node(1, current);
    unass = 1;
    shtring_add_child(new, p, &unass, len);
    p = new;
  }

  /* The compiler will optimize the following away, if NDEBUG is defined. */
  switch (h) {
  case 0: assert(p == NULL_PTR); break;
  case 1: heavy_assert(shtring_check_chunk(p)); break;
  default: heavy_assert(shtring_check_node(p)); break;
  }

  return p;
}


/* Join two trees without making the resulting tree higher than the
   higher one of the input trees. Return pointer to new root, or
   NULL_PTR if operation failed. Assume `can_allocate(enough)'.  
   
   If two trees are of equal height, and their roots have less than
   MAX_SHTRING_NODE_CHILDREN children together, the trees can be catenated
   by creating a new root to hold the children of both roots. This
   requires that the adjacent ends of the input trees don't have
   any extra characters, but we make that assumption.
   
   If the two trees are of different height, we traverse downward the
   side of the taller tree that is adjacent to the lower one, until we
   come to level that is at the same height as the lower tree, and then
   we try to cat that subtree with the lower tree, as above. If that
   succeeds, we re-build the upper levels of the higher tree replacing
   the old subtree with the newly joined subtree-and-lower-tree. If it
   fails, then it fails. Tough.
   
   */
static ptr_t cat_recursive(ptr_t p1, int h1, ptr_t p2, int h2)
{
  ptr_t q, qq;
  int n1, n2, unass;

  assert(h1 > 0);
  assert(h2 > 0);
  assert((word_t) h1 == shtring_tree_height(p1));
  assert((word_t) h2 == shtring_tree_height(p2));

  n1 = SHTRING_NODE_CHILDREN(p1);
  n2 = SHTRING_NODE_CHILDREN(p2);
  unass = 1;

  if (h1 == h2) {
    /* The trees are of equal height. If the roots of the trees are
       small enough together, create a single new node that holds all
       the child-pointers of the two roots. Otherwise, fail. */
    if (h1 == 1 || n1 + n2 > MAX_SHTRING_NODE_CHILDREN)
      return NULL_PTR;
    q = shtring_new_node(n1 + n2, h1);
    shtring_add_children(q, &unass, p1, 1, n1);
    shtring_add_children(q, &unass, p2, 1, n2);
    heavy_assert(shtring_check_node(q));
    return q;
  } else if (h1 < h2) {
    q = cat_recursive(p1, h1, SHTRING_NODE_LEFTMOST(p2), h2 - 1);
    if (q == NULL_PTR)
      return NULL_PTR;
    qq = shtring_new_node(n2, h2);
    shtring_add_child(qq, q, &unass, shtring_tree_length(q));
    shtring_add_children(qq, &unass, p2, 2, n2);
    heavy_assert(shtring_check_node(qq));
    return qq;
  } else {
    q = cat_recursive(SHTRING_NODE_RIGHTMOST(p1), h1 - 1, p2, h2);
    if (q == NULL_PTR)
      return NULL_PTR;
    qq = shtring_new_node(n1, h1);
    shtring_add_children(qq, &unass, p1, 1, n1 - 1);
    shtring_add_child(qq, q, &unass, shtring_tree_length(q));
    heavy_assert(shtring_check_node(qq));
    return qq;
  }
  assert(0);
}


/* Catenate two chunk trees. Return pointer to new root node.
   Assume `can_allocate(enough)'.
   
   First, try to catenate using cat_recursive, since that is a memory
   efficient way of doing things. If that fails, create a new root node
   that has the two trees as its children. For this to work, it makes
   the trees of equal height, if they aren't already.  */
static ptr_t cat_trees(ptr_t p1, int h1, ptr_t p2, int h2)
{
  ptr_t q;
  int n, unass;

  q = cat_recursive(p1, h1, p2, h2);
  if (q != NULL_PTR)
    return q;

  unass = 1;
  if (h1 == h2) {
    q = shtring_new_node(2, h1 + 1);
    shtring_add_child(q, p1, &unass, shtring_tree_length(p1));
    shtring_add_child(q, p2, &unass, shtring_tree_length(p2));
  } else if (h1 < h2) {
    n = SHTRING_NODE_CHILDREN(p2);
    if (n < MAX_SHTRING_NODE_CHILDREN) {
      p1 = make_of_height(p1, h2 - 1);
      q = shtring_new_node(n + 1, h2);
      shtring_add_child(q, p1, &unass, shtring_tree_length(p1));
      shtring_add_children(q, &unass, p2, 1, n);
    } else {
      p1 = make_of_height(p1, h2);
      q = shtring_new_node(2, h2 + 1);
      shtring_add_child(q, p1, &unass, shtring_tree_length(p1));
      shtring_add_child(q, p2, &unass, shtring_tree_length(p2));
    }
  } else {
    n = SHTRING_NODE_CHILDREN(p1);
    if (n < MAX_SHTRING_NODE_CHILDREN) {
      p2 = make_of_height(p2, h1 - 1);
      q = shtring_new_node(n + 1, h1);
      shtring_add_children(q, &unass, p1, 1, n);
      shtring_add_child(q, p2, &unass, shtring_tree_length(p2));
    } else {
      p2 = make_of_height(p2, h1);
      q = shtring_new_node(2, h1 + 1);
      shtring_add_child(q, p1, &unass, shtring_tree_length(p1));
      shtring_add_child(q, p2, &unass, shtring_tree_length(p2));
    }
  }

  heavy_assert(shtring_check_node(q));
  return q;
}


/* Copy characters from one chunk to another, empty one. */
static void copy_chunk_text(ptr_t q, ptr_t p, int start)
{
  word_t i, j, len, off;

  heavy_assert(shtring_check_chunk(q));
  heavy_assert(shtring_check_chunk(p));
  assert(start >= 0);
  assert(start <= (int) SHTRING_CHUNK_LENGTH(p));

  off = start % sizeof(word_t);
  i = 1;
  j = 1 + start / sizeof(word_t);

  if (off == 0)
    memcpy(q + i, p + j, shtring_idiv(SHTRING_CHUNK_LENGTH(q), sizeof(word_t)));
  else {
    len = SHTRING_CHUNK_LENGTH(q);
    while (off + len > sizeof(word_t)) {
      q[i] = (p[j] << (off * CHAR_BIT)) | 
      		(p[j+1] >> ((sizeof(word_t)-off) * CHAR_BIT));
      ++i;
      ++j;
      len -= sizeof(word_t);
    }
    q[i] = p[j] << (off * CHAR_BIT);
  }

  heavy_assert(shtring_check_chunk(q));
}


/* Copy tree `p' so that the first `start' characters and the last
   `end' characters are removed from the copy. If copy would be
   identical to `p', return `p', otherwise return pointer to root
   of copy. The copy will be of the same height as `p'. Assume
   `can_allocate(needed_for_trim(p, start, end))'. */
static ptr_t trim_ends(ptr_t p, word_t start, word_t end)
{
  word_t h, i, j, k, len;
  ptr_t q, left, right;
  int unass;

  assert(p != NULL_PTR);  
  assert(start + end < shtring_tree_length(p));
  
  /* Trivial case. */
  if (start == 0 && end == 0)
    return p;
  
  /* Chunks are easy, too. */
  if (CELL_TYPE(p) == CELL_shtring_chunk) {
    len = SHTRING_CHUNK_LENGTH(p);
    q = shtring_new_chunk(len - start - end);
    copy_chunk_text(q, p, start);
    heavy_assert(shtring_check_chunk(q));
    return q;
  }
  
  assert(CELL_TYPE(p) == CELL_shtring_node);

  /* Skip children that can be skipped completely. */
  for (i = 1; start > 0; ++i) {
    assert(i <= SHTRING_NODE_CHILDREN(p));
    len = shtring_tree_length(SHTRING_NODE_CHILD(p,i));
    if (start < len)
      break;
    start -= len;
  }
  for (j = SHTRING_NODE_CHILDREN(p); end > 0 && j > i; --j) {
    len = shtring_tree_length(SHTRING_NODE_CHILD(p,j));
    if (end < len)
      break;
    end -= len;
  }

  /* i is now index of first child (j is last) that is included in result. */
  assert(i <= j);
  q = shtring_new_node(j-i+1, SHTRING_NODE_HEIGHT(p));

  /* Fill in children in new node. */
  left = right = NULL_PTR;
  if (SHTRING_NODE_HEIGHT(p) > 2) {
    if (i == j)
      left = trim_ends(SHTRING_NODE_CHILD(p,i), start, end);
    else {
      left = trim_ends(SHTRING_NODE_CHILD(p,i), start, 0);
      right = trim_ends(SHTRING_NODE_CHILD(p,j), 0, end);
    }
  } else {
    if (i == j) {
      len = SHTRING_CHUNK_LENGTH(SHTRING_NODE_CHILD(p,i));
      left = shtring_new_chunk(len - start - end);
      copy_chunk_text(left, SHTRING_NODE_CHILD(p,i), start);
    } else {
      len = SHTRING_CHUNK_LENGTH(SHTRING_NODE_CHILD(p,i));
      left = shtring_new_chunk(len - start);
      copy_chunk_text(left, SHTRING_NODE_CHILD(p,i), start);

      len = SHTRING_CHUNK_LENGTH(SHTRING_NODE_CHILD(p,j));
      right = shtring_new_chunk(len - end);
      copy_chunk_text(right, SHTRING_NODE_CHILD(p,i), 0);
    }
  }

  unass = 1;
  shtring_add_child(q, left, &unass, shtring_tree_length(left));
  for (k = i+1; k < j; ++k)
    shtring_add_child(q, SHTRING_NODE_CHILD(p,k), &unass, shtring_tree_length(SHTRING_NODE_CHILD(p,k)));
  if (right != NULL_PTR)
    shtring_add_child(q, right, &unass, shtring_tree_length(right));

  /* Skip unnecessary nodes in tree (root has 1 child). */
  if (SHTRING_NODE_CHILDREN(q) == 1) {
    h = SHTRING_NODE_HEIGHT(q);
    while (h > 1 && SHTRING_NODE_CHILDREN(q) == 1) {
      --h;
      q = SHTRING_NODE_CHILD(q, 1);
    }
  }

  heavy_assert(shtring_check_node(q));
  return q;
}


/* Catenate two shtrings. */
ptr_t shtring_cat(ptr_t shtr1, ptr_t shtr2)
{
  ptr_t q, p1, p2;

  heavy_assert(shtring_check_shtring(shtr1));
  heavy_assert(shtring_check_shtring(shtr2));

  p1 = SHTRING_CHUNK_TREE(shtr1);
  p2 = SHTRING_CHUNK_TREE(shtr2);

  if (p1 == NULL_PTR)
    return shtr2;
  else if (p2 == NULL_PTR)
    return shtr1;

  p1 = trim_ends(p1, 0, 
	  shtring_tree_length(p1) - SHTRING_OFFSET(shtr1) - SHTRING_LENGTH(shtr1));
  p2 = trim_ends(p2, SHTRING_OFFSET(shtr2), 0);
  
  q = cat_trees(p1, SHTRING_HEIGHT(shtr1), p2, SHTRING_HEIGHT(shtr2));
  return shtring_new_shtring(SHTRING_OFFSET(shtr1), 
  			SHTRING_LENGTH(shtr1) + SHTRING_LENGTH(shtr2), 
			SHTRING_NODE_HEIGHT(q), q);
}




/* Make a new shtring out of an existing shtring, or a part of it. */

/* Return root of minimal subtree that contains the desired sequence of
   characters. Note: no modification of the tree is required. Reduce
   `*start' with the length of discarded chunks. */
static ptr_t minimal_subtree(ptr_t p, word_t *start, word_t len)
{
  ptr_t child;
  word_t h, i, child_len, shtring_new_start;

  assert(*start <= shtring_tree_length(p));
  assert(len <= shtring_tree_length(p));
  assert(*start + len <= shtring_tree_length(p));
  
  h = shtring_tree_height(p);
  shtring_new_start = *start;
  while (h > 1) {
    for (i = 1; ; ++i) {
      assert(i <= SHTRING_NODE_CHILDREN(p));
      child = SHTRING_NODE_CHILD(p, i);
      if (h == 2)
	child_len = SHTRING_CHUNK_LENGTH(child);
      else
	child_len = SHTRING_NODE_LENGTH(child);
      if (child_len > shtring_new_start)
	break;
      shtring_new_start -= child_len;
    }
    assert(child == SHTRING_NODE_CHILD(p, i));
    assert(child_len == shtring_tree_length(child));
    
    if (i < SHTRING_NODE_CHILDREN(p) && child_len < shtring_new_start + len)
      break;
    *start = shtring_new_start;
    p = child;
    --h;
  
    assert(h == shtring_tree_height(p));
    assert(*start <= shtring_tree_length(p));
    assert(len <= shtring_tree_length(p));
    assert(*start + len <= shtring_tree_length(p));
  }

  return p;
}


/* Make a new shtring that is a copy of some part of an existing shtring.
   Assume `can_allocate(SHTRING_WORDS)'. */
ptr_t shtring_copy(ptr_t shtr, word_t start, word_t len)
{
  ptr_t tree, new_shtr;
  word_t offset;

  heavy_assert(shtring_check_shtring(shtr));
  assert(start <= SHTRING_LENGTH(shtr));
  assert(len <= SHTRING_LENGTH(shtr));
  assert(start + len <= SHTRING_LENGTH(shtr));

  tree = SHTRING_CHUNK_TREE(shtr);
  offset = SHTRING_OFFSET(shtr);
  
  offset += start;
  tree = minimal_subtree(tree, &offset, len);

  new_shtr = shtring_new_shtring(offset, len, shtring_tree_height(tree), tree);
  heavy_assert(shtring_check_shtring(new_shtr));
  return new_shtr;
}




/* Copy characters from a shtring to a C string. */

/* Copy characters from chunk to C character array. Start with character
   at offset `start' in chunk, and copy `len' characters. `cstr' is
   assumed to be big enough; it will _not_ be zero terminated. */
static void copy_chunk_to_cstr(char *cstr, ptr_t p, int start, int len)
{
  int i, off;
  word_t w;

  assert(CHAR_BIT == 8);
  assert(sizeof(word_t) == 4);
  assert(cstr != NULL);
  heavy_assert(shtring_check_chunk(p));
  assert(start >= 0);
  assert(len >= 0);
  assert(start <= (int) SHTRING_CHUNK_LENGTH(p));
  assert(start + len <= (int) SHTRING_CHUNK_LENGTH(p));
  
#if WORDS_BIGENDIAN
  /* XXX NOTE: This has not yet been tested properly. */
  memcpy(cstr, (char *)(p+1) + start, len);
#else
  if (len == 0)
    return;

  i = 1 + start / sizeof(word_t);
  off = start % sizeof(word_t);

  if (off > 0) {
    w = p[i];
    switch (off) {
    case 1:                *cstr++ = (w >> (2*CHAR_BIT)) & CHAR_MASK; --len;
    case 2: if (len > 0) { *cstr++ = (w >> CHAR_BIT) & CHAR_MASK; --len; }
    case 3: if (len > 0) { *cstr++ = w & CHAR_MASK;         --len; }
    }
    ++i;
  }

  while (len >= (int) sizeof(word_t)) {
    w = p[i];
    *cstr++ = (w >> 24) & CHAR_MASK;
    *cstr++ = (w >> 16) & CHAR_MASK;
    *cstr++ = (w >>  8) & CHAR_MASK;
    *cstr++ = w & CHAR_MASK;
    ++i;
    len -= sizeof(word_t);
  }

  if (len > 0) {
    w = p[i];
    do {
      *cstr++ = (w >> (3*CHAR_BIT)) & CHAR_MASK;
      w <<= CHAR_BIT;
    } while (--len > 0);
  }
#endif
}


/* Find chunk in chunk tree that contains position `pos'. */
static void find_chunk(ptr_t *chunk, word_t * start, word_t * root, word_t pos)
{
  word_t h, i, n, len;
  ptr_t q;
  
  assert(chunk != NULL);
  heavy_assert(shtring_check_chunk(root) || shtring_check_node(root));
  assert(pos < shtring_tree_length(root));

  *start = 0;
  h = shtring_tree_height(root);
  while (h > 1) {
    n = SHTRING_NODE_CHILDREN(root);
    for (i = 1; ; ++i) {
      assert(i <= n);
      q = SHTRING_NODE_CHILD(root, i);
      if (h == 2)
	len = SHTRING_CHUNK_LENGTH(q);
      else
	len = SHTRING_NODE_LENGTH(q);
      if (pos < *start + len)
	break;
      *start += len;
    }
    root = q;
    --h;
    assert(h == shtring_tree_height(root));
  }
  *chunk = root;
  assert(CELL_TYPE(root) == CELL_shtring_chunk);
}


/* Return the character at a given position in a shtring. */
int shtring_charat(ptr_t shtr, word_t pos)
{
  word_t start;
  ptr_t p;
  char ch;

  heavy_assert(shtring_check_shtring(shtr));
  assert(pos < SHTRING_LENGTH(shtr));

  pos += SHTRING_OFFSET(shtr);
  find_chunk(&p, &start, SHTRING_CHUNK_TREE(shtr), pos);
  copy_chunk_to_cstr(&ch, p, pos - start, 1);
  return ch;
}


/* Return the characters at a given range in a shtring in a C array. */

#if 1
static int collect_string(ptr_t chunk, word_t off_in_chunk, word_t off_in_tree,
			  word_t len, void *arg)
{
  word_t copy_from, copy_len;
  char **p;

  assert(chunk != NULL_PTR);
  assert(CELL_TYPE(chunk) == CELL_shtring_chunk);
  assert(arg != NULL);
  assert(off_in_chunk + len <= SHTRING_CHUNK_LENGTH(chunk));

  p = arg;
  copy_chunk_to_cstr(*p, chunk, off_in_chunk, len);
  *p += len;

  return 0;
}
#endif

void shtring_strat(char *cstr, ptr_t shtr, word_t pos, word_t len)
{
  char *arg;
  word_t aux;

  arg = cstr;
  aux = SHTRING_MONKEY_START_POS;
  shtring_monkey(SHTRING_CHUNK_TREE(shtr), SHTRING_OFFSET(shtr) + pos, len, 
  	&aux, collect_string, left_to_right, &arg);
  assert(arg == cstr + len);
}


/* Return the length of a shtring. */
word_t shtring_length(ptr_t shtr)
{
  heavy_assert(shtring_check_shtring(shtr));
  return SHTRING_LENGTH(shtr);
}




/* Compare two shtrings. Return a cmp_result_t (see cells.h). 

   The comparison is done by comparing the ith character in the
   first shtring to the ith character of the second one. Note
   that two shtrings may have differently shaped chunk trees,
   and the chunk sequence may have leading and trailing parts
   that are ignored. These, of course, do not affect the
   comparison.
   
   The comparison is done by traversing the two chunk
   trees in parallel. This is implemented with the help of
   `cursors', which indicate character positions in the chunk
   sequences. First, a cursor is set to the beginning of each
   shtring, and the characters at the cursors are compared. If
   they are same, the cursors are advanced and the next
   characters are compared, and so on, until a difference is
   found, or the end of one shtring is reached.  For efficiency,
   several characters at a time are compared.
   
   XXX the above is probably obsolete
   
*/

#define CMP_BUF	10240

cmp_result_t shtring_cmp_without_allocating(ptr_t shtr1, ptr_t shtr2)
{
  char buf1[CMP_BUF];
  char buf2[CMP_BUF];
  word_t n, len, len1, len2, pos;
  int ret;

  heavy_assert(shtring_check_shtring(shtr1));
  heavy_assert(shtring_check_shtring(shtr2));

  len1 = SHTRING_LENGTH(shtr1);
  len2 = SHTRING_LENGTH(shtr2);
  if (len1 < len2)
    len = len1;
  else
    len = len2;

  n = CMP_BUF;
  for (pos = 0; pos < len; pos += n) {
    if (pos + CMP_BUF > len)
      n = len - pos;
    shtring_strat(buf1, shtr1, pos, n);
    shtring_strat(buf2, shtr2, pos, n);
    ret = memcmp(buf1, buf2, n);
    if (ret < 0)
      return CMP_LESS;
    else if (ret > 0)
      return CMP_GREATER;
  }
  
  if (len1 < len2)
    return CMP_LESS;
  else if (len1 > len2)
    return CMP_GREATER;
  return CMP_EQUAL;
}


#if 0

word_t shtring_cmp_max_allocation(ptr_t shtr1, ptr_t shtr2)
{
  /* XXX magic numbers, foo */
  return 32*shtring_cursor_max_allocation(shtr1) +
         32*shtring_cursor_max_allocation(shtr2);
}

/* Compare two shtrings. */
cmp_result_t shtring_cmp(ptr_t shtr1, ptr_t shtr2)
{
  ptr_t cur1, cur2;
  cmp_result_t result;
  
  cur1 = shtring_cursor_create(shtr1);
  cur2 = shtring_cursor_create(shtr2);

  while ((result = shtring_cmp_new(&cur1, &cur2, 1024)) == CMP_PLEASE_CONTINUE)
    continue;
  return result;
}

#endif




/* Shtring hashing. This is used by shtring interning (next section),
   but may be useful by others as well, so it is exported. */

/* Add the text in a chunk to the hash. */
static int hash_chunk(ptr_t chunk, word_t off_in_chunk, word_t off_in_tree,
		      word_t len, void *arg)
{
  word_t h, i, w, c;

  assert(CHAR_BIT == 8);

  h = *(word_t *) arg;

  chunk += 1 + off_in_chunk/sizeof(word_t);
  off_in_chunk %= sizeof(word_t);
  if (off_in_chunk > 0) {
    w = *chunk;
    ++chunk;
    w <<= CHAR_BIT * off_in_chunk;
    for (i = sizeof(word_t) - off_in_chunk; i > 0; --i) {
      c = w & (0xFF << (sizeof(word_t) - 1) * CHAR_BIT);
      c = c >> (sizeof(word_t) - 1) * CHAR_BIT;
      h = (h << 5) + h + (h >> 3) + c;	/* XXX constants assume 8 bit byte? */
      w <<= 8;	/* XXX should be CHAR_BIT, below also? */
    }
    len -= off_in_chunk;
  }

  while (len >= sizeof(word_t)) {
    w = *chunk;
    chunk++;
    for (i = 0; i < sizeof(word_t); ++i) {
      c = w & (0xFF << (sizeof(word_t) - 1) * CHAR_BIT);
      c = c >> (sizeof(word_t) - 1) * CHAR_BIT;
      h = (h << 5) + h + (h >> 3) + c;	/* XXX constants assume 8 bit byte? */
      w <<= 8;	/* XXX should be CHAR_BIT, below also? */
    }
    len -= sizeof(word_t);
  }
    
  if (len > 0) {
    w = *chunk;
    for (; len > 0; --len) {
      c = w & (0xFF << (sizeof(word_t) - 1) * CHAR_BIT);
      c = c >> (sizeof(word_t) - 1) * CHAR_BIT;
      h = (h << 5) + h + (h >> 3) + c;
      w <<= 8;
    }
  }
  
  *(word_t *) arg = h;
  return 0;
}


/* Hash function for a shtring. I have no idea if this hash ish good,
   the algorithm is copied from Sirkku's string_intern.c module. 
   However, it looks complicated, so it probably works. :)  */
word_t shtring_hash(ptr_t shtr)
{
  word_t h, pos;
  
  h = SHTRING_HASH_START_VALUE;
  pos = SHTRING_MONKEY_START_POS;
  (void) shtring_monkey(SHTRING_CHUNK_TREE(shtr), SHTRING_OFFSET(shtr),
  		SHTRING_LENGTH(shtr), &pos, hash_chunk, left_to_right, &h);
  return h;
}



/* Shtring interning. 

   Interning means converting a string to a unique integer
   so that two strings with the same content get the same
   integer. For example, if "foo" becomes 1 and "bar" becomes
   2, and a new "foo" (stored in another location, perhaps)
   is converted, it also becomes 1. This will reduce storage
   requirement and make it easier and faster to compare strings
   for equality.
   
   Interning of shtrings is implemented by keeping a search
   structure with all the shtrings that have been interned
   already, and their unique integers. When a new shtring is
   interned, it is looked up in the search structure.  If found,
   the stored integer is returned; otherwise, a new integer is
   generated and added with the shtring to the search structure.
   
   Note, there is no way to remove shtrings from the search structure.
   
   This implementation uses the trie module, and uses a hash
   function (shtring_hash) to create a search key for shtrings.
   Shtrings with the same hash value are stored in a linked list
   in the nodes of the trie.
   
*/


/* Allocate and initialize a new CELL_shtring_intern_node. */
static ptr_t new_intern_node(ptr_t next, ptr_t shtr, word_t id)
{
  ptr_t p;
  
  p = allocate(4, CELL_shtring_intern_node);
  p[1] = PTR_TO_WORD(next);
  p[2] = PTR_TO_WORD(shtr);
  p[3] = id;
  return p;
}


/* Allocate and initialize a new CELL_shtring_intern_root. */
static ptr_t new_intern_root(ptr_t hash_trie, ptr_t id_trie, word_t count)
{
  ptr_t p;
  
  p = allocate(4, CELL_shtring_intern_root);
  p[1] = PTR_TO_WORD(hash_trie);
  p[2] = PTR_TO_WORD(id_trie);
  p[3] = count;
  return p;
}


/* Intern a shtring. `root' is the set of interned shtrings so far.
   It is initially NULL_PTR, but potentially a new value is returned
   after each operation.  */
struct shtring_intern_result shtring_intern(ptr_t root, ptr_t shtr)
{
  word_t hash, old_count, new_id;
  ptr_t q, list, old_node, new_node, old_hash_trie, new_hash_trie;
  ptr_t old_id_trie, new_id_trie;
  struct shtring_intern_result result;

  assert(root == NULL_PTR || CELL_TYPE(root) == CELL_shtring_intern_root);
  assert(CELL_TYPE(shtr) == CELL_shtring);

  /* To be, or not to be... */
  hash = shtring_hash(shtr);
  if (root == NULL_PTR) {
    old_hash_trie = NULL_PTR;
    old_id_trie = NULL_PTR;
    old_count = 0;
    list = NULL_PTR;
  } else {
    old_hash_trie = WORD_TO_PTR(root[1]);
    old_id_trie = WORD_TO_PTR(root[2]);
    old_count = root[3];
    list = WORD_TO_PTR(triev2_find(old_hash_trie, hash, SHTRING_HASH_BITS));
    for (q = list; q != NULL_PTR; q = WORD_TO_PTR(q[1])) {
      old_node = WORD_TO_PTR(q[2]);
      if (shtring_cmp_without_allocating(old_node, shtr) == CMP_EQUAL) {
	result.new_intern_root = root;
	result.interned_shtring = old_node;
	result.interned_shtring_id = q[3];
	result.was_new = 0;
	return result;
      }
    }
  }
  
  /* ...what's the question? */
  assert(old_count < (word_t) ~0UL);	/* XXX this should probably be
  					   handled as an error condition */
  new_id = old_count + 1;
  new_node = new_intern_node(list, shtr, new_id);
  new_hash_trie = triev2_insert(old_hash_trie, hash, SHTRING_HASH_BITS, 
  			NULL, NULL, PTR_TO_WORD(new_node));
  new_id_trie = triev2_insert(old_id_trie, new_id, CHAR_BIT * sizeof(new_id),
  			      NULL, NULL, PTR_TO_WORD(shtr));
  heavy_assert(WORD_TO_PTR(triev2_find(new_id_trie, new_id, CHAR_BIT*sizeof(new_id))) == shtr);
  result.new_intern_root = new_intern_root(new_hash_trie, new_id_trie, 
  					   new_id);
  result.interned_shtring = shtr;
  result.interned_shtring_id = new_id;
  result.was_new = 1;
  return result;
}


/* Find a shtring given its id. `root' is the set of interned shtrings,
   implemented as a trie. Return a pointer to the shtring, or NULL_PTR
   if the shtring has not been interned. */
ptr_t shtring_lookup_by_intern_id(ptr_t root, word_t id)
{
  ptr_t id_trie = WORD_TO_PTR(root[2]);
  return WORD_TO_PTR(triev2_find(id_trie, id, CHAR_BIT * sizeof(id)));
}





/* Converting shtrings to numbers. */

shtring_to_result_t shtring_to_long(long *result, word_t *end_offset, 
					ptr_t shtr, int base)
{
  word_t max;
  char buffer[4096];
  char *buffer_end;

  max = SHTRING_LENGTH(shtr);
  if (max >= sizeof(buffer))
    max = sizeof(buffer) - 1;
  shtring_strat(buffer, shtr, 0, max);
  buffer[max] = '\0';

  errno = 0;
  *result = strtol(buffer, &buffer_end, base);
  if ((*result == LONG_MIN || *result == LONG_MAX) && errno == ERANGE)
    return SHTRING_ABS_TOO_BIG;
  *end_offset = buffer_end - buffer;
  if (*end_offset == 0)
    return SHTRING_NOT_A_NUMBER;
  return SHTRING_OK;
}

shtring_to_result_t shtring_to_ulong(unsigned long *result, word_t *end_offset,
					 ptr_t shtr, int base)
{
  word_t max;
  char buffer[4096];
  char *buffer_end;

  max = SHTRING_LENGTH(shtr);
  if (max >= sizeof(buffer))
    max = sizeof(buffer) - 1;
  shtring_strat(buffer, shtr, 0, max);
  buffer[max] = '\0';

  errno = 0;
  *result = strtoul(buffer, &buffer_end, base);
  if (*result == ULONG_MAX && errno == ERANGE)
    return SHTRING_ABS_TOO_BIG;
  *end_offset = buffer_end - buffer;
  if (*end_offset == 0)
    return SHTRING_NOT_A_NUMBER;
  return SHTRING_OK;
}




/* Various small utility functions for manipulating shtrings. */

/* XXX could trim_leading and _trailing be combined into a single
   work-horse function? */

static int find_first_non_ws(ptr_t chunk, word_t off_in_chunk, 
			     word_t off_in_tree, word_t len, void *arg)
{
  char buf[SHTRING_CHUNK_MAX];
  word_t i;
  
  copy_chunk_to_cstr(buf, chunk, off_in_chunk, len);
  for (i = 0; i < len && isspace(buf[i]); ++i)
    continue;
  *(word_t *) arg = off_in_tree + i;
  if (i < len)
    return -1;
  return 0;
}

ptr_t shtring_trim_leading(ptr_t shtr)
{
  word_t off, pos, start, len, first_non_ws;
  ptr_t root;
  
  pos = SHTRING_MONKEY_START_POS;
  root = SHTRING_CHUNK_TREE(shtr);
  off = SHTRING_OFFSET(shtr);
  len = SHTRING_LENGTH(shtr);
  first_non_ws = 0;
  (void) shtring_monkey(root, off, len, &pos, find_first_non_ws, left_to_right,
  		&first_non_ws);
  if (first_non_ws == 0)
    return shtr;	/* No leading whitespace */

  start = off + first_non_ws;
  len -= first_non_ws;
  root = minimal_subtree(root, &start, len);
  return shtring_new_shtring(start, len, shtring_tree_height(root), root);
}


static int find_last_non_ws(ptr_t chunk, word_t off_in_chunk, 
			     word_t off_in_tree, word_t len, void *arg)
{
  char buf[SHTRING_CHUNK_MAX];
  word_t i, j, *p;

  if (len == 0)
    return 0;

  copy_chunk_to_cstr(buf, chunk, off_in_chunk, len);
#if 0
  i = len;
  do {
    --i;
    if (!isspace(buf[i]))
      break;
  } while (i > 0);
  if (i == 0 && isspace(buf[i]))
    return 0;
#endif
  for (i = 0; i < len; ++i)
    if (!isspace(buf[i]))
      break;
  if (i == len)
    return 0;
  *(word_t *) arg = off_in_tree + i;
  return -1;
}

ptr_t shtring_trim_trailing(ptr_t shtr)
{
  word_t off, pos, start, len, last_non_ws;
  ptr_t root;
  
  pos = SHTRING_MONKEY_START_POS;
  root = SHTRING_CHUNK_TREE(shtr);
  off = SHTRING_OFFSET(shtr);
  len = SHTRING_LENGTH(shtr);
  last_non_ws = (word_t) -1;
  (void) shtring_monkey(root, off, len, &pos, find_last_non_ws, right_to_left,
  		&last_non_ws);
  if (last_non_ws == (word_t) -1)	/* Only whitespace */
    return shtring_new_shtring(0, 0, 0, NULL_PTR);
  if (last_non_ws + 1 == len)
    return shtr;	/* No leading whitespace */

  start = 0;
  len = last_non_ws + 1;
  root = minimal_subtree(root, &start, len);
  return shtring_new_shtring(start, len, shtring_tree_height(root), root);
}




/* Debugging tools that can be used from outside this module. */

#if 0

/* Print debugging information about a chunk to a file. */
static void dump_chunk(ptr_t p, FILE *f)
{
  word_t i, j, n, x, len;
  int c;
  
  heavy_assert(shtring_check_chunk(p));
  assert(f != NULL);

  len = SHTRING_CHUNK_LENGTH(p);
  n = sizeof(word_t);
  
  for (i = 0; i < len; ++i) {
    x = p[1 + i/n];
    j = (n - i%n - 1) * CHAR_BIT;
    c = (x >> j) & CHAR_MASK;
    fprintf(f, "%c", c);
  }
}


/* Print debugging information for a chunk tree. */
static void dump_tree(ptr_t p, FILE *f)
{
  int i;

  assert(f != NULL);

  if (p == NULL_PTR)
    return;
  if (CELL_TYPE(p) == CELL_shtring_chunk)
    dump_chunk(p, f);
  else {
    heavy_assert(shtring_check_node(p));
    for (i = 1; i <= (int) SHTRING_NODE_CHILDREN(p); ++i)
      dump_tree(SHTRING_NODE_CHILD(p, i), f);
  }
}


/* Write debugging information about `shtr' to `f'. */
void shtring_fprint(FILE *f, ptr_t shtr)
{
  heavy_assert(shtring_check_shtring(shtr));
  assert(f != NULL);
  dump_tree(SHTRING_CHUNK_TREE(shtr), f);
}

#else

static int fprint_chunk(ptr_t chunk, word_t off_in_chunk,
			word_t off_in_tree, word_t len, void *arg)
{
  char buf[SHTRING_CHUNK_MAX];
  word_t n;

  assert(chunk != NULL_PTR);
  assert(CELL_TYPE(chunk) == CELL_shtring_chunk);
  assert(arg != NULL);
  assert(off_in_chunk + len <= SHTRING_CHUNK_LENGTH(chunk));

  while (len > 0) {
    if (len > SHTRING_CHUNK_MAX)
      n = SHTRING_CHUNK_MAX;
    else
      n = len;
    copy_chunk_to_cstr(buf, chunk, off_in_chunk, n);
    (void) fwrite(buf, 1, n, arg);
    len -= n;
  }

  return 0;
}

/* Write text of shtring to open file. XXX Ignores write errors. */
void shtring_fprint(FILE *f, ptr_t shtr)
{
  word_t pos;

  heavy_assert(shtring_check_shtring(shtr));
  assert(f != NULL);
  
  pos = SHTRING_MONKEY_START_POS;
  (void) shtring_monkey(SHTRING_CHUNK_TREE(shtr), 0, SHTRING_LENGTH(shtr),
  			&pos, fprint_chunk, left_to_right, f);
}

#endif
