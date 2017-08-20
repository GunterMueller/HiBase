/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1998 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Function call insns.
 */


/* Make a copy of the cont frame in `accu' and send it `pc[1]'
   arguments from on top of the stack.  Leaves the new cont in
   `accu'. */
INSN(bind,
     2,
     {
       DECLARE_WORD(pc[1]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t old_cont = WORD_TO_PTR(accu);
       ptr_t new_cont;
       unsigned long i;

       new_cont = raw_allocate((old_cont[0] >> 12) & 0xFFF);
       accu = PTR_TO_WORD(new_cont);
       new_cont[0] = old_cont[0] + pc[1];
       assert(pc[1] < 0xFFF);
       new_cont[1] = old_cont[1];
       new_cont[2] = old_cont[2];
       assert((old_cont[0] & 0xFFF) == 0); /* We can bind a cont only once. */
       new_cont = &new_cont[3];
       /* Copy the bound values to the callee's stack frame. */
       sp -= pc[1];
       for (i = 0; i < pc[1]; i++)
	 *new_cont++ = sp[i];
       pc += 2;
     })

INSN(call,
     3,
     {
       DECLARE_WORD(pc[1]);	/* number of args */
       DECLARE_WORD(pc[2]);	/* next */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t proto_cont = WORD_TO_PTR(accu);
       ptr_t new_cont;
       ptr_t p;
       unsigned long i;

       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF; /* + pc[1] - 1 */
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       /* Copy previously bound or passed arguments of the callee. */
       p = &new_cont[3];
       for (i = 0; i < (proto_cont[0] & 0xFFF); i++)
	 *p++ = proto_cont[3 + i];
       /* Copy new arguments to the callee's cont and accu. */
       if (pc[1] == 0) {
	 if ((proto_cont[0] & 0xFFF) != 0)
	   accu = *--p;
       } else {
	 sp -= pc[1];
	 for (i = 0; i < pc[1] - 1; i++)
	   *p++ = sp[i];
	 accu = sp[pc[1] - 1];
       }
       /* XXX Doesn't do partial application test.  Should we? */
       cont[1] = PTR_TO_WORD(bcode_get(pc[2]));
       cont = new_cont;
       RUN_CONT;
     })

INSN(tail_call,
     2,
     {
       DECLARE_WORD(pc[1]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t proto_cont = WORD_TO_PTR(accu);
       ptr_t new_cont;
       ptr_t p;
       unsigned long i;

       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF; /* + pc[1] - 1 */
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       /* Copy previously bound or passed arguments of the callee. */
       p = &new_cont[3];
       for (i = 0; i < (proto_cont[0] & 0xFFF); i++)
	 *p++ = proto_cont[3 + i];
       /* Copy new arguments to the callee's cont and accu. */
       if (pc[1] == 0) {
	 if ((proto_cont[0] & 0xFFF) != 0)
	   accu = *--p;
       } else {
	 sp -= pc[1];
	 for (i = 0; i < pc[1] - 1; i++)
	   *p++ = sp[i];
	 accu = sp[pc[1] - 1];
       }
       /* XXX Doesn't do partial application test yet.  Should we? */
       accu = sp[pc[1] - 1];
       cont = new_cont;
       RUN_CONT;
     })

INSN(return,
     1,
     {
     },
     0,
     {
       cont = CONT_NEXT(cont);
       if (cont == NULL_PTR)
	 DIE_CONT;
       else
	 RUN_CONT;
     })

/* Insns dedicated to calling global functions. */

INSN(call_global,
     4,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode_id */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_WORD(pc[3]);	/* next as bcode_id */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       sp -= pc[2] - 1;
       for (i = 0; i < pc[2] - 1; i++)
	 new_cont[3 + i] = sp[i];
       cont[1] = PTR_TO_WORD(bcode_get(pc[3]));
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global01,		/* Specialized for arity 0 or 1. */
     4,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode_id */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_WORD(pc[3]);	/* next as bcode_id */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 0 || pc[2] == 1);
       cont[1] = PTR_TO_WORD(bcode_get(pc[3]));
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global2,
     4,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode_id */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_WORD(pc[3]);	/* next as bcode_id */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 2);
       new_cont[3] = sp[-1];
       cont[1] = PTR_TO_WORD(bcode_get(pc[3]));
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global3,
     4,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode_id */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_WORD(pc[3]);	/* next as bcode_id */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 3);
       new_cont[3] = sp[-2];
       new_cont[4] = sp[-1];
       cont[1] = PTR_TO_WORD(bcode_get(pc[3]));
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global4,
     4,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode_id */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_WORD(pc[3]);	/* next as bcode_id */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 4);
       new_cont[3] = sp[-3];
       new_cont[4] = sp[-2];
       new_cont[5] = sp[-1];
       cont[1] = PTR_TO_WORD(bcode_get(pc[3]));
       cont = new_cont;
       RUN_CONT;
     })


INSN(call_global_with_to_as_cont_ptr,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_WORD(pc[3]);	/* next as bcode_id */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       sp -= pc[2] - 1;
       for (i = 0; i < pc[2] - 1; i++)
	 new_cont[3 + i] = sp[i];
       cont[1] = PTR_TO_WORD(bcode_get(pc[3]));
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_to_as_cont_ptr01,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_WORD(pc[3]);	/* next as bcode_id */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 0 || pc[2] == 1);
       cont[1] = PTR_TO_WORD(bcode_get(pc[3]));
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_to_as_cont_ptr2,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_WORD(pc[3]);	/* next as bcode_id */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 2);
       new_cont[3] = sp[-1];
       cont[1] = PTR_TO_WORD(bcode_get(pc[3]));
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_to_as_cont_ptr3,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_WORD(pc[3]);	/* next as bcode_id */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 3);
       new_cont[3] = sp[-2];
       new_cont[4] = sp[-1];
       cont[1] = PTR_TO_WORD(bcode_get(pc[3]));
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_to_as_cont_ptr4,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_WORD(pc[3]);	/* next as bcode_id */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 4);
       new_cont[3] = sp[-3];
       new_cont[4] = sp[-2];
       new_cont[5] = sp[-1];
       cont[1] = PTR_TO_WORD(bcode_get(pc[3]));
       cont = new_cont;
       RUN_CONT;
     })


INSN(call_global_with_next_bcode_as_ptr,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode id */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_NONNULL_PTR(pc[3]); /* next as bcode ptr */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       sp -= pc[2] - 1;
       for (i = 0; i < pc[2] - 1; i++)
	 new_cont[3 + i] = sp[i];
       cont[1] = pc[3];
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_next_bcode_as_ptr01,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode id */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_NONNULL_PTR(pc[3]); /* next as bcode ptr */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 0 || pc[2] == 1);
       cont[1] = pc[3];
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_next_bcode_as_ptr2,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode id */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_NONNULL_PTR(pc[3]); /* next as bcode ptr */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 2);
       new_cont[3] = sp[-1];
       cont[1] = pc[3];
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_next_bcode_as_ptr3,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode id */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_NONNULL_PTR(pc[3]); /* next as bcode ptr */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 3);
       new_cont[3] = sp[-2];
       new_cont[4] = sp[-1];
       cont[1] = pc[3];
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_next_bcode_as_ptr4,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode id */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_NONNULL_PTR(pc[3]); /* next as bcode ptr */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 4);
       new_cont[3] = sp[-3];
       new_cont[4] = sp[-2];
       new_cont[5] = sp[-1];
       cont[1] = pc[3];
       cont = new_cont;
       RUN_CONT;
     })


INSN(call_global_with_to_as_cont_ptr_and_next_bcode_as_ptr,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_NONNULL_PTR(pc[3]); /* next as bcode ptr */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       sp -= pc[2] - 1;
       for (i = 0; i < pc[2] - 1; i++)
	 new_cont[3 + i] = sp[i];
       cont[1] = pc[3];
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_to_as_cont_ptr_and_next_bcode_as_ptr01,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_NONNULL_PTR(pc[3]); /* next as bcode ptr */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 0 || pc[2] == 1);
       cont[1] = pc[3];

assert(CELL_TYPE(WORD_TO_PTR(cont[1])) == CELL_bcode);

       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_to_as_cont_ptr_and_next_bcode_as_ptr2,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_NONNULL_PTR(pc[3]); /* next as bcode ptr */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 2);
       new_cont[3] = sp[-1];
       cont[1] = pc[3];
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_to_as_cont_ptr_and_next_bcode_as_ptr3,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_NONNULL_PTR(pc[3]); /* next as bcode ptr */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 3);
       new_cont[3] = sp[-2];
       new_cont[4] = sp[-1];
       cont[1] = pc[3];
       cont = new_cont;
       RUN_CONT;
     })

INSN(call_global_with_to_as_cont_ptr_and_next_bcode_as_ptr4,
     /* This insn is usually generated by the peephole optimization in
        the byte code assembler. */
     4,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
       DECLARE_NONNULL_PTR(pc[3]); /* next as bcode ptr */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = PTR_TO_WORD(cont);
       assert(pc[2] == 4);
       new_cont[3] = sp[-3];
       new_cont[4] = sp[-2];
       new_cont[5] = sp[-1];
       cont[1] = pc[3];
       cont = new_cont;
       RUN_CONT;
     })


INSN(tail_call_global,
     3,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode_id */
       DECLARE_WORD(pc[2]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       sp -= pc[2] - 1;
       for (i = 0; i < pc[2] - 1; i++)
	 new_cont[3 + i] = sp[i];
       cont = new_cont;
       RUN_CONT;
     })

INSN(tail_call_global01,
     3,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode_id */
       DECLARE_WORD(pc[2]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       assert(pc[2] == 0 || pc[2] == 1);
       cont = new_cont;
       RUN_CONT;
     })

INSN(tail_call_global2,
     3,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode_id */
       DECLARE_WORD(pc[2]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       assert(pc[2] == 2);
       new_cont[3] = sp[-1];
       cont = new_cont;
       RUN_CONT;
     })

INSN(tail_call_global3,
     3,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode_id */
       DECLARE_WORD(pc[2]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       assert(pc[2] == 3);
       new_cont[3] = sp[-2];
       new_cont[4] = sp[-1];
       cont = new_cont;
       RUN_CONT;
     })

INSN(tail_call_global4,
     3,
     {
       DECLARE_WORD(pc[1]);	/* to as bcode_id */
       DECLARE_WORD(pc[2]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(global_get(pc[1]));
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       assert(pc[2] == 4);
       new_cont[3] = sp[-3];
       new_cont[4] = sp[-2];
       new_cont[5] = sp[-1];
       cont = new_cont;
       RUN_CONT;
     })


INSN(tail_call_global_with_to_as_cont_ptr,
     3,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       sp -= pc[2] - 1;
       for (i = 0; i < pc[2] - 1; i++)
	 new_cont[3 + i] = sp[i];
       cont = new_cont;
       RUN_CONT;
     })

INSN(tail_call_global_with_to_as_cont_ptr01,
     3,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       assert(pc[2] == 0 || pc[2] == 1);
       cont = new_cont;
       RUN_CONT;
     })

INSN(tail_call_global_with_to_as_cont_ptr2,
     3,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       assert(pc[2] == 2);
       new_cont[3] = sp[-1];
       cont = new_cont;
       RUN_CONT;
     })

INSN(tail_call_global_with_to_as_cont_ptr3,
     3,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       assert(pc[2] == 3);
       new_cont[3] = sp[-2];
       new_cont[4] = sp[-1];
       cont = new_cont;
       RUN_CONT;
     })

INSN(tail_call_global_with_to_as_cont_ptr4,
     3,
     {
       DECLARE_NONNULL_PTR(pc[1]); /* to as cont ptr */
       DECLARE_WORD(pc[2]);	/* number of args */
     },
     MAX_NUMBER_OF_WORDS_IN_CONT, /* XXX */
     {
       ptr_t new_cont;
       ptr_t proto_cont;
       unsigned long i;

       proto_cont = WORD_TO_PTR(pc[1]);
       assert(proto_cont != NULL_PTR);
       new_cont = raw_allocate((proto_cont[0] >> 12) & 0xFFF);
       new_cont[0] = proto_cont[0] | 0xFFF;
       new_cont[1] = proto_cont[1];
       new_cont[2] = cont[2];
       assert(pc[2] == 4);
       new_cont[3] = sp[-3];
       new_cont[4] = sp[-2];
       new_cont[5] = sp[-1];
       cont = new_cont;
       RUN_CONT;
     })
