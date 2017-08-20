/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1998 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Object management insns.
 */


INSN(get_obj_field,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       accu = triev2_find_quick(WORD_TO_PTR(WORD_TO_PTR(accu)[1]), pc[1], 32);
       pc += 2;
     })

INSN(rebind_obj_field,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     TRIEV2_MAX_ALLOCATION + 2,
     {
       ptr_t old_obj = WORD_TO_PTR(*--sp);
       ptr_t new_obj = raw_allocate(2);

       new_obj[0] = old_obj[0];
       new_obj[1] = 
	 PTR_TO_WORD(triev2_replace(WORD_TO_PTR(old_obj[1]), pc[1], 32,
				    NULL, NULL, accu));
       accu = PTR_TO_WORD(new_obj);
       pc += 2;
     })


INSN(t_isa,
     1,
     {
     },
     0,
     {
       sp--;
       if (accu == NULL_WORD)
	 accu = TAGGED_TRUE;
       else if (sp[0] == NULL_WORD)
	 accu = TAGGED_FALSE;
       else {
	 word_t id = WORD_TO_PTR(accu)[0] & ~CELL_TYPE_MASK;
	 ptr_t isa = 
	   WORD_TO_PTR(triev2_find_quick(WORD_TO_PTR(WORD_TO_PTR(sp[0])[1]),
					 0, 32));
	 if (triev2_contains(isa, id, 32))
	   accu = TAGGED_TRUE;
	 else
	   accu = TAGGED_FALSE;
       }
       pc++;
     })

INSN(t_branch_if_not_isa,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     0,
     {
       sp--;
       if (accu == NULL_WORD)
	 pc += 2;
       else if (sp[0] == NULL_WORD)
	 pc = program_start + pc[1];
       else {
	 word_t id = WORD_TO_PTR(accu)[0] & ~CELL_TYPE_MASK;
	 ptr_t isa = 
	   WORD_TO_PTR(triev2_find_quick(WORD_TO_PTR(WORD_TO_PTR(sp[0])[1]),
					 0, 32));
	 if (triev2_contains(isa, id, 32))
	   pc += 2;
	 else
	   pc = program_start + pc[1];
       }
     })


INSN(get_obj_method,
     3,
     {
       DECLARE_WORD(pc[1]);	/* `__traits' */
       DECLARE_WORD(pc[2]);	/* method id */
     },
     0,
     {
       ptr_t obj = WORD_TO_PTR(accu);
       word_t id = obj[0] & ~CELL_TYPE_MASK;
       ptr_t traits = WORD_TO_PTR(global_get(pc[1]));
       ptr_t trait = WORD_TO_PTR(triev2_find_quick(traits, id, 32));
       accu = triev2_find_quick(trait, pc[2], 32);
       pc += 3;
     })


/* All insns below are used only during prototype initialization. */

INSN(insert_obj_field,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     TRIEV2_MAX_ALLOCATION + 2,
     {
       ptr_t old_obj = WORD_TO_PTR(*--sp);
       ptr_t new_obj = raw_allocate(2);

       new_obj[0] = old_obj[0];
       new_obj[1] = 
	 PTR_TO_WORD(triev2_insert(WORD_TO_PTR(old_obj[1]), pc[1], 32,
				   NULL, NULL, accu));
       accu = PTR_TO_WORD(new_obj);
       pc += 2;
     })

INSN(declare_obj_isa,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     (2 * TRIEV2_MAX_ALLOCATION) + 2, /* XXX Magic constant */
     {
       ptr_t old = WORD_TO_PTR(accu);
       ptr_t old_isa = WORD_TO_PTR(triev2_find(WORD_TO_PTR(old[1]), 0, 32));
       ptr_t new_isa = triev2_insert(old_isa, pc[1], 32,
				     NULL, NULL, TAGGED_TRUE);
       ptr_t new = raw_allocate(2);
       new[0] = old[0];
       new[1] = PTR_TO_WORD(triev2_insert(WORD_TO_PTR(old[1]), 0, 32,
					  NULL, NULL, PTR_TO_WORD(new_isa)));
       accu = PTR_TO_WORD(new);
       pc += 2;
     })

INSN(insert_obj_method,
     3,
     {
       DECLARE_WORD(pc[1]);	/* `__traits' */
       DECLARE_WORD(pc[2]);	/* method id */
     },
     (3 * TRIEV2_MAX_ALLOCATION),
     {
       ptr_t obj = WORD_TO_PTR(*--sp);
       word_t id = obj[0] & ~CELL_TYPE_MASK;
       ptr_t traits = WORD_TO_PTR(global_get(pc[1]));
       ptr_t trait = WORD_TO_PTR(triev2_find(traits, id, 32));
       trait = triev2_insert(trait, pc[2], 32, NULL, NULL, accu);
       traits = triev2_insert(traits, id, 32, NULL, NULL, PTR_TO_WORD(trait));
       global_set(PTR_TO_WORD(traits), pc[1]);
       accu = *sp;
       pc += 3;
     })


INSN(make_new_proto,
     2,
     {
       DECLARE_WORD(pc[1]);
     },
     2,				/* XXX Magic constant */
     {
       ptr_t obj;

       obj = allocate(2, CELL_object);
       obj[0] |= pc[1];
       obj[1] = NULL_WORD;
       accu = PTR_TO_WORD(obj);
       pc += 2;
     })

INSN(inherit_new_proto,
     3,
     {
       DECLARE_WORD(pc[1]);	/* `__traits' */
       DECLARE_WORD(pc[2]);	/* class id. */
     },
     (2 * TRIEV2_MAX_ALLOCATION + 2), /* XXX Magic constant */
     {
       ptr_t traits;
       word_t trait;
       ptr_t obj;
       word_t old_id = WORD_TO_PTR(accu)[0] & ~CELL_TYPE_MASK;

       obj = allocate(2, CELL_object);
       obj[0] |= pc[2];
       obj[1] = WORD_TO_PTR(accu)[1];
       accu = PTR_TO_WORD(obj);

       traits = WORD_TO_PTR(global_get(pc[1]));
       trait = triev2_find(traits, old_id, 32);
       global_set(PTR_TO_WORD(triev2_insert(traits, pc[2], 32,
					    NULL, NULL, trait)),
		  pc[1]);

       pc += 3;
     })

#define CTR_AS_TAGGED  accu
#define OVERRIDDEN     sp[-1]
#define OVERRIDER      sp[-2]

INSN(multiple_inherit,
     2,
     {
       DECLARE_WORD(pc[1]);	/* `__traits' */
     },
     (6 * TRIEV2_MAX_ALLOCATION + 2 * 2), /* XXX Magic constants. */
     {
       word_t ctr = CTR_AS_TAGGED >> 2;
       ptr_t traits;
       ptr_t overrider = WORD_TO_PTR(OVERRIDER);
       ptr_t overrider_isa;
       word_t overrider_id;
       ptr_t overrider_trait;
       ptr_t overridden = WORD_TO_PTR(OVERRIDDEN);
       ptr_t overridden_isa;
       word_t overridden_id;
       ptr_t overridden_trait;
       ptr_t new;
       word_t tmp;
       int finished = 1;

       assert(overrider[1] != NULL_WORD);

       if (ctr <= triev2_find_max(WORD_TO_PTR(overrider[1]), 32)) {
	 finished = 0;
	 if (ctr != 0 && triev2_contains(WORD_TO_PTR(overrider[1]), ctr, 32)) {
	   /* Inherit a field. */
	   new = raw_allocate(2);
	   new[0] = overridden[0];
	   new[1] = 
	     PTR_TO_WORD(
	       triev2_insert(WORD_TO_PTR(overridden[1]), ctr, 32, NULL, NULL,
			     triev2_find_quick(WORD_TO_PTR(overrider[1]),
					       ctr, 32)));
	   OVERRIDDEN = PTR_TO_WORD(new);
	   overridden = new;
	 }
       }

       overrider_isa = 
	 WORD_TO_PTR(triev2_find_quick(WORD_TO_PTR(overrider[1]), 0, 32));
       overridden_isa = 
	 WORD_TO_PTR(triev2_find_quick(WORD_TO_PTR(overridden[1]), 0, 32));
       if (ctr <= triev2_find_max(overrider_isa, 32)) {
	 finished = 0;
	 if (triev2_contains(overrider_isa, ctr, 32)
	     && !triev2_contains(overridden_isa, ctr, 32)) {
	   /* Inherit ISAness. */
	   new = raw_allocate(2);
	   new[0] = overridden[0];
	   overrider_isa = triev2_insert(overridden_isa, ctr, 32,
					 NULL, NULL, TAGGED_TRUE);
	   new[1] =
	     PTR_TO_WORD(
	       triev2_insert(WORD_TO_PTR(overridden[1]), 0, 32,
			     NULL, NULL, PTR_TO_WORD(overrider_isa)));
	   OVERRIDDEN = PTR_TO_WORD(new);
	   overridden = new;
	 }
       }

       traits = WORD_TO_PTR(global_get(pc[1]));
       overrider_id = overrider[0] & ~CELL_TYPE_MASK;
       overrider_trait = WORD_TO_PTR(triev2_find(traits, overrider_id, 32));
       overridden_id = overridden[0] & ~CELL_TYPE_MASK;
       overridden_trait = WORD_TO_PTR(triev2_find(traits, overridden_id, 32));
       if (ctr <= triev2_find_max(overrider_trait, 32)) {
	 finished = 0;
	 if (triev2_contains(overrider_trait, ctr, 32)) {
	   /* Inherit METHOD. */
	   tmp = triev2_find_quick(overrider_trait, ctr, 32);
	   overridden_trait = 
	     triev2_insert(overridden_trait, ctr, 32, NULL, NULL, tmp);
	   traits = triev2_insert(traits, overridden_id, 32, NULL, NULL,
				  PTR_TO_WORD(overridden_trait));
	   global_set(PTR_TO_WORD(traits), pc[1]);
	 }
       }

       if (finished) {
	 /* Inheritance has now fully happened.  Proceed to next
            insn. */
	 accu = OVERRIDDEN;
	 pc += 2;
	 sp -= 2;
       } else {
	 ctr++;
	 CTR_AS_TAGGED = (ctr << 2) + 1;
	 RUN_CONT;
       }
     })

#undef CTR_AS_TAGGED
#undef OVERRIDDEN
#undef OVERRIDER

