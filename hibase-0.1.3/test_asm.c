/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Fibonacci with the CPS byte code interpreter. */

#include "includes.h"
#include "asm.h"
#include "root.h"
#include "test_aux.h"
#include "interp.h"
#include "shtring.h"

static char *rev_id = "$Id: test_asm.c,v 1.11 1998/03/31 14:15:17 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


#define INIT_BCODE_NAME  "_init"


int main(int argc, char **argv)
{
  char *filename, *entry;
  word_t arg;
  ptr_t cont, bcode, bcode_string, s;
  unsigned long entry_length;
  shtring_intern_result_t sir;

  if (argc < 4) {
    fprintf(stderr, "Usage: %s filename entry_bcode arg\n", argv[0]);
    exit(1);
  }
  filename = argv[1];
  entry = argv[2];
  arg = atoi(argv[3]);
  argv[1] = argv[2] = argv[3] = NULL;

  /* Initialize the system. */
  srandom(0);
  init_interp(argc, argv);
  init_shades(argc, argv);
  create_db();

  if (asm_file(filename)) {
    fprintf(stderr, "Errors reported by the byte code assembler.  Exiting.\n");
    exit(1);
  }

  /* Try to find an init bcode, then the size of its continuation
     frame, and allocate and initialize the frame, and finally call
     it. */
  entry_length = strlen(INIT_BCODE_NAME);
  while (!can_allocate(shtring_create_max_allocation(entry_length)
		       + SHTRING_INTERN_MAX_ALLOCATION
		       + MAX_NUMBER_OF_WORDS_IN_CONT))
    flush_batch();
  if (shtring_create(&s, INIT_BCODE_NAME, entry_length) != 1) {
    fprintf(stderr, "test_asm/main/1: can_allocate() didn't suffice.\n");
    exit(2);
  }
  sir = shtring_intern(GET_ROOT_PTR(interned_shtrings), s);
  if (!sir.was_new) {
    bcode = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(bcodes),
				    sir.interned_shtring_id, 32));
    cont = allocate(BCODE_NUMBER_OF_WORDS_IN_CONT(bcode), CELL_cont);
    cont[0] |= (BCODE_NUMBER_OF_WORDS_IN_CONT(bcode) << 12) | 0xFFF;
    cont[1] = PTR_TO_WORD(bcode);
    cont[2] = NULL_WORD;
    interp(cont, arg, 0);
  }

  /* Find out the bcode, then the size of the continuation frame, and
     allocate and initialize the frame, and finally call it. */
  entry_length = strlen(entry);
  while (!can_allocate(shtring_create_max_allocation(entry_length)
		       + SHTRING_INTERN_MAX_ALLOCATION
		       + MAX_NUMBER_OF_WORDS_IN_CONT))
    flush_batch();
  if (shtring_create(&s, entry, entry_length) != 1) {
    fprintf(stderr, "test_asm/main/2: can_allocate() didn't suffice.\n");
    exit(2);
  }
  sir = shtring_intern(GET_ROOT_PTR(interned_shtrings), s);
  if (sir.was_new) {
    fprintf(stderr, "No such bcode `%s'.\n", entry);
    exit(1);
  }
  bcode = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(bcodes),
				  sir.interned_shtring_id, 32));
  cont = allocate(BCODE_NUMBER_OF_WORDS_IN_CONT(bcode), CELL_cont);
  cont[0] |= (BCODE_NUMBER_OF_WORDS_IN_CONT(bcode) << 12) | 0xFFF;
  cont[1] = PTR_TO_WORD(bcode);
  cont[2] = NULL_WORD;
  interp(cont, arg, 2);

#ifdef ENABLE_BCPROF

#define REPORT(x, oldx)  \
  x, (100.0 * ((double) (oldx) - (double) (x)) / (double) (oldx))

  fprintf(stderr, 
	  "Number of words allocated    = %9lu, %.3f%% total reduction\n",
	  REPORT(number_of_words_allocated, 75823078));
  fprintf(stderr,
	  "Number of insns executed     = %9lu, %.3f%% total reduction\n",
	  REPORT(number_of_insns_executed, 100465788));
  fprintf(stderr,
	  "Number of sequences executed = %9lu, %.3f%% total reduction\n",
	  REPORT(number_of_sequences_executed, 10778952));

#endif

  return 0;
}
