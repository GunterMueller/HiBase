%{
/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */


#include <stdio.h>
#include "asm_defs.h"

int asmlex(void);

static int asmerror(char *msg)
{
  fprintf(stderr, "%s:%d: %s\n", asm_filename, asm_lineno, msg);
  exit(1);
  return 0;
}

%}


%start program

%union {
  char *s;
  long i;
  word_type_t t;
  insn_t insn;
}

%token <s> LABEL
%token <i> INT BCODE STRING
%token <t> TYPE
%token <insn> MNEMONIC
%token DOT_STACK DOT_ACCU DOT_END DOT_ESCAPE DOT_NOESCAPE DOT_ENTRY
%token CPP_LINE NL

%%

program:  nls routines
	| routines
	;

routines: routines routine
	| routine
	;

routine:  BCODE ':' nls preludes insns DOT_END { asm_finish_bcode(); } nls
	;

preludes: preludes prelude nls
	|
	;

prelude:  DOT_STACK stacks INT 		{ asm_set_max_stack_depth($3); }
	| DOT_ACCU TYPE			{ asm_set_accu_type($2); }
	| DOT_NOESCAPE			{ asm_set_escaping(0); }
	| DOT_ESCAPE			{ asm_set_escaping(1); }
	| DOT_ENTRY			{ asm_set_bcode_is_entry(); }
	;

stacks:   stacks TYPE			{ asm_push_stack_item_type($2); }
	|
	;

insns:	  insns insn nls
	|
	;

insn:	  LABEL	':'			{ asm_make_label($1); }
	| MNEMONIC LABEL		{ asm_make_branch($1, $2); }
	| MNEMONIC INT LABEL		{ asm_make_branch_arg($1, $2, $3); }
	| MNEMONIC pargs		{ asm_make_insn($1); }
	;

pargs:    args
	|
	;

args:	  args ',' INT			{ asm_append_arg($3); }
	| args ',' BCODE		{ asm_append_arg($3); }
	| args ',' STRING		{ asm_append_arg($3); }
	| INT				{ asm_append_arg($1); }
	| BCODE				{ asm_append_arg($1); }
	| STRING			{ asm_append_arg($1); }
	;

nls:	  NL nls
	| NL
	;

%%
