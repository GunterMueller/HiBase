/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Kengatharan Sivalingam <siva@iki.fi>
 *          Antti-Pekka Liedes <apl@iki.fi>
 */

/* Instruction definitions.
 */


/* XXX This comment is only being written.

   `pc' is the program counter.  It must be updated one way or
   another, otherwise the byte code interpreter enters an infinite
   loop, issuing the same insn over and over again.

   `accu' is a register of the virtual machine.

   `sp' is the stack pointer.  It points to the first unused slot on
   the stack.  I.e., `sp[-1]' is the topmost of the stack, `sp[-2]' the
   next, etc.

   `cont' points to the current continuation frame.  `cont[1]' points
   to the byte code sequence.  `cont[2]' is the next-link of this
   continuation, which typically is used to indicate where this
   function should return to.  `cont[3]' is the lowermost item on
   stack, `cont[4]' the next, etc.

   `stack_start' points to the lowermost item on the stack.

   `bcode'.


   Complex insns may not change the word-level types of the items on
   stack and accu unless the insn is successfully entirely executed.
   For example, it is illegal for an insn to accept a WORD in `accu',
   store a PTR in `accu', and then `FLUSH_AND_RETRY_CONT'.  This
   restriction can easily be solved by, e.g., using auxiliary values
   on stack.
 */

#include "insn-obj.h"
#include "insn-calls.h"
#include "insn-string.h"

/* Stack manipulation insns. */

INSN(push,
     1,
     {
     },
     0,
     {
       *sp++ = accu;
       pc++;
     })

INSN(drop,
     1,
     {
     },
     0,
     {
       --sp;
       pc++;
     })

INSN(drop_n,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       sp -= pc[1];
       pc += 2;
     })


/* Evaluating variables and constants. */

INSN(pick,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu = stack_start[pc[1]];
       assert(accu != UNDECL_WORD_CLOBBER_COOKIE
	      || backtrace(cont));
       pc += 2;
     })

/* Move the given immediate value to the `accu'. */

INSN(t_load_imm,
     2,
     {
       DECLARE_TAGGED(pc[1]);
     },
     0,
     {
       accu = pc[1];
       pc += 2;
     })

INSN(get_global,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu = global_get(pc[1]);
       assert(accu != NULL_WORD);
       pc += 2;
     })

INSN(set_global,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     TRIEV2_MAX_ALLOCATION,
     {
       global_set(accu, pc[1]);
       pc += 2;
     })


/* Arithmetic insns. */

INSN(t_add,
     1,
     {
     },
     0,
     {
       assert(TAGGED_IS_WORD(accu));
       assert(TAGGED_IS_WORD(sp[-1]));

       accu += *--sp;
       /* Tagged words end with bits 01, which become 10 when summed
	  directly.  Decrease by one, and we're back to the original
	  tag bits. */
       accu--;
       pc++;
     })

INSN(t_add_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       assert(TAGGED_IS_WORD(accu));

       accu += pc[1];
       pc += 2;
     })

INSN(t_sub,
     1,
     {
     },
     0,
     {
       assert(TAGGED_IS_WORD(accu));
       assert(TAGGED_IS_WORD(sp[-1]));

       accu = *--sp - accu;
       /* Tagged words end with bits 01, which become 00 when summed
	  directly.  Increase by one, and we're back to the original
	  tag bits. */
       accu++;
       pc++;
     })
     
INSN(t_sub_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       assert(TAGGED_IS_WORD(accu));

       accu -= pc[1];
       pc += 2;
     })

INSN(t_neg,
     1,
     {
     },
     0,
     {
       assert(TAGGED_IS_WORD(accu));

       /* XXX Faster way? */
       accu = SIGNED_WORD_TO_TAGGED(-TAGGED_TO_SIGNED_WORD(accu));
       pc++;
     })

INSN(t_mul,
     1,
     {
     },
     0,
     {
       signed_word_t tmp = *--sp;

       assert(TAGGED_IS_WORD(accu));
       assert(TAGGED_IS_WORD(tmp));

       tmp = TAGGED_TO_SIGNED_WORD(accu) * TAGGED_TO_SIGNED_WORD(tmp);
       accu = SIGNED_WORD_TO_TAGGED(tmp);
       pc++;
     })

INSN(t_mul_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       assert(TAGGED_IS_WORD(accu));

       accu = SIGNED_WORD_TO_TAGGED(TAGGED_TO_SIGNED_WORD(accu) * pc[1]);
       /* Possible??? accu = ((accu ^ 0x1) * pc[1]) ^ 0x1; */
       pc += 2;
     })

INSN(t_div,
     1,
     {
     },
     0,
     {
       signed_word_t tmp = *--sp;

       assert(TAGGED_IS_WORD(accu));
       assert(TAGGED_IS_WORD(tmp));

       tmp = TAGGED_TO_SIGNED_WORD(tmp) / TAGGED_TO_SIGNED_WORD(accu);
       accu = SIGNED_WORD_TO_TAGGED(tmp);
       pc++;
     })

INSN(t_mod,
     1,
     {
     },
     0,
     {
       signed_word_t tmp = *--sp;

       assert(TAGGED_IS_WORD(accu));
       assert(TAGGED_IS_WORD(tmp));

       tmp = TAGGED_TO_SIGNED_WORD(tmp) % TAGGED_TO_SIGNED_WORD(accu);
       accu = SIGNED_WORD_TO_TAGGED(tmp);
       pc++;
     })

INSN(t_cmpeq,
     1,
     {
     },
     0,
     {
       if (*--sp == accu)
	 accu = TAGGED_TRUE;
       else
	 accu = TAGGED_FALSE;
       pc++;
     })

INSN(t_cmpne,
     1,
     {
     },
     0,
     {
       if (*--sp != accu)
	 accu = TAGGED_TRUE;
       else
	 accu = TAGGED_FALSE;
       pc++;
     })

INSN(t_cmplt,
     1,
     {
     },
     0,
     {
       if (*--sp < accu)
	 accu = TAGGED_TRUE;
       else
	 accu = TAGGED_FALSE;
       pc++;
     })

INSN(t_cmple,
     1,
     {
     },
     0,
     {
       if (*--sp <= accu)
	 accu = TAGGED_TRUE;
       else
	 accu = TAGGED_FALSE;
       pc++;
     })

INSN(t_cmpgt,
     1,
     {
     },
     0,
     {
       if (*--sp > accu)
	 accu = TAGGED_TRUE;
       else
	 accu = TAGGED_FALSE;
       pc++;
     })

INSN(t_cmpge,
     1,
     {
     },
     0,
     {
       if (*--sp >= accu)
	 accu = TAGGED_TRUE;
       else
	 accu = TAGGED_FALSE;
       pc++;
     })

INSN(t_not,
     1,
     {
     },
     0,
     {
       /* Regard the NULL_PTR (accu == 0) and tagged integer 0 
	  (accu == 1) as FALSE values. */
       /* Actually "accu = 5 - accu" might be sufficient? */
       if (accu < 2)
	 accu = TAGGED_TRUE;
       else
	 accu = TAGGED_FALSE;
       pc++;
     })

INSN(t_print,
     1,
     {
     },
     0,
     {
       if (!print_insns_are_disabled)
	 tagged_fprint(stdout, accu);
       fflush(stdout);
       pc++;
     })

INSN(r_cons,
     1,
     {
     },
     CONS_MAX_ALLOCATION,
     {
       accu = WCONS(accu, *--sp);
       pc++;
     })

INSN(car,
     1,
     {
     },
     0,
     {
       accu = WCAR(accu);
       pc++;
     })

INSN(cdr,
     1,
     {
     },
     0,
     {
       accu = WCDR(accu);
       pc++;
     })

INSN(load_null,
     1,
     {
     },
     0,
     {
       accu = NULL_WORD;
       pc++;
     })

INSN(load_imm_string,
     2,
     {
       DECLARE_NONNULL_PTR(pc[1]);
     },
     0,
     {
       accu = pc[1];
       pc += 2;
     })


/* Tuple manipulation. */

INSN(tuple_ref,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu = WORD_TO_PTR(accu)[pc[1] + 1];
       pc += 2;
     })

INSN(tuple_make,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     (1 + pc[1]),
     {
       ptr_t t = allocate(1 + pc[1], CELL_tuple);
       int i;

       assert(pc[1] > 0);
       t[0] |= pc[1];
       t[pc[1]] = accu;
       for (i = pc[1] - 1; i > 0; i--)
	 t[i] = *--sp;
       accu = PTR_TO_WORD(t);
       pc += 2;
     })


/* Branch insns. */

INSN(branch,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       pc = program_start + pc[1];
     })

INSN(beq,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       if (*--sp == accu)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

INSN(bne,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       if (*--sp != accu)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

INSN(blt,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       if (*--sp < accu)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

INSN(ble,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       if (*--sp <= accu)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

INSN(bgt,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       if (*--sp > accu)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

INSN(bge,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       if (*--sp >= accu)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

INSN(t_branch_if_false,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       /* Regard the NULL_PTR (accu == 0) and tagged integer 0 
	  (accu == 1) as FALSE values. */
       if (accu < 2)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

INSN(t_branch_if_true,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       if (accu < 2)
	 pc += 2;
       else
	 pc = program_start + pc[1];
     })

INSN(t_branch_if_not_cons,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       if (accu == NULL_WORD
	   || !TAGGED_IS_PTR(accu)
	   || CELL_TYPE(WORD_TO_PTR(accu)) != CELL_list)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

INSN(t_branch_if_not_null,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       if (accu != NULL_WORD)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

INSN(t_branch_if_not_ntuple,
     3,
     {
       DECLARE_WORD(pc[1]);	/* arity */
       DECLARE_WORD(pc[2]);	/* label */
     },
     0,
     {
       if (accu == NULL_WORD 
	   || !TAGGED_IS_PTR(accu)
	   || WORD_TO_PTR(accu)[0] !=
	        ((CELL_tuple << (32 - CELL_TYPE_BITS)) | pc[1]))
	 pc = program_start + pc[2];
       else
	 pc += 3;
     })

INSN(t_cond_eq_pick_pick,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp == accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(t_cond_le_pick_pick,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp <= accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(t_cond_lt_pick_pick,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp < accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(t_cond_eq_imm_pick,
     3,
     {
       DECLARE_TAGGED(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp == accu)
	 accu = pc[1];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(t_cond_le_imm_pick,
     3,
     {
       DECLARE_TAGGED(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp <= accu)
	 accu = pc[1];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(t_cond_lt_imm_pick,
     3,
     {
       DECLARE_TAGGED(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp < accu)
	 accu = pc[1];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(t_cond_eq_pick_imm,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_TAGGED(pc[2]);
     },
     0,
     {
       if (*--sp == accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(t_cond_le_pick_imm,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_TAGGED(pc[2]);
     },
     0,
     {
       if (*--sp <= accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(t_cond_lt_pick_imm,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_TAGGED(pc[2]);
     },
     0,
     {
       if (*--sp < accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(t_cond_eq_imm_imm,
     3,
     {
       DECLARE_TAGGED(pc[1]);
       DECLARE_TAGGED(pc[2]);
     },
     0,
     {
       if (*--sp == accu)
	 accu = pc[1];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(t_cond_le_imm_imm,
     3,
     {
       DECLARE_TAGGED(pc[1]);
       DECLARE_TAGGED(pc[2]);
     },
     0,
     {
       if (*--sp <= accu)
	 accu = pc[1];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(t_cond_lt_imm_imm,
     3,
     {
       DECLARE_TAGGED(pc[1]);
       DECLARE_TAGGED(pc[2]);
     },
     0,
     {
       if (*--sp < accu)
	 accu = pc[1];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(exit,
     1,
     {
     },
     0,
     {
       DIE_CONT;
     })

/* This insn is a temporary hack until we have chosen the right way of
   comparing the equality of complex objects. */
INSN(generic_equal,
     1,
     {
     },
     0,
     {
       if (TAGGED_IS_EQUAL(accu, sp[-1]))
	 accu = TAGGED_TRUE;
       else
	 accu = TAGGED_FALSE;
       sp--;
       pc++;
     })


/* Special insn for list appending.  

   This insn must be first in its byte code sequence.  It makes two
   passes through the list that is to be prefixed in front of the
   second list.  During the first pass it sets up a stack (implemented
   using a list) of every `LIST_APPEND_STEP_LENGTH'th item in the
   prefix.  During the second pass it uses this stack to copy
   `LIST_APPEND_STEP_LENGTH' last list nodes from the prefix to the
   front of the suffix, thereby gradually appending the lists. */

#define LIST_APPEND_STEP_LENGTH 20

#define SUFFIX  sp[-1]
#define PREFIX  accu
#define PREFIX_STACK  sp[-2]

INSN(list_append,
     1,
     {
     },
     (LIST_APPEND_STEP_LENGTH * CONS_MAX_ALLOCATION),
     {
       int i;

       /* tagged_fprint(stderr, PREFIX); fprintf(stderr, "\n");
	  tagged_fprint(stderr, PREFIX_STACK); fprintf(stderr, "\n"); */

       if (PREFIX != NULL_WORD) {
	 /* First pass. */
	 PREFIX_STACK = WCONS(PREFIX, PREFIX_STACK);
	 for (i = LIST_APPEND_STEP_LENGTH;
	      i > 0 && PREFIX != NULL_WORD;
	      i--)
	   PREFIX = WCDR(PREFIX);
	 RUN_CONT;
       } else if (PREFIX_STACK == NULL_WORD) {
	 /* Prefix is `[]', put suffix into accu and proceed. */
	 accu = SUFFIX;
	 sp -= 2;
	 pc++;
       } else {
	 /* Second pass through the list. */
	 ptr_t old_p = WORD_TO_PTR(WCAR(PREFIX_STACK));
	 ptr_t new_p;
	 word_t first_new_p;
	 ptr_t pp = &first_new_p;
	 
	 for (i = LIST_APPEND_STEP_LENGTH;
	      i > 0 && old_p != NULL_PTR;
	      i--) {
	   new_p = allocate(3, CELL_list);
	   new_p[1] = old_p[1];
	   *pp = PTR_TO_WORD(new_p);
	   pp = &new_p[2];
	   old_p = WORD_TO_PTR(CDR(old_p));
	 }
	 *pp = SUFFIX;
	 SUFFIX = first_new_p;
	 PREFIX_STACK = WCDR(PREFIX_STACK);
	 if (PREFIX_STACK != NULL_WORD)
	   RUN_CONT;
	 else {
	   /* We're finished, proceed to the next insn. */
	   accu = SUFFIX;
	   sp -= 2;
	   pc++;
	 }
       }
     })

#undef SUFFIX
#undef PREFIX
#undef PREFIX_STACK


/* No insns below should yet be produced by the Nolang compiler.  Some
   of these insns may vanish. */

INSN(pop,
     1,
     {
     },
     0,
     {
       accu = *--sp;
       pc++;
     })

INSN(dup,
     1,
     {
     },
     0,
     {
       word_t tmp = sp[-1];
       *sp++ = tmp;
       pc++;
     })

INSN(swap,
     1,
     {
     },
     0,
     {
       word_t tmp = sp[-1];
       sp[-1] = sp[-2];
       sp[-2] = tmp;
       pc++;
     })

INSN(exch,
     1,
     {
     },
     0,
     {
       word_t tmp = sp[-1];
       sp[-1] = accu;
       accu = tmp;
       pc++;
     })

/* Root block management. */

INSN(get_root_ptr,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu = root[pc[1]];
       pc += 2;
     })

INSN(set_root_ptr,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       root[pc[1]] = accu;
       pc += 2;
     })

/* Basic arithmetic. */

INSN(load_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu = pc[1];
       pc += 2;
     })

INSN(add,
     1,
     {
     },
     0,
     {
       accu += *--sp;
       pc++;
     })


INSN(add_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu += pc[1];
       pc += 2;
     })

INSN(sub,
     1,
     {
     },
     0,
     {
       accu = *--sp - accu;
       pc++;
     })

INSN(sub_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu -= pc[1];
       pc += 2;
     })

INSN(mul,
     1,
     {
     },
     0,
     {
       accu *= *--sp;
       pc++;
     })

INSN(mul_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu *= pc[1];
       pc += 2;
     })

INSN(div,
     1,
     {
     },
     0,
     {
       accu = *--sp / accu;
       pc++;
     })

INSN(mod,
     1,
     {
     },
     0,
     {
       accu = *--sp % accu;
       pc++;
     })

INSN(sqrt,
     1,
     {
     },
     0,
     {
       accu = (word_t) sqrt(accu);
       pc++;
     })

INSN(lshift_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu = accu << pc[1];
       pc += 2;
     })

INSN(t_lshift_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       assert(TAGGED_IS_WORD(accu));

       accu = accu << pc[1];
       pc += 2;
     })

INSN(rshift_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu = accu >> pc[1];
       pc += 2;
     })

INSN(t_rshift_imm,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       assert(TAGGED_IS_WORD(accu));

       accu = accu >> pc[1];
       pc += 2;
     })

/* Branch insns. */ 

INSN(cond_eq_pick_pick,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp == accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(cond_le_pick_pick,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp <= accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(cond_lt_pick_pick,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp < accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(cond_eq_imm_pick,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp == accu)
	 accu = pc[1];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(cond_le_imm_pick,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp <= accu)
	 accu = pc[1];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(cond_lt_imm_pick,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp < accu)
	 accu = pc[1];
       else 
	 accu = stack_start[pc[2]];
       pc += 3;
     })

INSN(cond_eq_pick_imm,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp == accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(cond_le_pick_imm,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp <= accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(cond_lt_pick_imm,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp < accu)
	 accu = stack_start[pc[1]];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(cond_eq_imm_imm,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp == accu)
	 accu = pc[1];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(cond_le_imm_imm,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp <= accu)
	 accu = pc[1];
       else 
	 accu = pc[2];
       pc += 3;
     })

INSN(cond_lt_imm_imm,
     3,
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
     },
     0,
     {
       if (*--sp < accu)
	 accu = pc[1];
       else 
	 accu = pc[2];
       pc += 3;
     })

/* Random number generators.  Serve also as a simple illustration of
   how to call C functions. */

INSN(init_random,
     1,
     {
     },
     0,
     {
       srandom(time(NULL));
       pc++;
     })

INSN(random_number,
     1,
     {
     },
     0,
     {
       word_t tmp;
       do 
	 tmp = ((word_t) random() & 0x7fffffffU);
       while (tmp >= (0x80000000U / accu) * accu);
       accu = tmp % accu;
       pc++;
     })

/* Printing.  Many of these are particularly useful as debugging tools
   since the don't meddle with the contents of the stack or accu. */

INSN(print_int,
     1,
     {
     },
     0,
     {
       if (!print_insns_are_disabled)
	 printf("%lu\n", (long) accu);
       pc++;
     })

INSN(print_list,
     1,
     {
     }, 
     0,
     {
       if (!print_insns_are_disabled)
	 print_list(WORD_TO_PTR(accu));
       pc++;
     })

INSN(print_cell,
     1,
     {
     },
     0,
     {
       if (!print_insns_are_disabled)
	 cell_fprint(stderr, WORD_TO_PTR(accu), 20);
       pc++;
     })

INSN(pick_print_cell,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       if (!print_insns_are_disabled) {
	 fprintf(stderr, "0x%08lX\n",
		 (unsigned long) WORD_TO_PTR(stack_start[pc[1]]));
	 cell_fprint(stderr, WORD_TO_PTR(stack_start[pc[1]]), 20);
       }
       pc += 2;
     })

INSN(print_cont,
     1,
     {
     },
     0,
     {
       if (!print_insns_are_disabled)
	 cell_fprint(stderr, cont, 1);
       pc++;
     })

INSN(print_table,
     1,
     {
     },
     0,
     {
       int rsize = *--sp;
       int num_records = *--sp;
       ptr_t rp = WORD_TO_PTR(accu);
       ptr_t data;
       if (!print_insns_are_disabled) {
	 int i;
	 int j;
	 for (i = 0; i < num_records; i++){
	   data = trie_find(rp, i);
	   for (j = 1; j < rsize + 1; j++)
	     fprintf(stdout, "%ld\t", data[j]);
	   fprintf(stdout,"\n");
	 }
       }
       pc++;
     })

/* Data structure insns.  Many of these will eventually be
   replaced. */

INSN(make_word_vector,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     (pc[1] + 1),
     {
       ptr_t p;
       unsigned long i;
       assert(can_allocate(pc[1] + 1));
       p = allocate(pc[1] + 1, CELL_word_vector);
       p[0] |= pc[1];
       sp -= pc[1] - 1;
       for (i = 0; i < pc[1] - 1; i++)
	 p[1 + i] = sp[i];
       p[pc[1]] = accu;
       accu = PTR_TO_WORD(p);
       pc += 2;
     })

INSN(copy_word_vector,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       ptr_t src = WORD_TO_PTR(*--sp);
       ptr_t copy = WORD_TO_PTR(accu);
       unsigned long i;
       copy[0] |= pc[1];
       for (i = 0; i < pc[1]; i++)
	 copy[1 + i] = src[1 + i];
       pc += 2;
     })

INSN(trie_insert,
     1,
     {
     },
     TRIE_MAX_ALLOCATION,
     {
       sp -= 2;
       accu = PTR_TO_WORD(trie_insert(WORD_TO_PTR(sp[0]),
				      sp[1],
				      NULL,
				      WORD_TO_PTR(accu)));
       pc++;
     })

INSN(trie_find,
     1,
     {
     },
     0,
     {
       accu = PTR_TO_WORD(trie_find(WORD_TO_PTR(*--sp), accu));
       pc++;
     })

INSN(get_field_value,
     1,
     {
     },
     0,
     {
       ptr_t p;
       p = WORD_TO_PTR(*--sp);
       accu = p[accu + 1];
       pc++;
     })

INSN(set_field_value,
     1,
     {
     },
     0,
     {
       int new_val = *--sp;
       ptr_t p = WORD_TO_PTR(*--sp);
       /* It is up to the user to ensure `p' is in the first generation. */
       p[accu + 1] = new_val;
       accu = PTR_TO_WORD(p);
       pc++;
     })

INSN(cons,
     1,
     {
     },
     CONS_MAX_ALLOCATION,
     {
       accu = WCONS(*--sp, accu);
       pc++;
     })

INSN(caar,
     1,
     {
     },
     0,
     {
      accu = WCAAR(accu);
      pc++;
     })

INSN(cdar,
     1,
     {
     },
     0,
     {
      accu = WCDAR(accu);
      pc++;
     })

INSN(cadr,
     1,
     {
     },
     0,
     {
      accu = WCADR(accu);
      pc++;
     })

INSN(cddr,
     1,
     {
     },
     0,
     {
      accu = WCDDR(accu);
      pc++;
     })

/* Function calls, jumps between byte code sequences. */

INSN(goto_bcode,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       cont[1] = PTR_TO_WORD(bcode_get(pc[1]));
       RUN_CONT;
     })

INSN(goto_self,
     1,
     {
     },
     0,
     {
       RUN_CONT;
     })

INSN(die,
     1,
     {
     },
     0,
     {
       DIE_CONT;
     })


INSN(spawn,
     4,				/* spawn bcode_id num_of_args priority */
     {
       DECLARE_WORD(pc[1]);
       DECLARE_WORD(pc[2]);
       DECLARE_WORD(pc[3]);
     },
     MAX_NUMBER_OF_WORDS_IN_CONT /* XXX */
       + CONTEXT_MAX_ALLOCATION
       + QUEUE_MAX_ALLOCATION,
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       ptr_t new_context;
       unsigned long i;

       /* Create a cont for the spawned function. */
       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = NULL_WORD;
       sp -= pc[2] - 1;
       for (i = 0; i < pc[2] - 1; i++)
	 new_cont[3 + i] = sp[i];
       /* Make a new thread from the continuation. */
       assert(can_allocate(CONTEXT_MAX_ALLOCATION));
       new_context = allocate(CONTEXT_MAX_ALLOCATION, CELL_context);
       new_context[0] |= CONT_ACCU_TYPE(new_cont);
       new_context[1] = PTR_TO_WORD(new_cont);
       new_context[2] = accu;
       SET_ROOT_WORD(highest_thread_id, GET_ROOT_WORD(highest_thread_id) + 1);
       new_context[3] = GET_ROOT_WORD(highest_thread_id);
       new_context[4] = pc[3];
       /* Insert the context to the queue of the given priority. */
       assert(pc[3] < NUMBER_OF_CONTEXT_PRIORITIES);
       SET_ROOT_PTR_VECTOR(contexts,
			   pc[3],
			   queue_insert_last(GET_ROOT_PTR_VECTOR(contexts,
								 pc[3]),
					     new_context));
       pc += 4;
     })


/* Arguments: accu:   port number

   Returns:   accu:   new handle
              sp[-1]: error status

   Insn args: label where to jump on fatal error

   Increases stack by 1 */

INSN(net_listen,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       net_return = net_listen(accu);
       accu = net_return.handle;
       *sp++ = net_return.error;
       if (NET_IS_FATAL_ERROR(net_return.error))
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

/* Arguments: accu:   handle of a listening connection

   Returns:   accu:   new handle
              sp[-1]: error status

   Insn args: label where to jump on fatal error

   Increases stack by 1 */

INSN(net_accept,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     CONTEXT_MAX_ALLOCATION + TRIE_MAX_ALLOCATION,
     {
       net_return = net_accept(thread_id, accu);
       if (net_return.error == NET_BLOCKED)
	 BLOCK_CONT;
       *sp++ = net_return.error;
       accu = net_return.handle;
       if (NET_IS_FATAL_ERROR(net_return.error))
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

/* Arguments: accu:   handle of the connection

   Returns:   accu:   character read
              sp[-1]: error status

   Insn args: label where to jump on fatal error

   Increases stack by 1 */

INSN(net_read_char,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     CONTEXT_MAX_ALLOCATION + TRIE_MAX_ALLOCATION,
     {
       char c;
       net_return = net_read_char(thread_id, accu, &c);
       if (net_return.error == NET_BLOCKED)
	 BLOCK_CONT;
       accu = c;
       *sp++ = net_return.error;
       if (NET_IS_FATAL_ERROR(net_return.error))
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

/* Arguments: accu:   handle of the connection
              sp[-1]: character to write

   Returns:   accu:   error status

   Insn args: label where to jump on fatal error

   Decreases stack by 1 */

INSN(net_write_char,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     CONTEXT_MAX_ALLOCATION + TRIE_MAX_ALLOCATION,
     {
       net_return = net_write_char(thread_id, accu, (char) sp[-1]);
       if (net_return.error == NET_BLOCKED)
	 BLOCK_CONT;
       sp--;
       accu = net_return.error;
       if (net_return.error)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

/* Arguments: accu:   handle of the connection
              sp[-1]: original shtring
	      sp[-2]: number of bytes

   Returns:   accu:   shtring read
              sp[-1]: error status

   Insn args: label where to jump on fatal error

   Decreases stack by 1 */

INSN(net_read_shtring,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     CONTEXT_MAX_ALLOCATION + TRIE_MAX_ALLOCATION,
     {
       net_return = net_read_shtring(thread_id,
				     WORD_TO_PTR(sp[-1]),
				     accu,
				     sp[-2]);
       if (net_return.error == NET_BLOCKED)
	 BLOCK_CONT;
       else if (net_return.error == NET_ALLOCATION_ERROR) {
	 sp[-1] = PTR_TO_WORD(net_return.shtring);
	 sp[-2] -= shtring_length(net_return.shtring);
	 FLUSH_AND_RETRY_CONT;
       }
       accu = PTR_TO_WORD(net_return.shtring);
       *--sp = net_return.error;
       if (NET_IS_FATAL_ERROR(net_return.error))
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

/* Arguments: accu:   handle of the connection
              sp[-1]: shtring to write
	      sp[-2]: number of bytes

   Returns:   accu:   error status

   Insn args: label where to jump on fatal error

   Decreases stack by 2 */

INSN(net_write_shtring,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     CONTEXT_MAX_ALLOCATION + TRIE_MAX_ALLOCATION,
     {
       net_return = net_write_shtring(thread_id, accu,
				      WORD_TO_PTR(sp[-1]), sp[-2]);
       if (net_return.error == NET_BLOCKED)
	 BLOCK_CONT;
       sp -= 2;
       accu = net_return.error;
       if (net_return.error)
	 pc = program_start + pc[1];
       else
	 pc += 2;
     })

/* Arguments: accu:   handle of the connection
   
   Returns:   accu:   error status */

INSN(net_close,
     1,
     {
       DECLARE_WORD(pc[1]);
     },
     CONTEXT_MAX_ALLOCATION + TRIE_MAX_ALLOCATION,
     {
       net_return = net_close(thread_id, accu);
       if (net_return.error == NET_BLOCKED)
	 BLOCK_CONT;
       assert(!net_return.error);
       pc++;
     })
