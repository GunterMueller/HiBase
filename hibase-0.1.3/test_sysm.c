/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Author: Kengatharan Sivalingam <siva@iki.fi>
 */

/* A credit card benchmark originally designed to test the performance 
   of System M. 

   Reference:
   ==========
   Salem, K., Garcia-Molina, H. System M: A Transaction Processing
   Testbed for Memory Resident Data, IEEE Transactions on Knowledge
   and Data Engineering, vol. 2, no. 1, March 1990.

 */

#include "includes.h"
#include "triev2.h"
#include "root.h"
#include "test_aux.h"
#include "shtring.h"
#include <time.h>

#define TABLE_NAME_ACC                 "test_acc_info"
#define TABLE_NAME_CUST                "test_cust_info"
#define TABLE_NAME_HOT                 "test_hot_info"
#define TABLE_NAME_STORE               "test_store_info"

#define NUM_RECORDS_TABLE_ACC          40000
#define NUM_RECORDS_TABLE_HOT          100   
#define NUM_RECORDS_TABLE_STORE        5000

#define REC_SIZE_ACC                   9        /* 1 + 2 + 4 * 1 + 2 */
#define REC_SIZE_CUST                  14       /* 4 * 1 + 1 + 3 + 3 + 3 */
#define REC_SIZE_HOT                   5        /* 1 + 1 + 2 + 1 */
#define REC_SIZE_STORE                 5        /* 1 + 1 + 3 * 1 */

/* We use cell-type `CELL_tuple' for records which have at lease one
   variable string field. The field order of these records differs
   from the original implementation in a way that variable string
   fields always precede fixed fields. */
#define ACC_REC_ACC_IDX                1
#define ACC_REC_EXP_IDX                2
#define ACC_REC_LIM_IDX                4
#define ACC_REC_UCR_IDX                5
#define ACC_REC_ADV_IDX                6
#define ACC_REC_MIN_IDX                7
#define ACC_REC_PAY_IDX                8

#define CUST_REC_NAME_IDX              1
#define CUST_REC_ADD_IDX               2
#define CUST_REC_ZIP_IDX               3
#define CUST_REC_ACC_IDX               4
#define CUST_REC_HPH_IDX               5
#define CUST_REC_BPH_IDX               8
#define CUST_REC_SOC_IDX               11

#define HOT_REC_NAME_IDX               1
#define HOT_REC_ACC_IDX                2
#define HOT_REC_DATE_IDX               3
#define HOT_REC_ATT_IDX                5

#define STORE_REC_NAME_IDX             1
#define STORE_REC_NUM_IDX              2
#define STORE_REC_CCCK_IDX             3
#define STORE_REC_CLCK_IDX             4
#define STORE_REC_DEB_IDX              5

#define VAR_STR_MAX_LEN                49
#define FIX_STR_MAX_LEN                9
#define MAX_STR_LEN                    255

#define RANDOM_MAX	               0x80000000U
#define RANDOM_MASK	               0x7fffffffU

#define TRANSACTION_SET_SIZE           4096

typedef enum {
  OK, NOT_FOUND_ACC, NOT_FOUND_CUST, HOT_CARD, LIMIT_EXCEEDED, 
  ALREADY_REPORTED, NOT_REPORTED
} trans_return_t;

typedef enum {
  TRANS_BAL, TRANS_CCCK, TRANS_CLCK, TRANS_DEBIT, 
  TRANS_PAY, TRANS_LOST, TRANS_FOUND, TRANS_CHCUST
} trans_t;

typedef struct {
  ptr_t cust_name;
  char exp_date[FIX_STR_MAX_LEN + 1];
  float crd_lim;
  float used_crd;
  float cash_adv;
  float min_due;
} acc_info_t;

typedef struct {
  char name[MAX_STR_LEN + 1];
  char address[MAX_STR_LEN + 1];
  char zip[MAX_STR_LEN + 1];
  char home_ph[FIX_STR_MAX_LEN + 1];
  char business_ph[FIX_STR_MAX_LEN + 1];
} cust_info_t;

static void sysm_db_create(void);
static void sysm_run(void);
static void sysm_do_one_transaction(trans_t);
static inline void compute_flush_batch_latency(double);
static trans_return_t trans_ccck(long, long);
static trans_return_t trans_clck(long, long, float);
static trans_return_t trans_debit(long, long, float);
static trans_return_t trans_pay(long, float);
static trans_return_t trans_bal(long, acc_info_t *);
static trans_return_t trans_lost(long, char *);
static trans_return_t trans_found(long);
static trans_return_t trans_chcust(long, cust_info_t);
static void insert_records(void);
static void float_to_word(float, word_t *);
static void word_to_float(word_t, float *);
static void get_var_len_string(char *, int);
static void get_random_string(char *, int);
static void str_to_shtr(char *, ptr_t *);
static word_t random_number(word_t);
static void print_table(char *);

float sysm_size = 1.0;
int show_progress = 0;
int duration_in_seconds = 0;
int *latency_table = NULL;
double latency_time_interval = 0;

/* Random strings used to update variable string fields. */
char *var_strings[] = {
  "aaaaaaaaaa", 
  "bbbbbbbbbb", 
  "ccccccccccddddddddddeeeee", 
  "ffffffffffgggggggggghhhhhhhhhhiii", 
  "jjjjjjjjjjkkkkkkkkkkllllllllllmmmmmmmmmm", 
  "nnnnnnnnnnooooooooooppppppppppqqqqqqqqqqq", 
  "rrrrrrrrrrssssssssssttttttttttuuuuuuuuuuuvvvvvvv", 
  "wwwwwwwwwwxxxxxxxxxxyyyyyyyyyyzzzzzzzzzzaaa", 
  "ccccccccccddddddddddeeeeeeeeeeffffffffffgggggggggghhhhhhhhhhii", 
  "jjjjjjjjjjkkkkkkkkkkllllllllllmmmmmmmmmmnnnnnnnnnnooooooooooppppppppppqqqqqqqqqqrrrrrrrrrrssssssssss"
};


int main(int argc, char *argv[])
{
  int i;
  
  if (argc < 3) {
    fprintf(stderr, "Usage: %s sysm-size seconds [--show-progress] [params]\n", 
	    argv[0]);
    exit(1);
  }
  
  sysm_size = atof(argv[1]);
  duration_in_seconds = atoi(argv[2]);

  argv[1] = NULL;
  argv[2] = NULL;
  
  /* Read other arguments given exclusively for `test_sysm'. */
  for (i = 3; i < argc; i++) {
    if (argv[i] == NULL)
      continue;

    if (strcmp("--show-progress", argv[i]) == 0) {
      show_progress = 1;
      argv[i] = NULL;
      break;
    }
  }

  srandom(give_time());
  init_shades(argc, argv);

  sysm_db_create();
  if (duration_in_seconds > 0) {
    sysm_run();
    flush_batch();
  };

#if 0
  print_table(TABLE_NAME_ACC);
#endif
  return 0;
}


static void sysm_db_create()
{
  if (be_verbose)
    fprintf(stdout, "Creating database \"%s\"...\n", disk_filename);
  create_db();
  insert_records();
  flush_batch();
}

/* Test driver for the test. */
static void sysm_run()
{
  double start_time, end_time, trans_set_runtime, total_runtime = 0;
  word_t trans_set[TRANSACTION_SET_SIZE];
  long number_of_transactions = 0;
  long trans_rnd, total_latencies = 0, cumulative_latencies = 0;
  register int i;

  /* Initialize flush-batch latency table and time interval. */
  if (number_of_latency_measurement_intervals > 0) {
    /* Allocate one extra integer for latencies which are greater 
       than `max_latency_measurement'. */
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
    fprintf(stdout, "\nRunning SysM-benchmark for %d seconds...\n", 
	    duration_in_seconds);
  
  while (1) {
    for (i = 0; i < TRANSACTION_SET_SIZE; i++) {
      trans_rnd = random_number(100);

      if (trans_rnd < 20)
	trans_set[i] = TRANS_CCCK;
      else if (trans_rnd < 40)
	trans_set[i] = TRANS_CLCK;
      else if (trans_rnd < 60)
	trans_set[i] = TRANS_DEBIT;
      else if (trans_rnd < 80)
	trans_set[i] = TRANS_PAY;
      else if (trans_rnd < 97)
	trans_set[i] = TRANS_BAL;
      else if (trans_rnd < 98)
	trans_set[i] = TRANS_LOST;
      else if (trans_rnd < 99)
	trans_set[i] = TRANS_FOUND;
      else
	trans_set[i] = TRANS_CHCUST;
    }

    start_time = give_time();
    for (i = 0; i < TRANSACTION_SET_SIZE; i++) 
      sysm_do_one_transaction(trans_set[i]);
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
    for (i = 0; i < number_of_latency_measurement_intervals + 1; i++)
      total_latencies += latency_table[i];

    for (i = 0; i < number_of_latency_measurement_intervals + 1; i++) {
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

  fprintf(stdout, 
	  "@@@ Total transactions: %ld & TPS: %.2f @@@\n", 
	  number_of_transactions,
	  (float) number_of_transactions / total_runtime);
}

static void sysm_do_one_transaction(trans_t trans_type)
{
  acc_info_t acc_buf;
  cust_info_t cust_buf;
  trans_return_t ret;
  static int var_str_idx = 0;

  switch (trans_type) {
  case TRANS_CCCK:
    ret = trans_ccck(random_number(sysm_size * NUM_RECORDS_TABLE_ACC),
		     random_number(sysm_size * NUM_RECORDS_TABLE_STORE));
    break;
    
  case TRANS_CLCK:
    ret = trans_clck(random_number(sysm_size * NUM_RECORDS_TABLE_ACC),
		     random_number(sysm_size * NUM_RECORDS_TABLE_STORE),
		     500.00);
    break;
    
  case TRANS_DEBIT:
    ret = trans_debit(random_number(sysm_size * NUM_RECORDS_TABLE_ACC),
		      random_number(sysm_size * NUM_RECORDS_TABLE_STORE),
		      500.00);
    break;
    
  case TRANS_PAY:
    ret = trans_pay(random_number(sysm_size * NUM_RECORDS_TABLE_ACC),
		    500.00);
    break;
    
  case TRANS_BAL:
    ret = trans_bal(random_number(sysm_size * NUM_RECORDS_TABLE_ACC),
		    &acc_buf);
    break;

  case TRANS_LOST:
    ret = trans_lost(random_number(sysm_size * NUM_RECORDS_TABLE_ACC),
		     var_strings[var_str_idx++]);
    if (var_str_idx >= 10)
      var_str_idx = 0;
    break;

  case TRANS_FOUND:
    ret = trans_found(random_number(sysm_size * NUM_RECORDS_TABLE_ACC));
    break;

  case TRANS_CHCUST:
    strcpy(cust_buf.name, var_strings[var_str_idx++]);
    if (var_str_idx >= 0)
      var_str_idx = 0;
    strcpy(cust_buf.address, var_strings[var_str_idx++]);
    if (var_str_idx >= 0)
      var_str_idx = 0;
    strcpy(cust_buf.zip, var_strings[var_str_idx++]);
    if (var_str_idx >= 0)
      var_str_idx = 0;

    /* Note that the following strings are not initialized at all in the 
       orignal implementation. But we do it here, because `chcust' requires
       complete customer information. */
    get_random_string(cust_buf.home_ph, 9);
    get_random_string(cust_buf.business_ph, 9);
    
    ret = trans_chcust(random_number(sysm_size * NUM_RECORDS_TABLE_ACC),
		       cust_buf);
    break;
    
  default:
    break;
  }
}

static inline void compute_flush_batch_latency(double time_current)
{
  static double flush_batch_time_prev = 0;
  int latency_table_index;

  if (flush_batch_time_prev) {
    latency_table_index = 
      (time_current - flush_batch_time_prev) / latency_time_interval;
    if (latency_table_index > number_of_latency_measurement_intervals)
      latency_table_index = number_of_latency_measurement_intervals;
    latency_table[latency_table_index]++;
  }
  
  flush_batch_time_prev = time_current;
}

static word_t increment_store_field(word_t old_data, word_t field_idx, 
				    void *dummy)
{
  int i;
  ptr_t new_data = allocate(1 + REC_SIZE_STORE, CELL_tuple);

  for (i = 0; i <= REC_SIZE_STORE; i++)
    new_data[i] = WORD_TO_PTR(old_data)[i];
  new_data[field_idx]++;

  return PTR_TO_WORD(new_data);
}

/* Credit card authorization -- checks whether the card has been reported 
   previously as lost or stolen */ 
trans_return_t trans_ccck(long acc_num, long store_num)
{
  word_t field_idx;
  ptr_t data_p;

  while (!can_allocate(TRIEV2_MAX_ALLOCATION + REC_SIZE_STORE + 1)) {
    flush_batch();
    if (number_of_latency_measurement_intervals > 0) 
      compute_flush_batch_latency(give_time());
  }

  /* Incremet field `CCCK' by 1 in store record. */
  field_idx = STORE_REC_CCCK_IDX;
  SET_ROOT_PTR(test_store_info,
	       triev2_insert(GET_ROOT_PTR(test_store_info), 
			     store_num, 
			     32,
			     increment_store_field,
			     NULL,
			     field_idx));

  /* Try reading the account number. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_acc_info), acc_num, 32));
  if (data_p == NULL_PTR)
    return NOT_FOUND_ACC;

  /* Try reading the acc in `table_hot_info'. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_hot_info), acc_num, 32));
  if (data_p != NULL_PTR)
    return HOT_CARD;
  
  return OK;
}

/* Check credit limit. */
trans_return_t trans_clck(long acc_num, long store_num, float amount)
{
  word_t field_idx;
  ptr_t data_p;
  float crlm, crus;

  while (!can_allocate(TRIEV2_MAX_ALLOCATION + REC_SIZE_STORE + 1)) {
    flush_batch();
    if (number_of_latency_measurement_intervals > 0) 
      compute_flush_batch_latency(give_time());
  }

  /* Incremet field `CLCK' by 1 in store record. */
  field_idx = STORE_REC_CLCK_IDX;
  SET_ROOT_PTR(test_store_info,
	       triev2_replace(GET_ROOT_PTR(test_store_info), 
			      store_num, 
			      32,
			      increment_store_field,
			      NULL,
			      field_idx));

  /* Try reading the account number. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_acc_info), acc_num, 32));
  if (data_p != NULL_PTR)
    return NOT_FOUND_ACC;

  /* Read credit limit and used credit values, and compute 
     unused credit amount. */
  word_to_float(data_p[ACC_REC_LIM_IDX], &crlm); 
  word_to_float(data_p[ACC_REC_UCR_IDX], &crus);
  if (amount > crlm - crus)
    return LIMIT_EXCEEDED;

  return OK;
}

static word_t update_acc_used_crd(word_t old_data, word_t new_amount, 
				  void *dummy)
{
  int i;
  ptr_t new_data = allocate(1 + REC_SIZE_ACC, CELL_word_vector);

  for (i = 0; i <= REC_SIZE_ACC; i++)
    new_data[i] = WORD_TO_PTR(old_data)[i];
  new_data[ACC_REC_UCR_IDX] = new_amount;

  return PTR_TO_WORD(new_data);
}

/* Perform the actual purchase of an item. */
trans_return_t trans_debit(long acc_num, long store_num,
			   float amount)
{
  word_t field_idx, updated_crd;
  ptr_t data_p;
  float crlm, crus;

  while (!can_allocate(TRIEV2_MAX_ALLOCATION + REC_SIZE_STORE + 1)) {
    flush_batch();
    if (number_of_latency_measurement_intervals > 0) 
      compute_flush_batch_latency(give_time());
  }
  
  /* Incremet field `DEBIT' by 1 in store record. */
  field_idx = STORE_REC_DEB_IDX;
  SET_ROOT_PTR(test_store_info,
	       triev2_replace(GET_ROOT_PTR(test_store_info), 
			      store_num, 
			      32,
			      increment_store_field,
			      NULL,
			      field_idx));

  /* Try reading the account number. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_acc_info), acc_num, 32));
  if (data_p != NULL_PTR)
    return NOT_FOUND_ACC;

  /* Read credit limit and used credit values, and compute 
     unused credit amount. */
  word_to_float(data_p[ACC_REC_LIM_IDX], &crlm); 
  word_to_float(data_p[ACC_REC_UCR_IDX], &crus);
  if (amount > crlm - crus)
    return LIMIT_EXCEEDED;

  crus += amount;
  float_to_word(crus, &updated_crd);

  /* Update used credit. */
  SET_ROOT_PTR(test_acc_info,
	       triev2_replace(GET_ROOT_PTR(test_acc_info), 
			      acc_num, 
			      32,
			      update_acc_used_crd,
			      NULL,
			      updated_crd));
  return OK;
}

/* Make a payment. */
trans_return_t trans_pay(long acc_num, float amount)
{
  word_t updated_crd;
  ptr_t data_p;
  float crus;

  while (!can_allocate(TRIEV2_MAX_ALLOCATION + REC_SIZE_ACC + 1)) {
    flush_batch();
    if (number_of_latency_measurement_intervals > 0) 
      compute_flush_batch_latency(give_time());
  }

  /* Try reading the account number. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_acc_info), acc_num, 32));
  if (data_p != NULL_PTR)
    return NOT_FOUND_ACC;

  /* Credit into account. */
  word_to_float(data_p[ACC_REC_UCR_IDX], &crus);
  crus -= amount; 
  float_to_word(crus, &updated_crd);

  /* Update used credit. */
  SET_ROOT_PTR(test_acc_info,
	       triev2_insert(GET_ROOT_PTR(test_acc_info), 
			     acc_num, 
			     32,
			     update_acc_used_crd,
			     NULL,
			     updated_crd));
  return OK;
}

/* Report a lost or stolen card. */
trans_return_t trans_bal(long acc_num, acc_info_t *acc_info)
{
  ptr_t data_p;

  /* Try reading the account number. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_acc_info), acc_num, 32));
  if (data_p != NULL_PTR)
    return NOT_FOUND_ACC;
  
  memcpy((void *) acc_info->exp_date, (void *) &data_p[ACC_REC_EXP_IDX], 8);
  word_to_float(data_p[ACC_REC_LIM_IDX], &acc_info->crd_lim);
  word_to_float(data_p[ACC_REC_UCR_IDX], &acc_info->used_crd);
  word_to_float(data_p[ACC_REC_ADV_IDX], &acc_info->cash_adv);
  word_to_float(data_p[ACC_REC_MIN_IDX], &acc_info->min_due);
  
  /* Try reading the corresponding cutomer info. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_cust_info), acc_num, 32));
  if (data_p != NULL_PTR)
    return NOT_FOUND_ACC;
  
  /* Return pointer to the name string. */
  acc_info->cust_name = WORD_TO_PTR(data_p[CUST_REC_NAME_IDX]);

  return OK;
}

/* Report a lost or stolen card. */
trans_return_t trans_lost(long acc_num, char *cust_name)
{
  ptr_t data_p, record, shtr_p;

  /* Try reading the account number. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_acc_info), acc_num, 32));
  if (data_p != NULL_PTR)
    return NOT_FOUND_ACC;

  /* Try reading the acc in `table_hot_info'. If found, the
     card is already reported lost. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_hot_info), acc_num, 32));
  if (data_p == NULL_PTR)
    return ALREADY_REPORTED;

  /* Create new hot card record. */
  while (!can_allocate(TRIEV2_MAX_ALLOCATION + REC_SIZE_HOT + 1 
		       + shtring_create_max_allocation(strlen(cust_name)))) {
    flush_batch();
    if (number_of_latency_measurement_intervals > 0) 
      compute_flush_batch_latency(give_time());
  }

  record = allocate(1 + REC_SIZE_HOT, CELL_tuple);
  record[0] |= 1 | ((REC_SIZE_HOT - 1) << 12);
  str_to_shtr(cust_name, &shtr_p);
  record[HOT_REC_NAME_IDX] = PTR_TO_WORD(shtr_p);
  record[HOT_REC_ACC_IDX] = acc_num;
  record[HOT_REC_ATT_IDX] = 0;

  /* Insert the new record. */
  SET_ROOT_PTR(test_hot_info,
	       triev2_insert(GET_ROOT_PTR(test_hot_info), 
			     acc_num, 
			     32, 
			     NULL,
			     NULL, 
			     PTR_TO_WORD(record)));
  return OK;
}

/* Report a lost or stolen card found. */
trans_return_t trans_found(long acc_num)
{
  ptr_t data_p;

  /* Try reading the account number. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_acc_info), acc_num, 32));
  if (data_p != NULL_PTR)
    return NOT_FOUND_ACC;

  /* Try reading the acc in `table_hot_info'. If found, remove it
     from the table. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_hot_info), acc_num, 32));
  if (data_p != NULL_PTR)
    return NOT_REPORTED;

  while (!can_allocate(TRIEV2_MAX_ALLOCATION)) {
    flush_batch();
    if (number_of_latency_measurement_intervals > 0) 
      compute_flush_batch_latency(give_time());
  }

  SET_ROOT_PTR(test_hot_info, 
	       triev2_delete(GET_ROOT_PTR(test_hot_info),   
			     acc_num, 
			     32,
			     NULL, 
			     NULL,
			     NULL_WORD));
  return OK;
}

/* Change customer information. */
trans_return_t trans_chcust(long acc_num, cust_info_t cust_info)
{
  ptr_t data_p, record, shtr_p;

  /* Try reading the customer record. */
  data_p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(test_cust_info), acc_num, 32));
  if (data_p != NULL_PTR)
    return NOT_FOUND_CUST;

  while (!can_allocate(TRIEV2_MAX_ALLOCATION + REC_SIZE_CUST + 1 
	       + shtring_create_max_allocation(strlen(cust_info.name))
	       + shtring_create_max_allocation(strlen(cust_info.address))
	       + shtring_create_max_allocation(strlen(cust_info.address)))) {
    flush_batch();
    if (number_of_latency_measurement_intervals > 0) 
      compute_flush_batch_latency(give_time());
  }
  
  /* Update customer info. */
  record = allocate(1 + REC_SIZE_CUST, CELL_tuple);
  
  record[0] |= 3 | ((REC_SIZE_CUST - 3) << 12);
  str_to_shtr(cust_info.name, &shtr_p);
  record[CUST_REC_NAME_IDX] = PTR_TO_WORD(shtr_p);
  str_to_shtr(cust_info.address, &shtr_p);
  record[CUST_REC_ADD_IDX] = PTR_TO_WORD(shtr_p);
  str_to_shtr(cust_info.zip, &shtr_p);
  record[CUST_REC_ZIP_IDX] = PTR_TO_WORD(shtr_p);
  record[CUST_REC_ACC_IDX] = acc_num;
  memcpy((void *) &record[CUST_REC_HPH_IDX], (void *) cust_info.home_ph, 
	 strlen(cust_info.home_ph) + 1);
  memcpy((void *) &record[CUST_REC_BPH_IDX], (void *) cust_info.business_ph, 
	 strlen(cust_info.business_ph) + 1);
  SET_ROOT_PTR(test_cust_info,
	       triev2_insert(GET_ROOT_PTR(test_cust_info), 
			     acc_num, 
			     32,
			     NULL, 
			     NULL,
			     PTR_TO_WORD(record)));
  return OK;
}

static void insert_records()
{
  ptr_t record, shtr_p;
  word_t key;
  int i, j, *hot_card_acc;
  char vstr1[MAX_STR_LEN + 1], vstr2[MAX_STR_LEN + 1], vstr3[MAX_STR_LEN + 1];
  char fstr1[FIX_STR_MAX_LEN + 1], fstr2[FIX_STR_MAX_LEN + 1];
  char fstr3[FIX_STR_MAX_LEN + 1];

  if (be_verbose)
    fprintf(stdout, "Creating tables \"%s\" and \"%s\"...\n", 
	    TABLE_NAME_ACC, TABLE_NAME_CUST);

  for (key = 0; key < sysm_size * NUM_RECORDS_TABLE_ACC; key++) {
    /* Record: ACC_INFO */
    get_random_string(fstr1, 7);
    get_random_string(fstr2, 7);

    while (!can_allocate(TRIEV2_MAX_ALLOCATION + REC_SIZE_ACC + 1))
      flush_batch();

    record = allocate(1 + REC_SIZE_ACC, CELL_word_vector);
    record[0] |= REC_SIZE_ACC;
    record[ACC_REC_ACC_IDX] = key;
    float_to_word((float) random_number(RANDOM_MAX - 1), 
		  &record[ACC_REC_LIM_IDX]); 

    /* Copy fixed strings using `memcpy'. */
    memcpy((void *) &record[ACC_REC_EXP_IDX], (void *) fstr1, 
	   strlen(fstr1) + 1);

    float_to_word((float) random_number(RANDOM_MAX - 1), 
		  &record[ACC_REC_UCR_IDX]);
    float_to_word((float) random_number(RANDOM_MAX - 1), 
		  &record[ACC_REC_ADV_IDX]); 
    float_to_word((float) random_number(RANDOM_MAX - 1), 
		  &record[ACC_REC_MIN_IDX]); 

    memcpy((void *) &record[ACC_REC_PAY_IDX], (void *) fstr2, 
	   strlen(fstr2) + 1);

    SET_ROOT_PTR(test_acc_info,
		 triev2_insert(GET_ROOT_PTR(test_acc_info), 
			       key,
			       32,
			       NULL, 
			       NULL,
			       PTR_TO_WORD(record)));

    /* Record: CUST_INFO */
    get_var_len_string(vstr1, 50);
    get_var_len_string(vstr2, 50);
    get_var_len_string(vstr3, 50);

    get_random_string(fstr1, 9);
    get_random_string(fstr2, 9);
    get_random_string(fstr3, 8);

    while (!can_allocate(TRIEV2_MAX_ALLOCATION + REC_SIZE_CUST + 
			 shtring_create_max_allocation(strlen(vstr1)) +
			 shtring_create_max_allocation(strlen(vstr2)) +
			 shtring_create_max_allocation(strlen(vstr3)) + 1))
      flush_batch();
    
    record = allocate(1 + REC_SIZE_CUST, CELL_tuple);

    record[0] |= 3 | ((REC_SIZE_CUST - 3) << 12);
    str_to_shtr(vstr1, &shtr_p);
    record[CUST_REC_NAME_IDX] = PTR_TO_WORD(shtr_p);
    str_to_shtr(vstr2, &shtr_p);
    record[CUST_REC_ADD_IDX] = PTR_TO_WORD(shtr_p);
    str_to_shtr(vstr3, &shtr_p);
    record[CUST_REC_ZIP_IDX] = PTR_TO_WORD(shtr_p);
    record[CUST_REC_ACC_IDX] = key;
    memcpy((void *) &record[CUST_REC_HPH_IDX], (void *) fstr1, 
	   strlen(fstr1) + 1);
    memcpy((void *) &record[CUST_REC_BPH_IDX], (void *) fstr2, 
	   strlen(fstr2) + 1);
    memcpy((void *) &record[CUST_REC_SOC_IDX], (void *) fstr3, 
	   strlen(fstr3) + 1);

    SET_ROOT_PTR(test_cust_info,
		 triev2_insert(GET_ROOT_PTR(test_cust_info), 
			       key, 
			       32,
			       NULL, 
			       NULL,
			       PTR_TO_WORD(record)));
  }

  /* Record: HOT_INFO */
  hot_card_acc = malloc(sysm_size * NUM_RECORDS_TABLE_HOT * sizeof(int));
  if (!hot_card_acc) {
    fprintf(stderr, "sysm_db_create: malloc failed.\n");
    exit(1);
  }
    
  /* Init integer array which keeps already chosen account numbers. */
  for (i = 0; i < sysm_size * NUM_RECORDS_TABLE_HOT; i++)
    hot_card_acc[i] = -1;

  if (be_verbose)
    fprintf(stdout, "Creating table \"%s\"...\n", TABLE_NAME_HOT);

  for (i = 0; i < sysm_size * NUM_RECORDS_TABLE_HOT; i++) {
    key = random_number(sysm_size * NUM_RECORDS_TABLE_ACC);
    
    for (j = 0; j < i; j++)
      if (hot_card_acc[j] == key)
	break;
    
    /* This key has already been chosen. */
    if (j < i) {  
      i--;
      continue;
    }

    hot_card_acc[i] = key;

    get_random_string(fstr1, 7);
    get_var_len_string(vstr1, 50);

    while (!can_allocate(TRIEV2_MAX_ALLOCATION + REC_SIZE_HOT +
			 shtring_create_max_allocation(strlen(vstr1) + 1)))
      flush_batch();

    record = allocate(1 + REC_SIZE_HOT, CELL_tuple);
    record[0] |= 1 | ((REC_SIZE_HOT - 1) << 12);

    /* Here we choose the name of the account holder randomly even
       though there is a name found for each account from the table 
       CUST_INFO, because the name match has no effect on the 
       transactions executed during this test. */
    str_to_shtr(vstr1, &shtr_p);
    record[HOT_REC_NAME_IDX] = PTR_TO_WORD(shtr_p);
    record[HOT_REC_ACC_IDX] = key;
    memcpy((void *) &record[HOT_REC_DATE_IDX], (void *) fstr1, 
	   strlen(fstr1) + 1);
    record[HOT_REC_ATT_IDX] = random_number(RANDOM_MAX - 1);
    
    SET_ROOT_PTR(test_hot_info,
		 triev2_insert(GET_ROOT_PTR(test_hot_info), 
			       key, 
			       32,
			       NULL, 
			       NULL,
			       PTR_TO_WORD(record)));
  }
  free(hot_card_acc);

  /* Record: STORE_INFO */
  if (be_verbose)
    fprintf(stdout, "Creating table \"%s\"...\n", TABLE_NAME_STORE);

  for (key = 0; key < sysm_size * NUM_RECORDS_TABLE_STORE; key++) {
    get_var_len_string(vstr1, 50);

    while (!can_allocate(TRIEV2_MAX_ALLOCATION + REC_SIZE_STORE +
			 shtring_create_max_allocation(strlen(vstr1) + 1)))
      flush_batch();

    record = allocate(1 + REC_SIZE_STORE, CELL_tuple);

    record[0] |= 1 | ((REC_SIZE_STORE - 1) << 12);
    str_to_shtr(vstr1, &shtr_p);
    record[STORE_REC_NAME_IDX] = PTR_TO_WORD(shtr_p);
    record[STORE_REC_NUM_IDX] = key;
    record[STORE_REC_CCCK_IDX] = random_number(RANDOM_MAX - 1);
    record[STORE_REC_CLCK_IDX] = random_number(RANDOM_MAX - 1);
    record[STORE_REC_DEB_IDX] = random_number(RANDOM_MAX - 1);
    
    SET_ROOT_PTR(test_store_info,
		 triev2_insert(GET_ROOT_PTR(test_store_info), 
			       key, 
			       32,
			       NULL, 
			       NULL,
			       PTR_TO_WORD(record)));
  }
}

static void float_to_word(float f, word_t *w)
{
  assert(sizeof(float) == sizeof(word_t));

  *w = *((word_t *) &f);
}

static void word_to_float(word_t w, float *f)
{
  assert(sizeof(float) == sizeof(word_t));

  *f = *((float *) &w);
}

static void get_var_len_string(char *sbuf, int avg_len)
{
  int len;
  
  len = random_number(avg_len) + avg_len / 2;
  if (len < avg_len) 
    len = avg_len;

  get_random_string(sbuf, len);
}

/* Note that `sbuf' has to have room for at least `slen' + 1 characters. */
static void get_random_string(char *sbuf, int slen)
{
  int i;
  char c = 'a' + random_number(26);

  for (i = 0; i < slen; i++)
    sbuf[i] = c;
  sbuf[i] = '\0';
}

static void str_to_shtr(char *str, ptr_t *shtr)
{
  assert(str != NULL);

  if (shtring_create(shtr, str, strlen(str)) != 1) {
    fprintf(stderr, "Fatal: Unable to create shtring.\n");
    exit(1);
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


/* Tools for SysM. */
static void print_table(char *table_name)
{
  ptr_t root_p, data;
  word_t key;
  long i, number_of_records = 0;
  char str[FIX_STR_MAX_LEN + 1];
  float flt;

  if (strcmp(table_name, TABLE_NAME_ACC) == 0) {
    root_p = GET_ROOT_PTR(test_acc_info);
    number_of_records = sysm_size * NUM_RECORDS_TABLE_ACC;

    fprintf(stdout,"\n*** Table: %s ***\n\n", TABLE_NAME_ACC);
    fprintf(stdout,"%s\t%s\t%s\t%s\t%s\t%s\t%s\n\n", "Acc", "Exp", "CrLim", 
	    "UsedCr", "CashAd", "MinDue", "PayDue");
    
    for (key = 0; key < number_of_records; key++) { 
      data = WORD_TO_PTR(triev2_find(root_p, key, 32));
      assert(data != NULL_PTR);

      for (i = 1; i <= REC_SIZE_ACC; i++) {
	if (i == 1)
	  fprintf(stdout, "%ld\t", data[i]);
	else if (i == 2 || i == 8) {
	  memcpy((void *) str, (void *) &data[i], 8);
	  fprintf(stdout, "%s\t", str);
	  i++;
	}
	else {
	  word_to_float(data[i], &flt);
	  fprintf(stdout, "%.2f\t", flt);
	}
      }

      fprintf(stdout, "\n");
    }
  }

  /* Other tables should be added here. XXX */
  
  return; 
}
