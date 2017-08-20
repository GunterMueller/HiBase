/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996-1998 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Byte code interpreter
 */

#include <math.h>

#include "includes.h"
#include "interp.h"
#include "root.h"
#include "shades.h"
#include "queue.h"
#include "bitops.h"
#include "shtring.h"
#include "list.h"
#include "net.h"
#include "triev2.h"

static char *rev_id = "$Id: interp.c,v 1.59 1998/03/30 18:43:52 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


#define CONT_BCODE(c)  WORD_TO_PTR((c)[1])
#define CONT_NEXT(c)  WORD_TO_PTR((c)[2])

#define CONT_IS_REUSABLE(c)  BCODE_IS_REUSABLE(CONT_BCODE(c))
#define CONT_MAX_ALLOCATION(c)  BCODE_MAX_ALLOCATION(CONT_BCODE(c))
#define CONT_ACCU_TYPE(c)  BCODE_ACCU_TYPE(CONT_BCODE(c))

#define CSW_MAX_ALLOCATION  (2*QUEUE_MAX_ALLOCATION + CONTEXT_MAX_ALLOCATION)

static int must_show_bcode_ids;


/* A two-way associative byte code cache to speed up lookups of
   globally defined values.  Indexed with interned string id's of the
   name of the global value being evaluated. */

typedef enum { 
  FIRST_LEAST_RECENTLY_USED, 
  SECOND_LEAST_RECENTLY_USED 
} lru_t;

#define GLOBAL_CACHE_LINES  128

#define ID_TO_GLOBAL_CACHE_LINE(id)                      \
  ((id) & ((1 << (ILOG2(GLOBAL_CACHE_LINES) - 1)) - 1))

static struct {
  lru_t lru;
  struct {
    word_t id;			/* 0 can be used as an invalid id. */
    word_t value;
  } first_entry, second_entry;
} global_cache[GLOBAL_CACHE_LINES];


static void flush_global_cache(void)
{
  int i;

  for (i = 0; i < GLOBAL_CACHE_LINES; i++)
    global_cache[i].first_entry.id = global_cache[i].second_entry.id = ~0UL;
}


static inline word_t global_get(word_t id)
{
  int i = ID_TO_GLOBAL_CACHE_LINE(id);
  word_t value;
  
  if (global_cache[i].first_entry.id == id) {
    global_cache[i].lru = FIRST_LEAST_RECENTLY_USED;
    return global_cache[i].first_entry.value;
  }
  if (global_cache[i].second_entry.id == id) {
    global_cache[i].lru = SECOND_LEAST_RECENTLY_USED;
    return global_cache[i].second_entry.value;
  }
  value = triev2_find(GET_ROOT_PTR(globals), id, 32);
  if (global_cache[i].lru == FIRST_LEAST_RECENTLY_USED) {
    global_cache[i].second_entry.id = id;
    global_cache[i].second_entry.value = value;
    global_cache[i].lru = SECOND_LEAST_RECENTLY_USED;
  } else {
    global_cache[i].first_entry.id = id;
    global_cache[i].first_entry.value = value;
    global_cache[i].lru = FIRST_LEAST_RECENTLY_USED;
  }
  return value;
}

static inline void global_set(word_t value, word_t id)
{
  int i = ID_TO_GLOBAL_CACHE_LINE(id);

  if (global_cache[i].first_entry.id == id) {
    global_cache[i].first_entry.value = value;
  } else if (global_cache[i].second_entry.id == id) {
    global_cache[i].second_entry.value = value;
  }
  
  SET_ROOT_PTR(globals,
	       triev2_insert(GET_ROOT_PTR(globals), id, 32, 
			     NULL, NULL, value));
}


/* A two-way associative byte code cache to speed up byte code
   lookups. */

#define BCODE_CACHE_LINES  16

#define BCODE_ID_TO_CACHE_LINE(id)                      \
  ((id) & ((1 << (ILOG2(BCODE_CACHE_LINES) - 1)) - 1))

static struct {
  lru_t lru;
  struct {
    word_t bcode_id;		/* 0 can be used as an invalid byte code id. */
    ptr_t bcode_ptr;
  } first_entry, second_entry;
} bcode_cache[BCODE_CACHE_LINES];


static void flush_bcode_cache(void)
{
  int i;

  for (i = 0; i < BCODE_CACHE_LINES; i++)
    bcode_cache[i].first_entry.bcode_id =
      bcode_cache[i].second_entry.bcode_id = ~0UL;
}


static inline ptr_t bcode_get(word_t id)
{
  int i = BCODE_ID_TO_CACHE_LINE(id);
  ptr_t b;
  
  if (bcode_cache[i].first_entry.bcode_id == id) {
    bcode_cache[i].lru = FIRST_LEAST_RECENTLY_USED;
    return bcode_cache[i].first_entry.bcode_ptr;
  }
  if (bcode_cache[i].second_entry.bcode_id == id) {
    bcode_cache[i].lru = SECOND_LEAST_RECENTLY_USED;
    return bcode_cache[i].second_entry.bcode_ptr;
  }
  b = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(bcodes), id, 32));
  if (bcode_cache[i].lru == FIRST_LEAST_RECENTLY_USED) {
    bcode_cache[i].second_entry.bcode_id = id;
    bcode_cache[i].second_entry.bcode_ptr = b;
    bcode_cache[i].lru = SECOND_LEAST_RECENTLY_USED;
  } else {
    bcode_cache[i].first_entry.bcode_id = id;
    bcode_cache[i].first_entry.bcode_ptr = b;
    bcode_cache[i].lru = FIRST_LEAST_RECENTLY_USED;
  }
  return b;
}


/* Given the insns of the bcode and its stack description at entry,
   fill in the missing gaps (such as allocation limits etc.), and
   construct the bcode sequence.  Returns 1 if impossible due to first
   generation exhaustion (try again), or 2 because the size of the
   continuation frame would exceed the limit given in `bcode.h'. */

int load_bcode(char *name,
	       word_type_t accu_type, int cont_is_reusable, int bcode_is_entry,
	       word_type_t *stack_types, 
	       unsigned long cur_stack_depth, unsigned long max_stack_depth,
	       ptr_t raw_bcode, unsigned long length_of_bcode,
	       int *bcode_id_ptr /* OUT */)
{
  ptr_t bcode, pc, shtring;
  unsigned long max_allocation_ctr = 0, i, name_length = strlen(name);
  word_t bcode_id;
  shtring_intern_result_t shtring_intern_result;
  int n_words;
  ptr_t cont;

  /* Allocate the bcode. */
  n_words = 7 + cur_stack_depth + length_of_bcode + TRIEV2_MAX_ALLOCATION
    + shtring_create_max_allocation(name_length)
    + SHTRING_INTERN_MAX_ALLOCATION;
  if (bcode_is_entry)
    n_words += 3 + max_stack_depth + TRIEV2_MAX_ALLOCATION;
  if (!can_allocate(n_words))
    return 1;
  bcode = allocate(7 + cur_stack_depth + length_of_bcode, CELL_bcode);

  /* Fill in the data slots in the bcode. */
  assert(accu_type < NUMBER_OF_WORD_TYPES);
  BCODE_ACCU_TYPE(bcode) = accu_type;
  assert(cur_stack_depth <= max_stack_depth);
  BCODE_CUR_STACK_DEPTH(bcode) = cur_stack_depth;
  BCODE_NUMBER_OF_WORDS_IN_CODE(bcode) = length_of_bcode;
  BCODE_CONT_IS_REUSABLE(bcode) = cont_is_reusable;
  if (3 + max_stack_depth > MAX_NUMBER_OF_WORDS_IN_CONT)
    return 2;
  BCODE_NUMBER_OF_WORDS_IN_CONT(bcode) = 3 + max_stack_depth;
  /* Since there are no cycles in the bcode, simply sum the
     `max_allocation' of all insns up, to get a safe majorant of the
     true allocation. */
  pc = raw_bcode;
  while (pc < raw_bcode + length_of_bcode)
    switch (*pc) {
#define INSN(mnemonic, insn_size, imm_decl_block, insn_max_alloc, code_block) \
    case INSN_PUSH_AND_ ## mnemonic:					      \
    case INSN_ ## mnemonic:						      \
      max_allocation_ctr += (insn_max_alloc);				      \
      pc += (insn_size);						      \
      break;
#include "insn-def.h"
#undef INSN
    default:
      assert(0);
      break;
    }
  BCODE_MAX_ALLOCATION(bcode) = max_allocation_ctr + CSW_MAX_ALLOCATION;

  /* Resolve interned string id's to actual pointers to the strings. */
  pc = raw_bcode;
  while (pc < raw_bcode + length_of_bcode)
    if (*pc == INSN_PUSH_AND_load_imm_string
	|| *pc == INSN_load_imm_string) {
      pc[1] = PTR_TO_WORD(
        shtring_lookup_by_intern_id(GET_ROOT_PTR(interned_shtrings), pc[1]));
      assert(pc[1] != NULL_WORD);
      pc += 2;
    } else
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

  /* Copy in the insns and stack description of the bcode. */
  for (i = 0; i < length_of_bcode; i++)
    bcode[7 + i] = raw_bcode[i];
  for (i = 0; i < cur_stack_depth; i++)
    bcode[7 + length_of_bcode + i] = stack_types[i];

  /* Insert the bcode into the table of known bcodes. */
  if (shtring_create(&shtring, name, name_length) != 1) {
    fprintf(stderr, "load_bcode: can_allocate() didn't suffice.\n");
    exit(2);
  }
  shtring_intern_result =
    shtring_intern(GET_ROOT_PTR(interned_shtrings), shtring);
  SET_ROOT_PTR(interned_shtrings, shtring_intern_result.new_intern_root);
  *bcode_id_ptr = bcode_id = shtring_intern_result.interned_shtring_id;
  if (be_verbose || must_show_bcode_ids)
    fprintf(stderr, "bcode name \"%s\" has been assigned bcode id %ld\n",
	    name, (long) bcode_id);
  flush_bcode_cache();
  SET_ROOT_PTR(bcodes,
	       triev2_insert(GET_ROOT_PTR(bcodes), bcode_id, 32, 
			     NULL, NULL, PTR_TO_WORD(bcode)));
  bcode[0] |= bcode_id;		/* For debugging purposes. */

  /* If the bcode is an entry point to a function, create a cont
     prototype for it, without any arguments sent to it. */
  if (bcode_is_entry) {
    assert(can_allocate(3 + max_stack_depth));
    cont = allocate(3 + max_stack_depth, CELL_cont);
    cont[0] |= (3 + max_stack_depth) << 12;
    cont[1] = PTR_TO_WORD(bcode);
    cont[2] = NULL_WORD;
    flush_global_cache();
    SET_ROOT_PTR(globals,
		 triev2_insert(GET_ROOT_PTR(globals), bcode_id, 32, 
			       NULL, NULL, PTR_TO_WORD(cont)));
  }

  return 0;
}


static int backtrace(ptr_t cont)
{
  ptr_t bcode, name;
  word_t id;

  fprintf(stderr, "Backtrace:\n");
  while (cont != NULL_PTR) {
    if (CELL_TYPE(cont) != CELL_cont) {
      fprintf(stderr, "\tNot a cont.\n");
      abort();
    }
    bcode = CONT_BCODE(cont);
    if (CELL_TYPE(bcode) != CELL_bcode) {
      fprintf(stderr, "\tNot a bcode.\n");
      abort();
    }
    id = bcode[0] & ~CELL_TYPE_MASK;
    fprintf(stderr, "\tid = %d, ", id);
    name = shtring_lookup_by_intern_id(GET_ROOT_PTR(interned_shtrings), id);
    if (name == NULL_PTR)
      fprintf(stderr, "unknown bcode sequence!\n");
    else {
      shtring_fprint(stderr, name);
      fprintf(stderr, "\n");
    }
    cont = CONT_NEXT(cont);
  }
  return 0;
}


#ifdef ENABLE_BCPROF
word_t number_of_insns_executed = 0;
word_t number_of_sequences_executed = 0;
#endif


word_t interp(ptr_t cont, word_t accu, unsigned long priority)
{
  ptr_t pc, sp, bcode, context;
  ptr_t program_start, stack_start;
  int jiffies_until_yield;
  word_t thread_id = 0;
  net_return_t net_return;
  int number_of_wakeups_left, flushed_batch_during_wakeups;
  struct timeval timeout;
  int is_idle = 0;

#if defined(__GNUC__) && defined(NDEBUG)
  /* Use a faster byte code insn dispatching method if gcc is
     available. */
  static void *jump_table[] = {
#define INSN(mnemonic, insn_size, imm_decl_block, insn_max_alloc, code_block) \
    &&LABEL_PUSH_AND_ ## mnemonic, &&LABEL_ ## mnemonic, 
#include "insn-def.h"
#undef INSN
  };
#endif /* __GNUC__ */

  /* Bcode cache size must be a power of 2. */
  assert((BCODE_CACHE_LINES & (BCODE_CACHE_LINES - 1)) == 0);

  flush_bcode_cache();
  flush_global_cache();

  /* "Jiffy" is a unit of work; in our case the execution of a given
     bytecode sequence (remember they don't contain jumps backwards,
     so no bytecode sequence is executed for indefinitely long).
     Jumping into a byte code sequence is done with any of the below
     macros:
       o `SWITCH_CONT' makes a context switch.
       o `DIE_CONT' forgets the current context and switches to another.
       o `RUN_CONT' to pick a new continuation.  Makes a context
         switch every `jiffies_between_yield' jiffy.
       o `FLUSH_AND_RETRY_CONT' can be called by instructions that
         need to `allocate' more than `can_allocate' allows.  It
         flushes the current commit batch (and hopefully clearing
         sufficiently free memory in the process), and jumps back to
         the first insn of the byte code sequence.
       o `BLOCK_CONT' blocks the continuation and schedules to another.
     Note that `FLUSH_AND_RETRY_CONT' and `BLOCK_CONT' can be called
     safely only from gc-safe points, i.e. from the first byte code
     insn in a byte code sequence. */
  jiffies_until_yield = jiffies_between_yields;
#define SWITCH_CONT					\
  do {							\
    jiffies_until_yield = jiffies_between_yields;	\
    goto yield_cont;					\
  } while (1)
#define DIE_CONT					\
  do {							\
    goto die_cont;					\
  } while (1)
#define RUN_CONT					\
  do {							\
    if (jiffies_until_yield <= 0) {			\
      jiffies_until_yield = jiffies_between_yields;	\
      goto yield_cont;					\
    } else {						\
      jiffies_until_yield--;				\
      goto run_cont;					\
    }							\
  } while (1)
#define FLUSH_AND_RETRY_CONT			\
  do {						\
    jiffies_until_yield--;			\
    goto flush_and_run_cont;			\
  } while (1)
#define BLOCK_CONT					\
  do {							\
    goto block_cont;					\
  } while (1)

  if (cont == NULL_PTR)
    goto recover_cont;

  assert(priority < NUMBER_OF_CONTEXT_PRIORITIES);

run_cont:
  bcode = CONT_BCODE(cont);
  assert(CELL_TYPE(bcode) == CELL_bcode);
  if (!is_in_first_generation(cont)
      || BCODE_CONT_IS_REUSABLE(bcode)
      || !can_allocate(BCODE_MAX_ALLOCATION(bcode))) {
    while (!can_allocate(BCODE_NUMBER_OF_WORDS_IN_CONT(bcode)
			 + BCODE_MAX_ALLOCATION(bcode))) {
    flush_and_run_cont:
      /* Protect the virtual machine registers `cont' and `accu' from
	 gc by copying them to the root block. */
      SET_ROOT_PTR(suspended_cont, cont);
      SET_ROOT_WORD(suspended_accu_type, BCODE_ACCU_TYPE(bcode));
      SET_ROOT_WORD(suspended_accu, accu);
      SET_ROOT_WORD(suspended_thread_id, thread_id);
      SET_ROOT_WORD(suspended_priority, priority);
      
      flush_batch();
      flush_bcode_cache();
      flush_global_cache();
      
      /* Copy the virtual machine registers back from the root
	 block. */
    recover_cont:
      cont = GET_ROOT_PTR(suspended_cont);
      if (cont == NULL_PTR)
	DIE_CONT;
      /* Drop reference to `cont' so that it may be collected away. */
      SET_ROOT_PTR(suspended_cont, NULL_PTR);
      bcode = CONT_BCODE(cont);
      accu = GET_ROOT_WORD(suspended_accu);
      SET_ROOT_WORD(suspended_accu_type, VOID);
      thread_id = GET_ROOT_WORD(suspended_thread_id);
      priority = GET_ROOT_WORD(suspended_priority);
    }
    cont = cell_copy(cont);
  }
  pc = program_start = bcode + 7; /* XXX */
  stack_start = cont + 3;	/* XXX */
  sp = stack_start + bcode[2];	/* XXX */

#ifdef ENABLE_BCPROF
  number_of_sequences_executed++;
#endif

#ifdef INTERP_INSN_TRACE

  fprintf(stderr, "RUNNING thread %d, bcode %d\n", 
	  thread_id, BCODE_ID(bcode));
  while (1) {
#ifdef ENABLE_BCPROF
    number_of_insns_executed++;
#endif
    switch (*pc) {
#define INSN(mnemonic, insn_size, imm_decl_block, insn_max_alloc, code_block) \
    case INSN_PUSH_AND_ ## mnemonic:					      \
      fprintf(stderr, "\tpush\n");					      \
      *sp++ = accu;							      \
    case INSN_ ## mnemonic:						      \
      fprintf(stderr, "\t" #mnemonic "\n");				      \
      code_block;							      \
      break;
#include "insn-def.h"
#undef INSN
    default:
      assert(0);
      break;
    }
  }

#else

#if !defined(__GNUC__) || !defined(NDEBUG)
  while (1) {
#ifdef ENABLE_BCPROF
    number_of_insns_executed++;
#endif
    switch (*pc) {
#define INSN(mnemonic, insn_size, imm_decl_block, insn_max_alloc, code_block) \
    case INSN_PUSH_AND_ ## mnemonic:					      \
      *sp++ = accu;							      \
    case INSN_ ## mnemonic:						      \
      code_block;							      \
      break;
#include "insn-def.h"
#undef INSN
    default:
      assert(0);
      break;
    }
  }

#else
  /* A faster bcode insn dispatcher using gcc's labels as values.
     A.k.a. jumps2 in Rossi, Sivalingam, "A Survey of Instruction
     Dispatch Techniques for Byte-Code Interpreters", in Oksanen (ed.)
     "Seminar on Mobile Code", Technical Report TKO-C79, Faculty of
     Information Technology, Helsinki University of Technology,
     1996. */

#ifdef ENABLE_BCPROF
#define DISPATCH				\
  do {						\
    number_of_insns_executed++;			\
    goto *jump_table[*pc];			\
  } while (0)
#else
#define DISPATCH				\
  do {						\
    goto *jump_table[*pc];			\
  } while (0)
#endif

  DISPATCH;

#define INSN(mnemonic, insn_size, imm_decl_block, insn_max_alloc, code_block) \
  LABEL_PUSH_AND_ ## mnemonic:						      \
    *sp++ = accu;							      \
  LABEL_ ## mnemonic:							      \
    code_block;								      \
    DISPATCH;
#include "insn-def.h"
#undef INSN

#endif
#endif

yield_cont:
  /* Continuation context switching.  It is assumed that the
     allocation required for continuation switching is included in the
     `CONT_MAX_ALLOCATION' of the continuations. */
  /* Put the current continuation in "thread table". */
  assert(can_allocate(CSW_MAX_ALLOCATION));
  context = allocate(CONTEXT_MAX_ALLOCATION, CELL_context);
  context[0] |= BCODE_ACCU_TYPE(bcode);
  context[1] = PTR_TO_WORD(cont);
  context[2] = accu;
  context[3] = thread_id;
  context[4] = priority;
  SET_ROOT_PTR_VECTOR(contexts,
		      priority,
		      queue_insert_last(GET_ROOT_PTR_VECTOR(contexts,
							    priority),
					context));
die_cont:
  /* Wake up blocked threads.  If we `flush_batch' during processing
     wakeups, then we redo this loop since it is quite possible that
     the latency of `flush_batch' allowed more blocked threads to wake
     up and we want to bring them back into the realm of the byte code
     scheduler as soon as possible. */
  timeout.tv_sec = 0;
  if (is_idle)
    timeout.tv_usec = usecs_for_network_select_when_idle;
  else
    timeout.tv_usec = 0;
  is_idle = 0;
  number_of_wakeups_left = net_number_of_wakeups(&timeout);
  flushed_batch_during_wakeups = 0;
  while (number_of_wakeups_left-- > 0) {
    while (!can_allocate(TRIEV2_MAX_ALLOCATION + 2 * QUEUE_MAX_ALLOCATION)) {
      flush_batch();
      flush_bcode_cache();
      flush_global_cache();
      flushed_batch_during_wakeups = 1;
    }
    net_return = net_get_wakeup();
    thread_id = net_return.thread_id;
#ifdef INTERP_INSN_TRACE
    fprintf(stderr, "WAKEUP thread %d\n", thread_id);
#endif
    context = WORD_TO_PTR(triev2_find(GET_ROOT_PTR(blocked_threads),
				      thread_id, 32));
    assert(context != NULL_PTR);
    assert(context[3] == thread_id);
    priority = context[4];
    assert(priority < NUMBER_OF_CONTEXT_PRIORITIES);
    SET_ROOT_PTR_VECTOR(contexts,
			priority,
			queue_insert_last(GET_ROOT_PTR_VECTOR(contexts,
							      priority),
					  context));
    SET_ROOT_PTR(blocked_threads,
		 triev2_delete(GET_ROOT_PTR(blocked_threads),
			       thread_id, 32, NULL, NULL, NULL_WORD));
  }
  if (flushed_batch_during_wakeups)
    goto die_cont;

  /* Pick a thread and start running it. */
  priority = NUMBER_OF_CONTEXT_PRIORITIES;
  do {
    ptr_t queue;

    priority--;
    if (priority == 0)
      /* The lowest priority is seen regarded as corresponding to an
         idle database server and this time is used for very
         low-priority tasks, e.g. preventive garbage collection. */
      is_idle = 1;
    queue = GET_ROOT_PTR_VECTOR(contexts, priority);
    if (!QUEUE_IS_EMPTY(queue)) {
      context = QUEUE_GET_FIRST(queue);
      SET_ROOT_PTR_VECTOR(contexts, priority, queue_remove_first(queue));
      cont = WORD_TO_PTR(context[1]);
      accu = context[2];
      thread_id = context[3];
      assert(context[4] == priority);
      goto run_cont;
    }
  } while (priority != 0);

  /* We come here if we have no runnable threads.  Usually this
     shouldn't happen in the eventual system since we will always have
     idler-threads, if nothing else. */
  if (GET_ROOT_PTR(blocked_threads) != NULL_PTR)
    /* We have blocked threads.  Try to wake them up. */
    goto die_cont;

  /* We come here only if there were no threads at all.  This is
     unlikely to ever happen in the final systems, but during testing
     and development we may well want to return from the interpreter
     with the value of accu. */
  return accu;

  /* Auxiliary pieces of code.  These are located after the core logic
     and outsize of INSNs in order to cut down the executable size (as
     these can be shared functionality by several INSNs). */

block_cont:
  /* The byte code thread we just ran blocked.  Store the variables in
     a context cell and store the context cell in the trie of blocked
     threads using the `thread_id' as the key. */
#ifdef INTERP_INSN_TRACE
  fprintf(stderr, "BLOCK thread %d\n", thread_id);
#endif
  assert(can_allocate(CONTEXT_MAX_ALLOCATION));
  context = allocate(CONTEXT_MAX_ALLOCATION, CELL_context);
  assert(BCODE_ACCU_TYPE(bcode) < NUMBER_OF_WORD_TYPES);
  context[0] |= BCODE_ACCU_TYPE(bcode);
  context[1] = PTR_TO_WORD(cont);
  context[2] = accu;
  context[3] = thread_id;
  context[4] = priority;
  assert(can_allocate(TRIE_MAX_ALLOCATION));
  SET_ROOT_PTR(blocked_threads,
	       triev2_insert(GET_ROOT_PTR(blocked_threads),
			     thread_id, 32, NULL, NULL, 
			     PTR_TO_WORD(context)));
  goto die_cont;

  /*NOTREACHED*/
  assert(0);
}


/* Returns non-zero in case of error. */
int init_interp(int argc, char **argv)
{
  int i;

  must_show_bcode_ids = 0;

  for (i = 1; i < argc; i++) {
    if (argv[i] == NULL)
      continue;
    
    if (strcmp("--show-bcode-ids", argv[i]) == 0) {
      must_show_bcode_ids = 1;
      argv[i] = NULL;
    }
  }

  flush_bcode_cache();
  flush_global_cache();

  net_init();

  return 0;
}
