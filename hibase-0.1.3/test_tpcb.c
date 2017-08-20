/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Kengatharan Sivalingam <siva@iki.fi>
 */

/* TPC-B test for Shades at cell-level.
 */

#include "includes.h"
#include "trie.h"
#include "root.h"
#include "test_aux.h"
#include <time.h>


static char *rev_id = "$Id: test_tpcb.c,v 1.32 1998/02/21 13:32:35 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

#define BRANCHES_PER_TPCB       1
#define TELLERS_PER_TPCB        10
#define ACCOUNTS_PER_TPCB       100000

/* Record sizes given in `word_t'. */
#define B_RECORD_SIZE	        2
#define T_RECORD_SIZE	        3
#define A_RECORD_SIZE	        3
#define H_RECORD_SIZE	        5

/* Column indices. */
#define B_RECORD_BID_IDX        1
#define B_RECORD_BAL_IDX        2

#define T_RECORD_TID_IDX        1
#define T_RECORD_BID_IDX        2
#define T_RECORD_BAL_IDX        3

#define A_RECORD_AID_IDX        1
#define A_RECORD_BID_IDX        2
#define A_RECORD_BAL_IDX        3

#define H_RECORD_BID_IDX        1
#define H_RECORD_TID_IDX        2
#define H_RECORD_AID_IDX        3
#define H_RECORD_DELTA_IDX      4
#define H_RECORD_TIME_IDX       5

/* Table names. */
#define TABLE_NAME_BRANCH       "test_branch"
#define TABLE_NAME_TELLER       "test_teller"
#define TABLE_NAME_ACCOUNT      "test_account"
#define TABLE_NAME_HISTORY      "test_history"

#define RANDOM_MAX	        0x80000000U
#define RANDOM_MASK	        0x7fffffffU

int tps = 1;
int duration_in_seconds = 0;
int show_progress = 0;
int scatter_key = 0;

int *latency_table = NULL;
double latency_time_interval = 0;

static void tpcb_create(void);
static void tpcb_run(void);
static void tpcb_do_one_transaction(word_t, word_t, word_t, word_t);
static void insert_records(void);
static void print_table(char *);
static word_t random_number(word_t);


int main(int argc, char *argv[])
{
  int i;
  
  if (argc < 3) {
    fprintf(stderr,
	    "Usage: %s tpcb-size seconds [--show-progress] [--scatter-key] [params]\n",
	    argv[0]);
    exit(1);
  }
  
  tps = atoi(argv[1]);
  duration_in_seconds = atoi(argv[2]);

  argv[1] = NULL;
  argv[2] = NULL;
  
  /* Read other arguments given exclusively for `test_tpcb'. */
  for (i = 3; i < argc; i++) {
    if (argv[i] == NULL)
      continue;
    if (strcmp("--show-progress", argv[i]) == 0) {
      show_progress = 1;
      argv[i] = NULL;
    } else if (strcmp("--scatter-key", argv[i]) == 0) {
      scatter_key = 1;
      argv[i] = NULL;
    }
  }

  init_shades(argc, argv);
  srandom(give_time());

  for (i = 3; i < argc; i++)
    if (argv[i] != NULL 
	&& strcmp(argv[i], "-v")
	&& strcmp(argv[i], "--verbose")) {
      fprintf(stderr,
	      "Usage: %s tpcb-size seconds [--show-progress] [--scatter-key] [params]\n",
	      argv[0]);
      exit(1);
    }

  tpcb_create();
  if (duration_in_seconds > 0) {
    tpcb_run();
    flush_batch();
  }
#if 0
  print_table(TABLE_NAME_TELLER);
#endif
  return 0;
}


static void tpcb_create()
{
  if (be_verbose)
    fprintf(stdout, "Creating database \"%s\"...\n", disk_filename);
  create_db();
  insert_records();
  flush_batch();
}

#define TRANSACTION_SET_SIZE    2048

static word_t balance;

/* Test driver for the test. */
static void tpcb_run()
{
  word_t bid, tid, aid, delta;
  word_t trans_set[TRANSACTION_SET_SIZE][4];
  double start_time, end_time, trans_set_runtime, total_runtime = 0;
  long total_latencies = 0, cumulative_latencies = 0;
  long number_of_transactions = 0;
  register int i;

  /* Initialize flush-batch latency table and time interval. */
  if (number_of_latency_measurement_intervals > 0) {
    latency_table = calloc(number_of_latency_measurement_intervals + 1, 
			   sizeof(int));
    if (latency_table == NULL) {
      fprintf(stderr, "tpcb_run: calloc failed.\n");
      exit(1);
    }
    latency_time_interval = (double) max_latency_measurement / 
      number_of_latency_measurement_intervals;
  }

  if (be_verbose)
    fprintf(stdout, "\nRunning TPC-B for %d seconds...\n", 
	    duration_in_seconds);

  while(1) {
    for (i = 0; i < TRANSACTION_SET_SIZE; i++) {
      tid = random_number(tps * TELLERS_PER_TPCB);
      bid = tid / TELLERS_PER_TPCB;
      
      if (tps <= 1 || random_number(100) < 85) {
	aid = random_number(ACCOUNTS_PER_TPCB / BRANCHES_PER_TPCB) +
	  bid * ACCOUNTS_PER_TPCB / BRANCHES_PER_TPCB;
      }
      else {
	do 
	  aid = random_number(tps * ACCOUNTS_PER_TPCB / BRANCHES_PER_TPCB);
	while (aid * BRANCHES_PER_TPCB / ACCOUNTS_PER_TPCB == bid);
      }

      delta = random_number(1999999) - 999999;

      trans_set[i][0] = bid;
      trans_set[i][1] = tid;
      trans_set[i][2] = aid;
      trans_set[i][3] = delta;
    }

    start_time = give_time();
    for (i = 0; i < TRANSACTION_SET_SIZE; i++) 
      tpcb_do_one_transaction(trans_set[i][0], trans_set[i][1], 
			      trans_set[i][2], trans_set[i][3]);
    end_time = give_time();

    trans_set_runtime = end_time - start_time;
    if (total_runtime + trans_set_runtime > duration_in_seconds)
      break;

    total_runtime += trans_set_runtime;
    number_of_transactions += TRANSACTION_SET_SIZE;

    if (be_verbose || show_progress)
      fprintf(stderr, 
	      " %8ld                                                 %c",
	      number_of_transactions, 13);
  }

  /* Display flush-batch latency statistics. */
  if (number_of_latency_measurement_intervals > 0) {
    /* Count total number of latencies. */
    for (i = 0; i <= number_of_latency_measurement_intervals; i++)
      total_latencies += latency_table[i];

    for (i = 0; i <= number_of_latency_measurement_intervals; i++) {
      if (i < number_of_latency_measurement_intervals)
	fprintf(stdout, "%1.2f - %1.2f sec: ", 
		(double) (i) * latency_time_interval, 
		(double) (i + 1) * latency_time_interval);
      else 
	fprintf(stdout, "     > %1.2f sec: ", 
		(double) (i) * latency_time_interval);
      
      cumulative_latencies += latency_table[i];

      fprintf(stdout, 
	      "    Latencies: %6d [%5.1f%%]    Cumulative: %6ld [%5.1f%%]\n", 
	      latency_table[i],
	      100.0 * (double) latency_table[i] / total_latencies,
	      cumulative_latencies, 
	      100.0 * (double) cumulative_latencies / total_latencies);

      /* Stop displaying latency information after reaching the
	 maximum number of latencies. */
      if (cumulative_latencies == total_latencies)
	break;
    }

    fprintf(stdout, "\n");
  }

  fprintf(stdout, "@@@ Total transactions: %ld & TPS: %.2f @@@\n", 
	  number_of_transactions,
	  (float) number_of_transactions / total_runtime);
}

static ptr_t update_branch(ptr_t old_data, ptr_t delta_p)
{
  int i;
  ptr_t new_data = raw_allocate(1 + B_RECORD_SIZE);

  for (i = 0; i <= B_RECORD_SIZE; i++)
    new_data[i] = old_data[i];
  new_data[B_RECORD_BAL_IDX] += *delta_p; 
  return new_data;
}

static ptr_t update_teller(ptr_t old_data, ptr_t delta_p)
{
  int i;
  ptr_t new_data = raw_allocate(1 + T_RECORD_SIZE);

  for (i = 0; i <= T_RECORD_SIZE; i++)
    new_data[i] = old_data[i];
  new_data[T_RECORD_BAL_IDX] += *delta_p; 
  return new_data;
}

static ptr_t update_account(ptr_t old_data, ptr_t delta_p)
{
  int i;
  ptr_t new_data = raw_allocate(1 + A_RECORD_SIZE);

  for (i = 0; i <= A_RECORD_SIZE; i++)
    new_data[i] = old_data[i];
  new_data[A_RECORD_BAL_IDX] += *delta_p;
  /* Assign the new value of the field `Abalance' to `balance'. */
  balance = new_data[A_RECORD_BAL_IDX];
  return new_data;
}

static void tpcb_do_one_transaction(word_t bid, word_t tid, word_t aid, 
				    word_t delta)
{
  int latency_table_index;
  ptr_t data, new_data, h_record, h_list_node;
  static int current_history_length = 0;
  double flush_batch_time_current = 0;
  static double flush_batch_time_prev = 0;

  while (!can_allocate(3 * TRIE_MAX_ALLOCATION
		       + B_RECORD_SIZE + 1
		       + T_RECORD_SIZE + 1
		       + A_RECORD_SIZE + 1
		       + 2 * 3 + H_RECORD_SIZE + 1)) {
    flush_batch();

    /* Compute flush-batch latency. */
    if (number_of_latency_measurement_intervals > 0) {
      flush_batch_time_current = give_time();

      if (flush_batch_time_prev) {
	latency_table_index = 
	  (flush_batch_time_current - flush_batch_time_prev) / 
	  latency_time_interval;
	if (latency_table_index > number_of_latency_measurement_intervals)
	  latency_table_index = number_of_latency_measurement_intervals;
	latency_table[latency_table_index]++;
      }

      flush_batch_time_prev = flush_batch_time_current;
    }
  }

  SET_ROOT_PTR(test_branch,
	       trie_insert(GET_ROOT_PTR(test_branch), 
			   scatter_key ? scatter(bid) : bid, 
			   update_branch,
			   &delta));

  SET_ROOT_PTR(test_teller,
	       trie_insert(GET_ROOT_PTR(test_teller),
			   scatter_key ? scatter(tid) : tid, 
			   update_teller, 
			   &delta));

  SET_ROOT_PTR(test_account,
	       trie_insert(GET_ROOT_PTR(test_account), 
			   scatter_key ? scatter(aid) : aid, 
			   update_account,
			   &delta));

  /* Insert record into `test_history'. */
  if (test_tpcb_history_length != 0) {
    h_record = allocate(1 + H_RECORD_SIZE, CELL_word_vector);
    assert(h_record != NULL_PTR);
    h_record[0] |= H_RECORD_SIZE;
    h_record[H_RECORD_BID_IDX] = bid;
    h_record[H_RECORD_TID_IDX] = tid;
    h_record[H_RECORD_AID_IDX] = aid;
    h_record[H_RECORD_DELTA_IDX] = delta;
    h_record[H_RECORD_TIME_IDX] = give_time();
    
    h_list_node = allocate(3, CELL_list);
    assert(h_list_node != NULL_PTR);
    h_list_node[1] = PTR_TO_WORD(h_record);
    h_list_node[2] = NULL_WORD;

    if (current_history_length != 0) {   
      /* History list exists. */
      if (current_history_length != test_tpcb_history_length) {
	/* Get the last node of the existing list. */
	data = GET_ROOT_PTR(test_history);
	assert(data == NULL_PTR && 
	       cell_get_number_of_words(data) == 3);
	
	new_data = allocate(3, CELL_list);
	assert(new_data != NULL_PTR);
	new_data[1] = data[1];
	new_data[2] = PTR_TO_WORD(h_list_node);
	SET_ROOT_PTR(test_history, new_data);
      }
      else 
	/* Maximum number of records reached, reset the list. */
	current_history_length = 0;
    }
    
    SET_ROOT_PTR(test_history, h_list_node);
    current_history_length++;
  }
}

/* Insert records for tables `test_branch', `test_teller' and 
   `test_account'. */
static void insert_records()
{
  ptr_t b_record, t_record, a_record;
  word_t skey, key;

  if (be_verbose)
    fprintf(stdout, "Creating table \"%s\"...\n", TABLE_NAME_BRANCH);
  for (key = 0; key < BRANCHES_PER_TPCB * tps; key++) {
    while (!can_allocate(TRIE_MAX_ALLOCATION + B_RECORD_SIZE + 1))
      flush_batch();
    
    if (scatter_key)
      skey = scatter(key);
    else 
      skey = key;

    b_record = allocate(1 + B_RECORD_SIZE, CELL_word_vector);
    b_record[0] |= B_RECORD_SIZE;
    b_record[B_RECORD_BID_IDX] = skey;
    b_record[B_RECORD_BAL_IDX] = 0;
    
    SET_ROOT_PTR(test_branch,
		 trie_insert(GET_ROOT_PTR(test_branch), skey,
			     NULL, b_record));
  }

  if (be_verbose)
    fprintf(stdout, "Creating table \"%s\"...\n", TABLE_NAME_TELLER);
  for (key = 0; key < TELLERS_PER_TPCB * tps; key++) {
    while (!can_allocate(TRIE_MAX_ALLOCATION + T_RECORD_SIZE + 1))
      flush_batch();
    
    if (scatter_key)
      skey = scatter(key);
    else 
      skey = key;

    t_record = allocate(1 + T_RECORD_SIZE, CELL_word_vector);
    t_record[0] |= T_RECORD_SIZE;
    t_record[T_RECORD_TID_IDX] = skey;
    t_record[T_RECORD_BID_IDX] = 0;
    t_record[T_RECORD_BAL_IDX] = 0;
    
    SET_ROOT_PTR(test_teller,
		 trie_insert(GET_ROOT_PTR(test_teller), skey,
			     NULL, t_record));
  }

  if (be_verbose)
    fprintf(stdout, "Creating table \"%s\"...\n", TABLE_NAME_ACCOUNT);
  for (key = 0; key < ACCOUNTS_PER_TPCB * tps; key++) {
    while (!can_allocate(TRIE_MAX_ALLOCATION + A_RECORD_SIZE + 1))
      flush_batch();
#if 0
    if (key % 10000 == 0)
      fprintf(stdout, "Inserting key %ld...\n", key);
#endif    
    if (scatter_key)
      skey = scatter(key);
    else 
      skey = key;

    a_record = allocate(1 + A_RECORD_SIZE, CELL_word_vector);
    a_record[0] |= A_RECORD_SIZE;
    a_record[A_RECORD_AID_IDX] = skey;
    a_record[A_RECORD_BID_IDX] = 0;
    a_record[A_RECORD_BAL_IDX] = 0;
    
    assert(trie_find(GET_ROOT_PTR(test_account), skey) == NULL_PTR);

    SET_ROOT_PTR(test_account,
		 trie_insert(GET_ROOT_PTR(test_account), skey,
			     NULL, a_record));
  }
}

/* Print table `table_name'. */
static void print_table(char *table_name)
{
  ptr_t root_p, data;
  word_t key;
  int i, record_size, number_of_records = 0;
  int history_table = 0;

  if (strcmp(table_name, TABLE_NAME_BRANCH) == 0) {
    fprintf(stdout,"\n*** Table: %s ***\n\n", TABLE_NAME_BRANCH);
    fprintf(stdout,"%s\t%s\n", "Bid", "Balance");
    
    root_p = GET_ROOT_PTR(test_branch);
    number_of_records = BRANCHES_PER_TPCB * tps;
    record_size = B_RECORD_SIZE;
  }
  else if (strcmp(table_name, TABLE_NAME_TELLER) == 0) {
    fprintf(stdout,"\n*** Table: %s ***\n\n", TABLE_NAME_TELLER);
    fprintf(stdout,"%s\t%s\t%s\n", "Tid", "Bid", "Balance");

    root_p = GET_ROOT_PTR(test_teller);
    number_of_records = TELLERS_PER_TPCB * tps;
    record_size = T_RECORD_SIZE;
  }
  else if (strcmp(table_name, TABLE_NAME_ACCOUNT) == 0) {
    fprintf(stdout,"\n*** Table: %s ***\n\n", TABLE_NAME_ACCOUNT);
    fprintf(stdout,"%s\t%s\t%s\n", "Aid", "Bid", "Balance");

    root_p = GET_ROOT_PTR(test_account);
    number_of_records = ACCOUNTS_PER_TPCB * tps;
    record_size = A_RECORD_SIZE;
  }
  else if (strcmp(table_name, TABLE_NAME_HISTORY) == 0 &&
	   test_tpcb_history_length) {
    fprintf(stdout,"\n*** Table: %s ***\n\n", TABLE_NAME_HISTORY);
    fprintf(stdout,"%s\t%s\t%s\t%s\t%s\n", "Bid", "Tid", "Aid", "delta",
	   "time");

    root_p = GET_ROOT_PTR(test_history);
    history_table = 1;
    record_size = H_RECORD_SIZE;
  }
  else 
    return;
  if (history_table) {
    /* This prints only the last record of the history table. */
    root_p = WORD_TO_PTR(root_p[1]);
    for (i = 1; i <= record_size; i++)
      fprintf(stdout, "%ld\t", root_p[i]);
    fprintf(stdout, "\n");
  }
  else {
    for (key = 0; key < number_of_records; key++) {
      data = trie_find(root_p, scatter_key ? scatter(key) : key);
      assert(data != NULL_PTR);
      for (i = 1; i <= record_size; i++)
	fprintf(stdout, "%ld\t", data[i]);
      fprintf(stdout, "\n");
    }
  }
}

static word_t random_number(word_t max)
{
  word_t tmp;

  assert(max < RANDOM_MAX);
  
  do {
    tmp = ((word_t) random() & RANDOM_MASK);
  } while (tmp >= (RANDOM_MAX / max) * max);

  return tmp % max;
}

