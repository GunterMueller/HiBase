/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Declarations of the low-level cell types and type tags understood
 * by the shades recovery algorithm. 
 */

/* Each `CELL(...)' declaration below defines a new low-level data
   type.  The `CELL'-macro takes three arguments:

   1. The name of the type, as a valid C identifier.  The name of the
      type is used rarely in the actual code, mostly for debugging
      purposes.

   2. An expression that evaluates to the size of the cell in words,
      including the first word that contains the type tag.  The
      expression can assume a variable `p' that is of type `word_t *'
      and points to the first word of the cell.  The variable `p0'
      contains the value of the first word in the cell, i.e. `p[0]'.
      The expression may not alter the cell or the value of the
      pointer `p'.  The expression may not refer to other cells or
      auxiliary data structures.

      The size of the cell may vary.  No cell may be smaller than two
      words.  No cell can be larger than approximately a few less than
      `PAGE_SIZE', though usually approximately a hundred words is a
      reasonably upper limit.  Frequently updated cells should be no
      larger than e.g. 8 to 12 words.

   3. A block of low-level type declarations of the words in the cell.
      Each declaration is one of the following:
          DECLARE_PTR(x)
          DECLARE_WORD(x)
          DECLARE_NONNULL_PTR(x)
	  DECLARE_TAGGED(x)

      The first line declares that the word `x' of the cell type being
      declared is a pointer to another cell, the second line declares
      that the word `x' is mere raw data.  The expression `x' can use
      the variable `word_t *p' which points to the cell's first word.
      The third line declares that `x' is a pointer, but never a
      `NULL_PTR'.  A tagged value is a PTR if its two lowest bits are
      00, and a WORD it they are 01.

      The uppermost 8 bits in `p0' contain the type tag.  The other
      bits in `p0' may contain other information, though not a
      pointer.  `p0' is implicitely declared and handled as word-like
      data.

      Note that the cell declaration block can contain normal C-code,
      such as `if-then-else's, loops, variables etc., although they
      must be syntactically constrained so that they can be passed as
      C's macro arguments.  (You can not, e.g., use a comma outside of
      parentheses.)

      Cell declaration blocks can refer to other cells pointed to by
      the cell being declared, but they must check for forward
      pointers.  See, e.g., `CELL_cont' for an example. */





CELL(bonk,
     2,
     {
       assert(0);
       abort();
     })

/* Cells used by index structures.  The two first cells (the list and
   the word_vector) are rather educational of how to implement your
   own cell types. */


/* A three-word LISP-list.  See `list.[hc]'. */
CELL(list,
     3,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
     })


CELL(object,
     2,
     {
       DECLARE_PTR(p[1]);	/* Ptr to field trie. */
     })


/* Cell types related to OIDs and maintaining them efficiently. */

CELL(oid_freelist,
     3,
     {
       DECLARE_WORD(p[1]);	/* Unused OID without the tag. */
       DECLARE_PTR(p[2]);	/* The next-pointer for the list. */
     })

CELL(oid_object,
     4,
     {
       DECLARE_PTR(p[1]);	/* Ptr to method trie. */
       DECLARE_PTR(p[2]);	/* Ptr to field trie. */
       DECLARE_WORD(p[3]);	/* This object's OID. */
     })


/* This declaration shows how one can implement varisized objects.
   Here the three lowest bytes in `p0' tell how long the vector is. */
CELL(word_vector,
     (p0 & 0xFFFFFF) + 1,
     {
       int i;
       int j = p0 & 0xFFFFFF;
       for (i = 1; i <= j; i++)
	 DECLARE_WORD(p[i]);
     })

/* Used in dynamic hashing. Internal node for n-ary tree.  */
CELL(internal,
     INTERNAL_NODE_SIZE + 1,
     {
       int i;
       for (i = 1; i <= INTERNAL_NODE_SIZE; i++)
	 DECLARE_PTR(p[i]);
     })

/* Leaf cell for `p0' & 0xFFFFFF items. */
CELL(leaf,
     (2 * (p0 & 0xFFFFFF)) + 1,
     {
       int i;
       int j = 2 * (p0 & 0xFFFFFF);
       for (i = 1; i <= j; i += 2) {
	 DECLARE_WORD(p[i]);    /* Key. */
	 DECLARE_PTR(p[i + 1]); /* Data. */
       }
     })

/* Header of a Hood-Melville queue, see `queue.[hc]'. */
CELL(queue,
     5,
     {
       DECLARE_NONNULL_PTR(p[1]); /* Head. */
       DECLARE_PTR(p[2]);	  /* Tail. */
       DECLARE_WORD(p[3]);	  /* Number of keys in the head. */
       DECLARE_WORD(p[4]);	  /* Number of keys in the tail. */
     })

/* Header of a queue when reversal process is unfinished. */
CELL(queue_rev,
     11,
     {
       DECLARE_NONNULL_PTR(p[1]); /* Head. */
       DECLARE_PTR(p[2]);	  /* Tail. */
       DECLARE_WORD(p[3]);	  /* Number of keys in the head. */
       DECLARE_WORD(p[4]);	  /* Number of keys in the tail. */
       DECLARE_PTR(p[5]);         /* `old_tail'. */
       DECLARE_PTR(p[6]);	  /* `new_head_end'. */
       DECLARE_PTR(p[7]);	  /* `old_head'. */
       DECLARE_PTR(p[8]);	  /* `old_head_reversed'. */
       DECLARE_WORD(p[9]);	  /*  Number of keys inserted from p[8] 
				    to p[6]. */
       DECLARE_WORD(p[10]);       /*  Number of keys in head when reversal
				    process started. */
     })

/* Header of a logarithmic queue, see `lq.[hc]'. */
CELL(lq_entry_point,
     3,
     {
       DECLARE_PTR(p[1]);	  /* Head. */
       DECLARE_PTR(p[2]);	  /* Tail. */
     })

CELL(lq_item00,
     2,
     {
       DECLARE_PTR(p[1]);         /* Data. */ 
     })

CELL(lq_item01,
     3,
     {
       DECLARE_PTR(p[1]);         /* Data. */ 
       DECLARE_NONNULL_PTR(p[2]); /* Brother. */ 
     })

CELL(lq_item10,
     3,
     {
       DECLARE_PTR(p[1]);         /* Data. */ 
       DECLARE_NONNULL_PTR(p[2]); /* Right son. */ 
     })

CELL(lq_item11,
     4,
     {
       DECLARE_PTR(p[1]);         /* Data. */ 
       DECLARE_NONNULL_PTR(p[2]); /* Brother. */ 
       DECLARE_NONNULL_PTR(p[3]); /* Right Son. */ 
     })

/* Priority queue consists of `CELL_priq_item's.  See `priq.[hc]'. */
CELL(priq_item,
     4,
     {
       DECLARE_PTR(p[1]);       /* Item */
       DECLARE_PTR(p[2]);       /* Son */
       DECLARE_PTR(p[3]);       /* Next */
     })


/* 2-3-4 tree consists of `CELL_ist234's. `p[0] & 0xFFFFFF' = 2,3, or 4. */
CELL(ist234,
     3 * (p0 & 0xFFFFFF) - 1,
     {
       int i;
       int cell_size = 3 * (p0 & 0xFFFFFF) - 1;
       for (i = 1; i < cell_size; i++) 
	 DECLARE_TAGGED(p[i]);       
     })

/* Internal AVL-node. Three lowest bytes in p[0] tells the height of node. */
CELL(avl_internal,
     4,
     {
       DECLARE_NONNULL_PTR(p[1]);       /* Key */
       DECLARE_NONNULL_PTR(p[2]);       /* Left son */
       DECLARE_NONNULL_PTR(p[3]);       /* Right son */
     })

CELL(avl_leaf,
     3,
     {
       DECLARE_NONNULL_PTR(p[1]);       /* Key */
       DECLARE_PTR(p[2]);               /* Item */
     })

/* This is a node of list that represents a part of path from root to
 * a leaf in AVL tree.
 *
 * If (CELL_TYPE(p) == CELL_history)  
 *   Let  node_in_path = WORD_TO_PTR(p[1]);
 *   WORD_TO_PTR(node_in_path[p[0] & 0xF]) is son of node_in_path;
 */
CELL(history,
     3,
     {
       DECLARE_NONNULL_PTR(p[1]); /* Pointer to internal node of AVL-tree. */ 
       DECLARE_PTR(p[2]); /* Pointer to previous node in path. */
     })


/* Nodes of the compressed quad-trie.  See `trie.[hc]'. */
CELL(quad_trie,
     5,
     {
       DECLARE_PTR(p[1]);
       DECLARE_PTR(p[2]);
       DECLARE_PTR(p[3]);
       DECLARE_PTR(p[4]);
     })


/* This implements tuples and records. */
CELL(tuple,
     (p0 & 0xFFF) + ((p0 >> 12) & 0xFFF) + 1,
     {
       unsigned int i;
       for (i = 1; i <= (p0 & 0xFFF); i++)
	 DECLARE_TAGGED(p[i]);
       for (i = 1; i <= ((p0 >> 12) & 0xFFF); i++)
	 DECLARE_WORD(p[(p0 & 0xFFF) + i]);
     })


/* Cells used by Shades's strings, shtrings. */

/* NOTE: The macro SHTRING_WORDS must agree with the size below. It is
   defined in shtring.h. */

/* The shtring descriptor. */
CELL(shtring,
     5,
     {
       DECLARE_WORD(p[1]);	/* Starting offset in chunk sequence. */
       DECLARE_WORD(p[2]);	/* Length of substring in chunk sequence. */
       DECLARE_WORD(p[3]);	/* Height of chunk tree. */
       DECLARE_PTR(p[4]);	/* Pointer to root node in chunk tree. */
     })

/* The shtring chunk: a variable-length sequence of characters. */
CELL(shtring_chunk,
     ((p0 & 0xFFFFFF) + sizeof(word_t) - 1) / sizeof(word_t) + 1,
     {
       int i;
       int j = ((p0 & 0xFFFFFF) + sizeof(word_t) - 1) / sizeof(word_t);
       for (i = 1; i <= j; ++i)
	 DECLARE_WORD(p[i]);
     })

/* A node in a chunk tree. */
CELL(shtring_node,
     (p0 & 0xFF) + 3,	/* p0 contains number of children */
     {
       int i;
       int j = p0 & 0xFF;
       DECLARE_WORD(p[1]);	/* Sum of lengths of chunks in tree. */
       DECLARE_WORD(p[2]);	/* Height of this tree. */
       for (i = 1; i <= j; ++i)
         DECLARE_NONNULL_PTR(p[2+i]);	/* Children. */
     })


/* Root of data structure for holding interned shtrings. */
CELL(shtring_intern_root,
     4,
     {
       DECLARE_NONNULL_PTR(p[1]);	/* Trie with shtrings, key is hash. */
       DECLARE_NONNULL_PTR(p[2]);	/* Trie with shtrings, key is id. */
       DECLARE_WORD(p[3]);		/* Number of shtrings so far. */
     })

/* Node in list of shtrings with the same hash value. */
CELL(shtring_intern_node,
     4,
     {
       DECLARE_PTR(p[1]);		/* Next node in list, or NULL_PTR. */
       DECLARE_NONNULL_PTR(p[2]);	/* The interned shtring. */
       DECLARE_WORD(p[3]);		/* Intern_ID for shtring. */
     })

/* A shtring cursor marks a position in a shtring. It keeps track of the
   node on each level, and the child of that node, by using a stack
   implemented as a linked list.

   The stack is implemented using the list functions in list.[ch].
   CAR (i.e., p[1]) is the pointer to the node and p[0]&0xFFFFFF
   is the current child of the node. CDR (i.e., p[2]) is the next
   element in the stack. The topmost element in the stack is the
   leafmost node. */
CELL(shtring_cursor,
     5,
     {
       DECLARE_NONNULL_PTR(p[1]);	/* shtring */
       DECLARE_WORD(p[2]);	/* offset from beginning of chunk tree
       				   to beginning of current chunk */
       DECLARE_WORD(p[3]);	/* offset from beginning of current chunk */
       DECLARE_PTR(p[4]);	/* top of stack */
     })


/* Cells used by the byte code interpreter. */

CELL(bcode,
     (p[2] + p[3] + 7),
     {
       unsigned long i;
       unsigned long j = p[3] + 7;
       ptr_t pc;
       DECLARE_WORD(p[1]);	/* Type of accu */
       DECLARE_WORD(p[2]);	/* `cur_stack_depth' */
       DECLARE_WORD(p[3]);	/* `number_of_words_in_code' */
       DECLARE_WORD(p[4]);	/* `cont_is_reusable' */
       DECLARE_WORD(p[5]);	/* `max_allocation' */
       DECLARE_WORD(p[6]);	/* `number_of_words_in_cont' */
       pc = &p[7];
       do {			/* bytecodes */
	 DECLARE_WORD(*pc);
	 switch (*pc) {
#define INSN(mnemonic, insn_size, imm_decl_block, max_allocation, code_block) \
	 case INSN_PUSH_AND_ ## mnemonic:				      \
	 case INSN_ ## mnemonic:					      \
	   imm_decl_block;						      \
	   pc += (insn_size);						      \
	   break;
#include "insn-def.h"
#undef INSN
	 default:
	   assert(0);
	   abort();
	   break;
	 }
       } while (pc < &p[j]);
       for (i = 0; i < p[2]; i++)
	 DECLARE_WORD(p[j+i]);	/* `stack_pos_is_ptr' */
     })

/* In addition to a full description of the continuation frame's
   contents in the bcode, the 12 middle bits in the first word in the
   cont itself tell how large (in words) the cont is.  The lowest
   bits, unless 0xFFF, tell how deep the stack is. */
CELL(cont,
     ((p0 >> 12) & 0xFFF),
     {
       ptr_t bcode;
       unsigned int i;
       unsigned int stack_depth;

       DECLARE_NONNULL_PTR(p[1]); /* bcode */
       DECLARE_PTR(p[2]);	/* return to that cont */
       /* Grok the pointer to the bcode of this continuation frame. */
       bcode = WORD_TO_PTR(p[1]);
       /* Check for forwarding pointer. */
       if (CELL_TYPE(bcode) == CELL_forward_pointer)
	 bcode = WORD_TO_PTR(bcode[1]);
       /* Deduce the current stack depth. */
       if ((p0 & 0xFFF) == 0xFFF)
	 /* This is used for conts which are already being executed. */
	 stack_depth = bcode[2];
       else {
	 /* This is used for conts which don't yet have all their
            arguments sent. */
	 stack_depth = p0 & 0xFFF;
	 assert(stack_depth <= bcode[2]);
       }
       bcode += 7 + bcode[3];
       for (i = 0; i < stack_depth; i++)
	 switch (bcode[i]) {
	 case WORD:
	   DECLARE_WORD(p[3 + i]);
	   break;
	 case PTR:
	   DECLARE_PTR(p[3 + i]);
	   break;
	 case NONNULL_PTR:
	   DECLARE_NONNULL_PTR(p[3 + i]);
	   break;
	 case TAGGED:
	   DECLARE_TAGGED(p[3 + i]);
	   break;
	 case VOID:
	   break;
	 default:
	   assert(0);
	 }
     })

CELL(context,
     5,
     {
       DECLARE_NONNULL_PTR(p[1]); /* cont */
       switch (p0 & 0xFFFFFF) {	/* accu */
       case WORD:
	 DECLARE_WORD(p[2]);
	 break;
       case PTR:
	 DECLARE_PTR(p[2]);
	 break;
       case NONNULL_PTR:
	 DECLARE_NONNULL_PTR(p[2]);
	 break;
       case TAGGED:
	 DECLARE_TAGGED(p[2]);
	 break;
       case VOID:
	 break;
       default:
	 assert(0);
       }
       DECLARE_WORD(p[3]);	/* thread_id */
       DECLARE_WORD(p[4]);	/* priority */
     })


/* These definitions must be here, since they are used (also or
   exclusively) internally by the Shades recovery and gc algorithm. */

CELL(generation_pinfo,		/* Persistent generation info. */
     4 + 2 * (p0 & 0xFFF),	/* The 12 lowest bits in p0 tell how
				   many pages the generation contains.
				   (Bits 12-23 tell the number of
				   parent generations.) */
     {
       unsigned long i;
       DECLARE_PTR(p[1]);	/* Previous generation in the list. */
       DECLARE_WORD(p[2]);	/* Generation number. */
       DECLARE_WORD(p[3]);	/* Number of referring ptrs. */
       for (i = 0; i < (p0 & 0xFFF); i++) {
	 DECLARE_WORD(p[4 + 2*i]); /* Page number. */
	 DECLARE_WORD(p[4 + 2*i + 1]); /* Disk page number. */
       }
       assert((p0 & 0xFFF) < MAX_GENERATION_SIZE);
     })

/* This declares the forwarding pointer used by the garbage collector.
   The forwarding pointer is not really visible in user programs, but
   this reserves the tag for use in the Shades collector. */
CELL(forward_pointer,
     2,
     {
       abort();
       DECLARE_NONNULL_PTR(p[1]);
     })


CELL(trie1234,
     5,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
       DECLARE_TAGGED(p[3]);
       DECLARE_TAGGED(p[4]);
     })


CELL(trie123,
     4,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
       DECLARE_TAGGED(p[3]);
     })


CELL(trie124,
     4,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
       DECLARE_TAGGED(p[3]);
     })


CELL(trie12,
     3,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
     })


CELL(trie134,
     4,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
       DECLARE_TAGGED(p[3]);
     })


CELL(trie13,
     3,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
     })


CELL(trie14,
     3,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
     })


CELL(trie1,
     2,
     {
       DECLARE_TAGGED(p[1]);
     })


CELL(trie234,
     4,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
       DECLARE_TAGGED(p[3]);
     })


CELL(trie23,
     3,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
     })


CELL(trie24,
     3,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
     })


CELL(trie2,
     2,
     {
       DECLARE_TAGGED(p[1]);
     })


CELL(trie34,
     3,
     {
       DECLARE_TAGGED(p[1]);
       DECLARE_TAGGED(p[2]);
     })


CELL(trie3,
     2,
     {
       DECLARE_TAGGED(p[1]);
     })


CELL(trie4,
     2,
     {
       DECLARE_TAGGED(p[1]);
     })
