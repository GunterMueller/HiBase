%{
/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

#include <stdlib.h>
#include <string.h>
#include "shades.h"
#include "shtring.h"
#include "asm_defs.h"
#include "asm_parse.h"

static int in_bcode = 0;

%}

%x CPPFILE CPPEND INSTRING

%%

(;.*)?\n		{ asm_lineno++; return NL; }
[ \t]			{ ; /* Skip whitespaces. */ }

\#[ ][0-9]+[ ]\"	{ /* cpp generated line comments. XXX This assumes
			   * only the user file has errors... 
			   */
			  asm_lineno = atoi(yytext + 2);
			  BEGIN(CPPFILE); }
<CPPFILE>[^\"]*		{ asm_filename = strdup(yytext);
			  BEGIN(CPPEND); }
<CPPEND>\".*\n          { BEGIN(0); }

\"			{ asm_string_start(); BEGIN(INSTRING); }
<INSTRING>\"		{ asmlval.i = asm_string_end();
                          BEGIN(0);
                          return STRING; }
<INSTRING>\\t		{ asm_string_char('\t'); }
<INSTRING>\\n		{ asm_string_char('\n'); }
<INSTRING>\\\"		{ asm_string_char('\"'); }
<INSTRING>.		{ asm_string_char(*yytext); }

-?(0|[1-9][0-9]*)	{ asmlval.i = atoi(asmtext); return INT; }
\.stack			{ return DOT_STACK; }
\.accu			{ return DOT_ACCU; }
WORD			{ asmlval.t = WORD; return TYPE; }
PTR			{ asmlval.t = PTR; return TYPE; }
NONNULL_PTR		{ asmlval.t = NONNULL_PTR; return TYPE; }
TAGGED			{ asmlval.t = TAGGED; return TYPE; }
VOID			{ asmlval.t = VOID; return TYPE; }
\.noescape		{ return DOT_NOESCAPE; }
\.escape		{ return DOT_ESCAPE; }
\.entry			{ return DOT_ENTRY; }
\.end			{ in_bcode = 0; return DOT_END; }
_[a-zA-Z_][a-zA-Z0-9_#!?%]*	{
                  shtring_intern_result_t sir;
		  unsigned long sl = strlen(asmtext + 1);
		  ptr_t s;

		  if (!in_bcode)
		    asm_set_name(asmtext + 1);
		  in_bcode = 1;
		  /* Intern the string, return its interned id. */
		  while (!can_allocate(shtring_create_max_allocation(sl)
				       + SHTRING_INTERN_MAX_ALLOCATION)) {
		    asm_resolve_ptrs();
		    flush_batch();
		  }
		  if (shtring_create(&s, asmtext + 1, sl) != 1) {
		    fprintf(stderr, "asm_lex: can_allocate() didn't suffice.\n");
		    exit(2);
		  }
		  sir = shtring_intern(GET_ROOT_PTR(interned_shtrings), s);
		  SET_ROOT_PTR(interned_shtrings, sir.new_intern_root);
		  asmlval.i = sir.interned_shtring_id;
                  return BCODE;
		}
L[0-9]+			{
			  asmlval.s = asm_strdup(asmtext);
			  return LABEL;
			}
R[a-zA-Z0-9_]+		{
			  asmlval.i = (int) asm_lookup_root(asmtext+1);
			  return INT;
			}
[a-z][a-z0-9_]*		{
			  asmlval.insn = asm_lookup_insn(asmtext);
			  return MNEMONIC;
			}
.			{ return *asmtext; }

%%

int asmwrap(void)
{
  return 1;
}
