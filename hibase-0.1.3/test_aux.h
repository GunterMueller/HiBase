/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Sirkku Paajanen <sirkku@iki.fi>
 */

/* Auxiliary routines for test and benchmark programs.
 */

#include "includes.h"


/* Returns wall clock time in seconds since the beginning of the
   program.  Typical use involves two calls; one before and one after
   the benchmark, and the consumed time is the difference of these two
   times. */
double give_time(void);


typedef struct {
  /* Value range is all integers numbers from 1 to N, bounds included. */
  int N;
  /* Skewedness of the distribution.  p == 0 gives a uniform
     distribution. */
  double p;
  /* Values computed once for all random numbers for the given `N' and
     `p'. */
  double c1, c2;
} zipf_t;

zipf_t *zipf_init(int N, double p);
/* Given the struct the `zipf_init' returns, `zipf' produces
   Zipfian-distributed pseudo-random numbers in the range from 1 to N,
   bounds included. */
int zipf(zipf_t *);


/* Key scattering functions.  These should be elaborated to cover a
   wider variety and with different locality characteristics. */
word_t scatter(word_t);


/* AVL-routines. */

/* Define an AVL-tree node. */
typedef struct avl_destr_node_t {
  word_t key;
  int height;
  struct avl_destr_node_t *left, *right;
} avl_destr_node_t;

/* Find the given key. Return 1 if succeed, else return 0. */
int avl_destr_find(avl_destr_node_t *p, word_t key);

/* Find the smallest key in the tree. Return 0 if failed. */
int avl_destr_find_min(avl_destr_node_t *p, word_t *key_p);

/* Find smallest key greater than the given key. If operation 
   fails, return 0, else store the found key in `*key_p' and return 1. */
int avl_destr_find_at_least(avl_destr_node_t *p, word_t *key_p);

/* Find greatest key less than the given key. If operation 
   fails, return 0, else store the found key in `*key_p' and return 1. */
int avl_destr_find_at_most(avl_destr_node_t *p, word_t *key_p);

/* Insert the given key in the tree. */
void avl_destr_insert(avl_destr_node_t **p, word_t key);

/* Delete the given key in the tree return 1 if succeed, else return 0. */
int avl_destr_delete(avl_destr_node_t **p, word_t key);

/* Store a randomly chosen key from the tree to `*choice'. */ 
void avl_destr_pick_random(avl_destr_node_t *p, int *count, word_t *choice);   


/* History routines, for regression testing with other persistent
   indexes. */

typedef avl_destr_node_t insertion_history_t;

/* A pointer to an AVL-tree. */
extern insertion_history_t *insertion_history;

/* Interfaces for AVL-routines. Uses `insertion_history' as the tree. */

/* Return a positive integer if the history is empty. */ 
int is_empty_history(void);

/* Delete all from the history. */
void remove_history(void);

/* Return a positive integer if the given key is in the history, 
   else return 0. */
int is_in_history(word_t key);

int minimum_in_history(void);

/* Insert the given key to the history. */
void prepend_history(word_t key);

/* Delete the given key from the history. */
int withdraw_history(word_t key);

/* Pick a randomly chosen key from the history. */
word_t pick_history(void);
