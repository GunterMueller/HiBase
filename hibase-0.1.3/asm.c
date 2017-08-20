/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */


#include "includes.h"
#include "interp.h"
#include "asm.h"
#include "asm_defs.h"
#include "shades.h"
#include "root.h"
#include "shtring.h"


static char *rev_id = "$Id: asm.c,v 1.19 1998/03/28 23:18:38 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


static int errors_have_occurred;


static struct {
  char *mnemonic;
  insn_t insn;
} asm_insn_defs[] = {
#define INSN(mnemonic, insn_size, imm_decl_block, max_allocation, code_block) \
  { # mnemonic, INSN_ ## mnemonic },
#include "insn-def.h"
#undef INSN
  { NULL, NUMBER_OF_INSNS }
};

insn_t asm_lookup_insn(char *s)
{
  int i;
  for (i = 0; asm_insn_defs[i].mnemonic != NULL; i++)
    if (strcmp(asm_insn_defs[i].mnemonic, s) == 0)
      return asm_insn_defs[i].insn;
  fprintf(stderr, "%s:%d: Unknown insn `%s'\n",
	  asm_filename, asm_lineno, s);
  errors_have_occurred = 1;
  return NUMBER_OF_INSNS;
}


static struct {
  char *s;
  root_ix_t root_ix;
} asm_root_defs[] = {
#define ROOT_WORD(name, initial_value)		\
  { # name, ROOT_IX_ ## name },
#define ROOT_PTR(name)				\
  { # name, ROOT_IX_ ## name },
#include "root-def.h"
#undef ROOT_WORD
#undef ROOT_PTR
  { NULL, NUMBER_OF_ROOT_IXS }
};

root_ix_t asm_lookup_root(char *s)
{
  int i;
  for (i = 0; asm_root_defs[i].s != NULL; i++)
    if (strcmp(s, asm_root_defs[i].s) == 0)
      return asm_root_defs[i].root_ix;
  fprintf(stderr, "%s:%d: Unknown root index `%s'\n",
	  asm_filename, asm_lineno, s);
  errors_have_occurred = 1;
  return NUMBER_OF_ROOT_IXS;
}


int asm_lineno;
char *asm_filename = NULL;
static char *asm_name;
static word_type_t asm_accu_type;
static int asm_accu_type_is_given;
static int asm_cont_is_reusable;
static int asm_bcode_is_entry = 0;
static word_type_t asm_stack_types[MAX_NUMBER_OF_WORDS_IN_CONT];
static int asm_cur_stack_depth, asm_max_stack_depth;

void asm_set_accu_type(word_type_t t)
{
  if (asm_accu_type_is_given)
    fprintf(stderr, "%s:%d: Setting accu type a second time in `%s'.\n", 
	    asm_filename, asm_lineno, asm_name);
  asm_accu_type_is_given = 1;
  asm_accu_type = t;
}

void asm_push_stack_item_type(word_type_t t)
{
  if (asm_cur_stack_depth >= MAX_NUMBER_OF_WORDS_IN_CONT) {
    fprintf(stderr, "%s:%d: Stack frame too large in `%s'.\n", 
	    asm_filename, asm_lineno, asm_name);
    exit(1);
  }
  asm_stack_types[asm_cur_stack_depth] = t;
  asm_cur_stack_depth++;
}

void asm_set_max_stack_depth(int max_stack_depth)
{
  if (asm_max_stack_depth >= MAX_NUMBER_OF_WORDS_IN_CONT) {
    fprintf(stderr, "%s:%d: Stack frame too large in `%s'.\n", 
	    asm_filename, asm_lineno, asm_name);
    exit(1);
  }
  if (max_stack_depth < asm_cur_stack_depth) {
    fprintf(stderr, "%s:%d: Declared stack depth %s too small in `%s'.\n", 
	    asm_filename, asm_lineno, max_stack_depth, asm_name);
    exit(1);
  }
  asm_max_stack_depth = max_stack_depth;
}

void asm_set_escaping(int escapes)
{
  asm_cont_is_reusable = escapes;
}

void asm_set_bcode_is_entry(void)
{
  asm_bcode_is_entry = 1;
}


/* An obstack for various data live during a single byte code sequence
   assembly. */

static struct obstack asm_obstack;

/* `obstack_chunk_alloc' and `obstack_chunk_free' are required by
   obstacks. */

static void *obstack_chunk_alloc(size_t sz)
{
  void *p = malloc(sz);
  if (p == NULL) {
    fprintf(stderr, "obstack_chunk_alloc: malloc(%d) failed.\n", sz);
    exit(1);
  }
  return p;
}

static void obstack_chunk_free(void *p)
{
  free(p);
}


/* As strdup, but allocate from the obstack. */

char *asm_strdup(char *s)
{
  char *r = obstack_alloc(&asm_obstack, strlen(s) + 1);
  strcpy(r, s);
  return r;
}


void asm_set_name(char *s)
{
  asm_name = asm_strdup(s);
}


static int string_ctr;

void asm_string_start(void)
{
  string_ctr = 0;
}

void asm_string_char(char c)
{
  obstack_1grow(&asm_obstack, c);
  string_ctr++;
}

word_t asm_string_end(void)
{
  shtring_intern_result_t sir;
  char *s;
  ptr_t sh;

  s = obstack_finish(&asm_obstack);
  /* Intern the string, return its interned id. */
  while (!can_allocate(shtring_create_max_allocation(string_ctr)
		       + SHTRING_INTERN_MAX_ALLOCATION)) {
    asm_resolve_ptrs();
    flush_batch();
  }
  if (shtring_create(&sh, s, string_ctr) != 1) {
    fprintf(stderr, "asm_string_end: can_allocate() didn't suffice.\n");
    exit(2);
  }
  sir = shtring_intern(GET_ROOT_PTR(interned_shtrings), sh);
  SET_ROOT_PTR(interned_shtrings, sir.new_intern_root);
  return sir.interned_shtring_id;
}


/* `asm_insn_t's for a double-ended list so that it is easy to scan,
   modify, and replace insns.  Maintain pointers for both the first
   and last asm insn. */

static asm_insn_t *asm_insn_head, *asm_insn_tail;


static void append_insn(asm_insn_t *insn)
{
  if (asm_insn_head == NULL) {
    asm_insn_head = asm_insn_tail = insn;
    insn->next = NULL;
  } else {
    asm_insn_tail->next = insn;
    insn->next = NULL;
    asm_insn_tail = insn;
  }
}


void asm_make_branch(insn_t insn, char *label)
{
  asm_insn_t *asm_insn = 
    (asm_insn_t *) obstack_alloc(&asm_obstack, sizeof(asm_insn_t));
  asm_insn->type = ASM_BRANCH;
  asm_insn->u.branch.insn = insn;
  asm_insn->u.branch.label = asm_strdup(label);
  append_insn(asm_insn);
}


void asm_make_branch_arg(insn_t insn, long arg, char *label)
{
  asm_insn_t *asm_insn = 
    (asm_insn_t *) obstack_alloc(&asm_obstack, sizeof(asm_insn_t));
  asm_insn->type = ASM_BRANCH_ARG;
  asm_insn->u.branch_arg.insn = insn;
  asm_insn->u.branch_arg.label = asm_strdup(label);
  asm_insn->u.branch_arg.arg = arg;
  append_insn(asm_insn);
}


void asm_make_label(char *label)
{
  asm_insn_t *asm_insn = 
    (asm_insn_t *) obstack_alloc(&asm_obstack, sizeof(asm_insn_t));
  asm_insn->type = ASM_LABEL;
  asm_insn->u.label = label;
  append_insn(asm_insn);
}


static word_t insn_args[200];	/* XXX */
static int number_of_insn_args = 0;

void asm_append_arg(int i)
{
  insn_args[number_of_insn_args++] = i;
}


void asm_make_insn(insn_t insn)
{
  asm_insn_t *asm_insn = (asm_insn_t *)
    obstack_alloc(&asm_obstack,
		  sizeof(asm_insn_t) + number_of_insn_args * sizeof(word_t));
  asm_insn->type = ASM_INSN;
  asm_insn->u.insn.number_of_words = 1 + number_of_insn_args;
  asm_insn->u.insn.word[0] = insn;
  if (number_of_insn_args > 0)
    memcpy(&asm_insn->u.insn.word[1], insn_args,
	   number_of_insn_args * sizeof(word_t));
  switch (asm_insn->u.insn.word[0]) {
#define INSN(mnemonic, insn_size, imm_decl_block, max_allocation, code_block) \
  case INSN_ ## mnemonic:						   \
    if ((insn_size) != number_of_insn_args + 1) {			   \
      fprintf(stderr, "%s:%d: Insn expected %d args, given %d.\n",	   \
	      asm_filename, asm_lineno,					   \
	      (insn_size) - 1, number_of_insn_args);			   \
      errors_have_occurred = 1;						   \
    }									   \
    break;
#include "insn-def.h"
#undef INSN
  default:
    /* No useful default, since an unknown insn should already have
       been reported. */
    assert(0);
    abort();
    break;
  }
  append_insn(asm_insn);
  /* Clear for next insn. */
  number_of_insn_args = 0;
}


static void *asm_obstack_start;


typedef struct bcode_list_t {
  struct bcode_list_t *next;
  int bcode_id;
} bcode_list_t;

/* List of bcode_id's created during this commit group. */
static bcode_list_t *bcode_list = NULL;

static void add_bcode_to_bcode_list(int bcode_id)
{
  bcode_list_t *p = malloc(sizeof(bcode_list_t));
  if (p == NULL) {
    fprintf(stderr, "add_bcode_to_bcode_list: malloc failed.\n");
    exit(1);
  }
  p->next = bcode_list;
  p->bcode_id = bcode_id;
  bcode_list = p;
}

void asm_resolve_ptrs(void)
{
  bcode_list_t *tmp;

  while (bcode_list != NULL) {
    ptr_t p = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(bcodes),
				      bcode_list->bcode_id, 32));
    ptr_t pc = &p[7];
    word_t to_cont, next_bcode;

    assert(is_in_first_generation(p));
    do {
      switch (*pc) {
      case INSN_call_global:
	to_cont = triev2_find(GET_ROOT_PTR(globals), pc[1], 32);
	next_bcode = triev2_find(GET_ROOT_PTR(bcodes), pc[3], 32);
	if (to_cont != NULL_WORD && next_bcode != NULL_WORD) {
	  pc[0] = INSN_call_global_with_to_as_cont_ptr_and_next_bcode_as_ptr;
	  pc[1] = to_cont;
	  pc[3] = next_bcode;
	} else if (to_cont != NULL_WORD) {
	  pc[0] = INSN_call_global_with_to_as_cont_ptr;
	  pc[1] = to_cont;
	} else if (next_bcode != NULL_WORD) {
	  pc[0] = INSN_call_global_with_next_bcode_as_ptr;
	  pc[3] = next_bcode;
	}
	/* Specialized call insns for low arities.  Note that we have
           to double the increase in INSN-codes because of the
           INSN_PUSH_AND...-versions in between. */
	if (pc[2] == 0 || pc[2] == 1)
	  pc[0] += 2;
	else if (pc[2] <= 4)
	  pc[0] += 2 * pc[2];
	break;
      case INSN_PUSH_AND_call_global:
	to_cont = triev2_find(GET_ROOT_PTR(globals), pc[1], 32);
	next_bcode = triev2_find(GET_ROOT_PTR(bcodes), pc[3], 32);
	if (to_cont != NULL_WORD && next_bcode != NULL_WORD) {
	  pc[0] = INSN_PUSH_AND_call_global_with_to_as_cont_ptr_and_next_bcode_as_ptr;
	  pc[1] = to_cont;
	  pc[3] = next_bcode;
	} else if (to_cont != NULL_WORD) {
	  pc[0] = INSN_PUSH_AND_call_global_with_to_as_cont_ptr;
	  pc[1] = to_cont;
	} else if (next_bcode != NULL_WORD) {
	  pc[0] = INSN_PUSH_AND_call_global_with_next_bcode_as_ptr;
	  pc[3] = next_bcode;
	}
	/* Specialized call insns for low arities.  Note that we have
           to double the increase in INSN-codes because of the
           INSN_PUSH_AND...-versions in between. */
	if (pc[2] == 0 || pc[2] == 1)
	  pc[0] += 2;
	else if (pc[2] <= 4)
	  pc[0] += 2 * pc[2];
	break;
      case INSN_tail_call_global:
	to_cont = triev2_find(GET_ROOT_PTR(globals), pc[1], 32);
	if (to_cont != NULL_WORD) {
	  pc[0] = INSN_tail_call_global_with_to_as_cont_ptr;
	  pc[1] = to_cont;
	}
	/* Specialized call insns for low arities.  Note that we have
           to double the increase in INSN-codes because of the
           INSN_PUSH_AND...-versions in between. */
	if (pc[2] == 0 || pc[2] == 1)
	  pc[0] += 2;
	else if (pc[2] <= 4)
	  pc[0] += 2 * pc[2];
	break;
      case INSN_PUSH_AND_tail_call_global:
	to_cont = triev2_find(GET_ROOT_PTR(globals), pc[1], 32);
	if (to_cont != NULL_WORD) {
	  pc[0] = INSN_PUSH_AND_tail_call_global_with_to_as_cont_ptr;
	  pc[1] = to_cont;
	}
	/* Specialized call insns for low arities.  Note that we have
           to double the increase in INSN-codes because of the
           INSN_PUSH_AND...-versions in between. */
	if (pc[2] == 0 || pc[2] == 1)
	  pc[0] += 2;
	else if (pc[2] <= 4)
	  pc[0] += 2 * pc[2];
	break;
      default:
	/* Do nothing. */
	break;
      }
      /* Increment `pc' to the next insns. */
      switch (*pc) {
#define INSN(mnemonic, insn_size, imm_decl_block, insn_max_alloc, code_block) \
      case INSN_PUSH_AND_ ## mnemonic:					      \
      case INSN_ ## mnemonic:						      \
        pc += (insn_size);						      \
        break;
#include "insn-def.h"
#undef INSN
      default:
        assert(0);
        break;
      }
    } while (pc < &p[7 + p[3]]);
    tmp = bcode_list;
    bcode_list = bcode_list->next;
    free(tmp);
  }
}


void asm_finish_bcode(void)
{
  asm_insn_t *p, *q;
  ptr_t raw_bcode, next_ptr, to_cont_ptr;
  int i, length_of_raw_bcode, bcode_id;

  if (errors_have_occurred)
    /* Don't process further. */
    goto cleanups;

  if (!asm_accu_type_is_given) {
    fprintf(stderr, 
	    "%s:%d: accu type not given in cont `%s', defaulting to TAGGED.\n",
	    asm_filename, asm_lineno, asm_name);
    asm_accu_type = TAGGED;
  }

  /* Some very low-level peephole optimizations. */
  for (i = 0, p = asm_insn_head; p != NULL; p = p->next) {
    /* Push-melding merges push-instructions to the next instruction.
       Do this after other peephole optimizations since this doubles
       the number of different insns. */
    if (p->type == ASM_INSN 
	&& p->u.insn.word[0] == INSN_push
	&& p->next != NULL
	&& (p->next->type == ASM_INSN 
	    || p->next->type == ASM_BRANCH
	    || p->next->type == ASM_BRANCH_ARG)) {
      p->type = ASM_NOP;	/* Delete `p' by making it a no-op. */
      p->next->u.insn.word[0]--; /* Push-melded INSNs are encoded to a
				   value smaller by one. */
    }
  }

  /* Construct the raw byte code.  First compute the raw word positions. */
  for (i = 0, p = asm_insn_head; p != NULL; p = p->next) {
    p->word_position = i;
    switch (p->type) {
    case ASM_LABEL:
    case ASM_NOP:
      /* No increase in `i', since this produces no new code. */
      break;
    case ASM_BRANCH:
      i += 2;
      break;
    case ASM_BRANCH_ARG:
      i += 3;
      break;
    case ASM_INSN:
      i += p->u.insn.number_of_words;
      break;
    default:
      assert(0);
    }
  }
  length_of_raw_bcode = i;
  /* Allocate raw bcode, and copy the insns into the raw bcode.
     Simultaneously resolve branch targets. */
  raw_bcode = (ptr_t) obstack_alloc(&asm_obstack,
				    length_of_raw_bcode * sizeof(word_t));
  for (p = asm_insn_head; p != NULL; p = p->next)
    switch (p->type) {
    case ASM_LABEL:
    case ASM_NOP:
      break;
    case ASM_BRANCH:
      raw_bcode[p->word_position] = p->u.branch.insn;
      /* The label has to be ahead, since backward branches are not allowed. */
      for (q = p->next; q != NULL; q = q->next)
	if (q->type == ASM_LABEL
	    && strcmp(p->u.branch.label, q->u.label) == 0) {
	  raw_bcode[p->word_position + 1] = q->word_position;
	  goto found;
	}
      fprintf(stderr, "Unresolved branch target `%s'.\n", p->u.branch.label);
      exit(0);
      found:
      break;
    case ASM_BRANCH_ARG:
      raw_bcode[p->word_position] = p->u.branch_arg.insn;
      raw_bcode[p->word_position + 1] = p->u.branch_arg.arg;
      /* The label has to be ahead, since backward branches are not allowed. */
      for (q = p->next; q != NULL; q = q->next)
	if (q->type == ASM_LABEL
	    && strcmp(p->u.branch_arg.label, q->u.label) == 0) {
	  raw_bcode[p->word_position + 2] = q->word_position;
	  goto found_arg;
	}
      fprintf(stderr, "Unresolved branch target `%s'.\n", 
	      p->u.branch_arg.label);
      exit(0);
      found_arg:
      break;
    case ASM_INSN:
      /* Copy the insn sequence as is. */
      memcpy(&raw_bcode[p->word_position], p->u.insn.word,
	     p->u.insn.number_of_words * sizeof(word_t));
      break;
    default:
      assert(0);
    }
    
  /* Bring the raw bcode into Shades' memory management and the byte
     code interpreter. */
  while (load_bcode(asm_name,
		    asm_accu_type, asm_cont_is_reusable, asm_bcode_is_entry,
		    asm_stack_types, asm_cur_stack_depth, asm_max_stack_depth,
		    raw_bcode, length_of_raw_bcode,
		    &bcode_id)) {
    asm_resolve_ptrs();
    assert(bcode_list == NULL);
    flush_batch();
  }
  add_bcode_to_bcode_list(bcode_id);

  /* Clear up for the next bcode sequence. */
  obstack_free(&asm_obstack, asm_obstack_start);
  asm_obstack_start = obstack_alloc(&asm_obstack, 1);

cleanups:
  asm_accu_type_is_given = 0;
  asm_cont_is_reusable = 1;
  asm_cur_stack_depth = asm_max_stack_depth = 0;
  asm_insn_head = asm_insn_tail = NULL;
  asm_bcode_is_entry = 0;
}


extern FILE *asmin;		/* `yyin' without prefix change. */
extern int asmdebug;
int asmparse(void);

/* Read the given assembler file. */
int asm_file(char *filename)
{
  char buf[200];		/* XXX */

  asm_lineno = 1;
  asm_insn_head = asm_insn_tail = NULL;
  flush_batch();
  obstack_init(&asm_obstack);
  asm_obstack_start = obstack_alloc(&asm_obstack, 1);
  /* `popen' to a cpp so that the asm we read can use cpp's
     facilities.  Note that even though gcc is a wonderful compiler,
     its use as cpp is a idiotically different from most others! */
  if (strncmp(CPP, "gcc", 3) == 0)
    sprintf(buf, CPP " -x c -I. %s", filename);
  else
    sprintf(buf, CPP " -I. %s", filename);
  asmin = popen(buf, "r");
  if (asmin == NULL) {
    fprintf(stderr, "Failed to open or cpp file \"%s\"\n", filename);
    return 1;
  }
  errors_have_occurred = 0;
  /* asmdebug = 1; */
  asmparse();
  pclose(asmin);
  obstack_free(&asm_obstack, asm_obstack_start);
  asm_resolve_ptrs();
  return errors_have_occurred;
}
