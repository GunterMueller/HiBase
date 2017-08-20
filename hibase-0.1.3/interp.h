/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* The byte-code interpreter.
 */


#ifndef INCL_INTERP
#define INCL_INTERP 1


#include "includes.h"
#include "trie.h"


typedef enum {
#define INSN(mnemonic, insn_size, imm_decl_block, insn_max_alloc, code_block) \
  INSN_PUSH_AND_ ## mnemonic, INSN_ ## mnemonic,
#include "insn-def.h"
#undef INSN
  NUMBER_OF_INSNS
} insn_t;


#define BCODE_ACCU_TYPE(bc)  ((bc)[1])
#define BCODE_CUR_STACK_DEPTH(bc)  ((bc)[2])
#define BCODE_NUMBER_OF_WORDS_IN_CODE(bc)  ((bc)[3])
#define BCODE_CONT_IS_REUSABLE(bc)  ((bc)[4])
#define BCODE_MAX_ALLOCATION(bc)  ((bc)[5])
#define BCODE_NUMBER_OF_WORDS_IN_CONT(bc)  ((bc)[6])
#define BCODE_ID(bc)  ((bc)[0] & ~CELL_TYPE_MASK)

/* Size of the `CELL_context'. */
#define CONTEXT_MAX_ALLOCATION  5

/* This is an apriori upper limit to how large the continuation frames
   can be.  254 words in a continuation frame requires a program with
   well over 200 nested expressions.  */
#define MAX_NUMBER_OF_WORDS_IN_CONT  254


/* Returns non-zero in case of error.  Should be done before calling
   `init_shades'. */
int init_interp(int argc, char **argv);


#ifdef ENABLE_BCPROF
extern word_t number_of_insns_executed, number_of_sequences_executed;
#endif


/* Given the insns of the bcode and its stack description at entry,
   fill in the missing gaps (such as allocation limits etc.), and
   construct the bcode sequence.  Returns non-zero if impossible due
   to first generation exhaustion because the size of the continuation
   frame would exceed the limit given in `bcode.h'. */
int load_bcode(char *name,
	       word_type_t accu_type, int cont_is_reusable, int bcode_is_entry,
	       word_type_t *stack_types,
	       unsigned long cur_stack_depth, unsigned long max_stack_depth,
	       ptr_t raw_bcode, unsigned long length_of_raw_bcode,
	       int *bcode_id_ptr);


/* Start executing the given `cont' with the given `accu' as argument
   and given `priority'.  If `cont == NULL_PTR', then try to continue
   executing other continuations. */
word_t interp(ptr_t cont, word_t accu, unsigned long priority);


#endif /* INCL_INTERP */
