/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Internal defitions of the byte code assembler.
 */


#ifndef INCL_ASM_DEFS
#define INCL_ASM_DEFS 1


#include "includes.h"
#include "interp.h"
#include "obstack.h"
#include "root.h"


typedef struct asm_insn_t {
  struct asm_insn_t *next;
  int word_position;
  enum { ASM_LABEL, ASM_BRANCH, ASM_BRANCH_ARG, ASM_INSN, ASM_NOP } type;
  union {
    char *label;
    struct {
      insn_t insn;
      char *label;
    } branch;
    struct {
      insn_t insn;
      long arg;
      char *label;
    } branch_arg;
    struct {
      int number_of_words;
      word_t word[1];
    } insn;
  } u;
} asm_insn_t;


extern int asm_lineno;
extern char *asm_filename;
void asm_set_name(char *name);
void asm_set_escaping(int escapes);
void asm_set_bcode_is_entry(void);
void asm_set_accu_type(word_type_t);
void asm_set_max_stack_depth(int);
void asm_push_stack_item_type(word_type_t);
char *asm_strdup(char *s);
insn_t asm_lookup_insn(char *s);
root_ix_t asm_lookup_root(char *s);

void asm_string_start(void);
void asm_string_char(char);
word_t asm_string_end(void);

void asm_make_branch(insn_t insn, char *label);
void asm_make_branch_arg(insn_t insn, long arg, char *label);
void asm_make_label(char *label);
void asm_make_insn(insn_t insn);
void asm_append_arg(int i);

void asm_finish_bcode(void);
void asm_resolve_ptrs(void);

#endif /* INCL_ASM_DEFS */
