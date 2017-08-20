/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Sirkku Paajanen <sirkku@iki.fi>
 */

/* Auxiliary routines for different test programs.
 */

#include "test_aux.h"
#include <sys/times.h>
#include <math.h>

static char *rev_id = "$Id: test_aux.c,v 1.17 1998/02/17 16:02:25 sirkku Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

/* Returns wall clock time in seconds since the beginning of the
   program.  Typical use involves two calls; one before and one after
   the benchmark, and the consumed time is the difference of these two
   times. */

double give_time(void)
{
  struct tms tms;
  long ticks_per_second;
  long ticks;

  ticks = times(&tms);
  if (ticks == -1) {
    fprintf(stderr, "'times' failed\n");
    exit(1);
  }
  ticks_per_second = sysconf(_SC_CLK_TCK);
  if (ticks_per_second == -1) {
    fprintf(stderr, "'sysconf(_SC_CLK_TCK)' failed.\n");
    exit(1);
  }
  return ticks / (double) ticks_per_second;
}


/* Approximately Zipfian random number generator. */

zipf_t *zipf_init(int N, double p)
{
  zipf_t *z;

  z = malloc(sizeof(zipf_t));
  if (z == NULL) {
    fprintf(stderr, "zipf_init: malloc failed.\n");
    exit(1);
  }
  z->N = N;

  if (fabs(p) < 0.0001)
    z->p = 0;
  else if (fabs(p - 1) < 0.0001) {
    z->p = p = 1;
    z->c1 = log(N + 1);
  } else {
    z->c1 = pow(N+1, 1-p) - 1;
    z->c2 = 1 / (1-p);
  }

  return z;
}

int zipf(zipf_t *z)
{
  int x;
  double xr;

  if (z->p == 0)
    return 1 + (random() % z->N);

  do {
    x = random();
  } while (x == 0);
  xr = x / (double) 0x7FFFFFFF;
  if (z->p == 1)
    xr = exp(xr * z->c1);
  else
    xr = pow(xr * z->c1 + 1, z->c2);
  return (int) xr;
}



/* Numbers from 0 to 255 in random order and split so that bits 6 and
   7 have been moved to bit positions 30 and 31, bits 4 and 5 have
   been moved to 22 and 23, bits 2 and 3 have been moved to 14 and 15,
   and bits 0 and 1 to bits 7 and 8, all other bits being zero.  The
   entropy of the table is almost 1684 bits.  */
static word_t scatter_bits[256] = {
  0xC080C040, 0x000040C0, 0xC0404040, 0x8000C0C0, 0x40000000, 0xC0C04080,
  0x00008000, 0xC0800080, 0xC0400080, 0x00C00000, 0x80400080, 0x00C0C000,
  0x808000C0, 0x80C04000, 0x80C0C000, 0x00800080, 0x00804040, 0x808040C0,
  0x00400000, 0x804000C0, 0xC04000C0, 0x40808000, 0xC0C00000, 0x00800040,
  0x8040C000, 0xC08080C0, 0x00C0C040, 0xC0004040, 0x00404080, 0x40004000,
  0xC080C000, 0x00C04000, 0x40808080, 0x80C00000, 0xC040C080, 0xC0800040,
  0x8080C080, 0xC000C040, 0x00C08040, 0xC04040C0, 0x80008040, 0x40408080,
  0x400040C0, 0xC040C040, 0x40C000C0, 0x0080C040, 0x0080C080, 0x80400040,
  0xC000C0C0, 0x00000080, 0xC0C08040, 0x0040C000, 0x004040C0, 0xC0C080C0,
  0x000080C0, 0x00C00040, 0x8080C040, 0x00808040, 0xC0000000, 0x8040C080,
  0x40C08040, 0xC0404000, 0x800000C0, 0x404000C0, 0x40800080, 0xC0400000,
  0xC0804000, 0x80004040, 0x4040C0C0, 0xC0808080, 0x00404040, 0xC0C00040,
  0x4000C0C0, 0x4080C080, 0x00408000, 0x004080C0, 0x80C00080, 0x00C08000,
  0x80004080, 0xC0008000, 0x400000C0, 0x40C040C0, 0x40C00080, 0x40004080,
  0x40C00000, 0x80C04080, 0x00C04040, 0x4000C080, 0x40C08080, 0x00008040,
  0x00400040, 0x40004040, 0x80C0C040, 0x00408080, 0xC0C04000, 0x4080C040,
  0x4040C000, 0xC0C04040, 0xC00000C0, 0x8000C080, 0xC0804040, 0xC0C0C080,
  0xC0404080, 0x80800000, 0xC0C08080, 0x00004000, 0x40C04000, 0x40400040,
  0x40404040, 0x40C080C0, 0x804040C0, 0x80C08040, 0x80000000, 0xC0408000,
  0x004000C0, 0x40400000, 0x80000080, 0x40400080, 0x00000040, 0xC000C000,
  0x80004000, 0xC080C080, 0x00400080, 0x008080C0, 0x00804000, 0x40408040,
  0x40C0C080, 0x4080C000, 0x00C0C080, 0x40C00040, 0x40C04080, 0x80808040,
  0xC0004000, 0xC000C080, 0xC08040C0, 0x80000040, 0x0040C080, 0x80808000,
  0x0000C0C0, 0x8080C0C0, 0xC0C0C040, 0x00008080, 0x00C080C0, 0x0080C000,
  0x00800000, 0x4040C040, 0xC0408080, 0x00404000, 0x8040C0C0, 0xC0000080,
  0x408080C0, 0x40804080, 0x0000C080, 0xC0C0C000, 0xC00040C0, 0x80C040C0,
  0xC0800000, 0x400080C0, 0xC0C0C0C0, 0x40808040, 0x00C000C0, 0xC0C00080,
  0x80804040, 0x80008000, 0xC08000C0, 0x00C00080, 0x4080C0C0, 0x80800080,
  0x00C04080, 0x0080C0C0, 0x40408000, 0x80C000C0, 0x80C08000, 0x80404040,
  0x808080C0, 0x0040C0C0, 0x40804000, 0x0000C040, 0x404040C0, 0x80408000,
  0x408040C0, 0xC0004080, 0xC0808000, 0x80C04040, 0x804080C0, 0x40C08000,
  0x000000C0, 0x4000C040, 0x00004080, 0x00804080, 0x00808000, 0x80C00040,
  0xC04080C0, 0x00C0C0C0, 0x008040C0, 0x40404080, 0xC0804080, 0x40C0C040,
  0x800080C0, 0x80404080, 0xC0400040, 0x0040C040, 0x008000C0, 0xC040C0C0,
  0x40008040, 0x00408040, 0x0000C000, 0x80400000, 0x40804040, 0x40C0C000,
  0x404080C0, 0x40008000, 0x00000000, 0x80008080, 0x4040C080, 0xC00080C0,
  0xC0C08000, 0xC080C0C0, 0x80404000, 0x8080C000, 0xC0C000C0, 0xC0008040,
  0x40C0C0C0, 0x40000080, 0x40000040, 0x80800040, 0x80804000, 0x408000C0,
  0xC0000040, 0x40404000, 0x40C04040, 0x40008080, 0x00C08080, 0x40800040,
  0x800040C0, 0x8040C040, 0xC0408040, 0x80C08080, 0x80C0C0C0, 0x80408080,
  0x4000C000, 0x80808080, 0xC0008080, 0x80C080C0, 0x8000C000, 0x00808080,
  0x80804080, 0x00004040, 0x40800000, 0x00C040C0, 0x8000C040, 0x80408040,
  0xC040C000, 0xC0808040, 0x80C0C080, 0xC0C040C0
};

word_t scatter(word_t x)
{
  word_t y;
  int i;

  /* It has been verified experimentally with the chi-square-test that
     this is just as good a scatterer function for `word_t's as a good
     random number generator. */
  for (i = 0; i < 6; i++) {
    y = scatter_bits[x & 0xFF];
    x >>= 8;
    y >>= 2;
    y |= scatter_bits[x & 0xFF];
    x >>= 8;
    y >>= 2;
    y |= scatter_bits[x & 0xFF];
    x >>= 8;
    y >>= 2;
    x = y | scatter_bits[x];
  }
  return x;
}


/* AVL-routines. */

/* Define the height of the given node. */
#define HEIGHT(p) 				\
  ((p)->left->height > (p)->right->height	\
   ? (p)->left->height + 1			\
   : (p)->right->height + 1)

/* Define the balance of given node. The balance is 
   height of rigth son - height of left son. */     
#define BALANCE(p)  ((p)->right->height - (p)->left->height)
     
/* Next four functions are "rotate" operations that balance the tree.
   These functions are called when the AVL-property: the balance of
   every node in the tree is -1, 0 or 1, violates. */
static void srl(avl_destr_node_t **p) 
{
  avl_destr_node_t *q;
  word_t a;
  
  q = (*p)->right;
  assert(q != NULL);
  
  (*p)->right = q->right;
  q->right = q->left;
  q->left = (*p)->left;
  (*p)->left = q;

  a = (*p)->key;
  (*p)->key = q->key;
  q->key = a;

  q->height = HEIGHT(q);
  (*p)->height = HEIGHT(*p);
  
  return;
}

static void srr(avl_destr_node_t **p) 
{
  avl_destr_node_t *q;
  word_t a;

  q = (*p)->left;
  assert(q != NULL);

  (*p)->left = q->left;
  q->left = q->right;
  q->right = (*p)->right;
  (*p)->right = q;

  a = (*p)->key;
  (*p)->key = q->key;
  q->key = a;

  q->height = HEIGHT(q);
  (*p)->height = HEIGHT(*p);

  return;
}

static void drl(avl_destr_node_t **p) 
{
  avl_destr_node_t *q;
  word_t a;

  assert((*p)->right != NULL);
  q = (*p)->right->left;
  assert(q != NULL);

  (*p)->right->left = q->right;
  q->right = q->left;
  q->left = (*p)->left;
  (*p)->left = q;

  a = (*p)->key;
  (*p)->key = q->key;
  q->key = a;

  q->height = HEIGHT(q);
  (*p)->right->height = HEIGHT((*p)->right);
  (*p)->height = HEIGHT(*p);

  return;
}

static void drr(avl_destr_node_t **p)
{
  avl_destr_node_t *q;
  word_t a;

  assert((*p)->left != NULL);
  q = (*p)->left->right;
  assert(q != NULL);

  (*p)->left->right = q->left;
  q->left = q->right;
  q->right = (*p)->right;
  (*p)->right = q;

  a = (*p)->key;
  (*p)->key = q->key;
  q->key = a;

  q->height = HEIGHT(q);
  (*p)->left->height = HEIGHT((*p)->left);
  (*p)->height = HEIGHT(*p);

  return;
}


/* Find the given key. Return 1 if succeed, else return 0. */
int avl_destr_find(avl_destr_node_t *p, word_t key) 
{
  if (p == NULL)
    return 0;
  if (p->left == NULL)
    return (p->key == key) ? 1 : 0; 
  else if (key <= p->key)
    return avl_destr_find(p->left, key);
  else
    return avl_destr_find(p->right, key);
}

/* Find the smallest key in the tree. Return 0 if failed. */
int avl_destr_find_min(avl_destr_node_t *p, word_t *key_p)
{
  if (p == NULL)
    return 0;
  if (p->left == NULL) {
    *key_p = p->key;
    return 1;
  }
  return avl_destr_find_min(p->left, key_p);
}


/* Find the greatest key in the tree. Return 0 if failed. */
int avl_destr_find_max(avl_destr_node_t *p, word_t *key_p)
{
  if (p == NULL)
    return 0;
  if (p->right == NULL) {
    *key_p = p->key;
    return 1;
  }
  return avl_destr_find_max(p->right, key_p);
}


/* Find smallest key greater than the given key. If operation 
   fails, return 0, else store the found key in `*key_p' and return 1. */
int avl_destr_find_at_least(avl_destr_node_t *p, word_t *key_p)
{
  if (p == NULL)
    return 0;
  if (p->left == NULL) {
    if (p->key >= *key_p) {
      *key_p = p->key;
      return 1;
    } 
    return 0;
  } else if (*key_p <= p->key) { 
    if (avl_destr_find_at_least(p->left, key_p))
      return 1;
    else
      return avl_destr_find_min(p->right, key_p);
  } else
    return avl_destr_find_at_least(p->right, key_p);   
}


/* Find greatest key smaller than the given key. If operation 
   fails, return 0, else store the found key in `*key_p' and return 1. */
int avl_destr_find_at_most(avl_destr_node_t *p, word_t *key_p)
{
  if (p == NULL)
    return 0;
  if (p->right == NULL) {
    if (p->key <= *key_p) {
      *key_p = p->key;
      return 1;
    } 
    return 0;
  } else if (*key_p > p->key) { 
    if (avl_destr_find_at_most(p->right, key_p))
      return 1;
    else
      return avl_destr_find_max(p->left, key_p);
  } else
    return avl_destr_find_at_most(p->left, key_p);   
}

/* Insert the given key in the tree. */
void avl_destr_insert(avl_destr_node_t **p, word_t key) 
{
  avl_destr_node_t *new_item, *new_p, *q;
  int balance, balance_of_son;
  
  if (*p == NULL) {
    new_item = malloc(sizeof(avl_destr_node_t));
    if (new_item == NULL) {
      fprintf(stderr,"avl_destr_insert: malloc failed.\n");
      exit(1);
    }
    new_item->key = key;
    new_item->height = 0;
    new_item->left = NULL;
    new_item->right = NULL;
    *p = new_item;
    return;
  }
  if ((*p)->left == NULL) {
    if ((*p)->key != key) {
      new_item = malloc(sizeof(avl_destr_node_t));
      if (new_item == NULL) {
	fprintf(stderr,"avl_destr_insert: malloc failed.\n");
	exit(1);
      }
      new_item->key = key;
      new_item->height = 0;
      new_item->left = NULL;
      new_item->right = NULL;
      
      new_p = malloc(sizeof(avl_destr_node_t));
      if (new_p == NULL) {
	fprintf(stderr,"avl_destr_insert: malloc failed.\n");
	exit(1);
      }
      new_p->key = (*p)->key;
      new_p->height = 0;
      new_p->left = NULL;
      new_p->right = NULL;
      
      if (key < (*p)->key) {
	(*p)->key = key;
	(*p)->left = new_item;
	(*p)->right = new_p;
      } else {
	(*p)->left =  new_p;
	(*p)->right = new_item;
      }
      (*p)->height = 1;
    }
    return;
  }
  if (key <= (*p)->key) {
    q = (*p)->left;
    avl_destr_insert(&q, key);
  } else {
    q = (*p)->right;
    avl_destr_insert(&q, key);
  }
  (*p)->height = HEIGHT(*p);
  balance = BALANCE(*p);
  if (balance < -1) {
    balance_of_son = BALANCE((*p)->left);
    if (balance_of_son == -1)
      srr(p);
    else if (balance_of_son == 1)
      drr(p);
  } else if (balance > 1) {
    balance_of_son = BALANCE((*p)->right);
    if (balance_of_son == 1)
      srl(p);
    else if (balance_of_son == -1)
      drl(p);
  }
  return;
}

/* Delete the given key in the tree return 1 if succeed, else return 0. */
int avl_destr_delete(avl_destr_node_t **p, word_t key) 
{
  avl_destr_node_t *q;
  
  int balance, balance_of_son, result;
  
  if (*p == NULL)
    return 0;
  if ((*p)->left == NULL) {
    if ((*p)->key == key) {
      q = *p;
      free(q);
      *p = NULL;
      return 1;
    } else
      return 0;
  }
  if (key <= (*p)->key) {
    if (((*p)->left->key == key) && ((*p)->left->left == NULL)) {
      
      q = (*p)->left;
      (*p)->left = NULL;
      free(q);
      
      q = (*p)->right;
      (*p)->key = q->key;
      (*p)->height = q->height;
      (*p)->left = q->left;
      (*p)->right = q->right;
      free(q);
      
      return 1;
    } else {
      q = (*p)->left;
      result = avl_destr_delete(&q, key);
    }
  } else {
    if (((*p)->right->key == key) && ((*p)->right->left == NULL)) {
      
      q = (*p)->right;
      (*p)->right = NULL;
      free(q);
      
      q = (*p)->left;
      (*p)->key = q->key;
      (*p)->height = q->height;
      (*p)->left = q->left;
      (*p)->right = q->right;
      free(q);

      return 1;
    } else {
      q = (*p)->right;
      result = avl_destr_delete(&q, key);
    }
  }

  if (result) {
    (*p)->height = HEIGHT(*p); 
    balance = BALANCE(*p);
    if (balance < -1) {
      balance_of_son = BALANCE((*p)->left);
      if (balance_of_son == 1)
	drr(p);
      else
	srr(p);
    } else if (balance > 1) {
      balance_of_son = BALANCE((*p)->right);
      if (balance_of_son == -1)
	drl(p);
      else
	srl(p);
    }
  }
  return result;
}

/* Save a randomly chosen key from the tree to `*choice'. */ 
void avl_destr_pick_random(avl_destr_node_t *p, int *count, word_t *choice) 
{
  if (p != NULL) {
    avl_destr_pick_random(p->left, count, choice);
    if (p->left == NULL) {
      if (rand() % (*count + 1) == 0)
	*choice = p->key;
      (*count)++;
    } 
    avl_destr_pick_random(p->right, count, choice);
  }
  return; 
}


/* Interfaces for AVL-routines. Uses `insertion_history' as the tree. */

insertion_history_t *insertion_history;

/* Return a positive integer if the history is empty. */ 
int is_empty_history(void)
{
  return (insertion_history == NULL);
}

/* Delete all from the history. */
void remove_history(void)
{
  insertion_history = NULL;
  return;
}

/* Insert an item to the history. */
void prepend_history(word_t key)
{
  avl_destr_insert(&insertion_history, key);
  return;
}

/* Delete an item from the history. */
int withdraw_history(word_t key)
{
  return avl_destr_delete(&insertion_history, key);
}

/* Return a positive integer if the given key is in the history, 
   else return 0. */
int is_in_history(word_t key)
{
  return avl_destr_find(insertion_history, key);
}

int minimum_in_history(void)
{
  word_t key = 0;
  int res;

  res = avl_destr_find_min(insertion_history, &key);
  if (res == 0)
    return -1;
  else 
    return key;
}

int maximum_in_history(void)
{
  word_t key = 0;
  int res;

  res = avl_destr_find_max(insertion_history, &key);
  if (res == 0)
    return -1;
  else 
    return key;
}

/* Pick a randomly chosen key from the history. */
word_t pick_history(void)
{
  int count, *count_p;
  word_t choice, *choice_p;

  count = 0;
  count_p = &count;
  choice_p = &choice;
  if (insertion_history != NULL) {
    avl_destr_pick_random(insertion_history, count_p, choice_p);
    return choice;
  }
  return -1;
}
