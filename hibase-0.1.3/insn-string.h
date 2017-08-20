/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1998 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* String insns.
 */


INSN(string_append,
     1,
     {
     },
     0,
     {
       ptr_t s1 = WORD_TO_PTR(sp[-1]);
       ptr_t s2 = WORD_TO_PTR(accu);

       if (!can_allocate(shtring_cat_max_allocation(s1, s2)))
	 FLUSH_AND_RETRY_CONT;
       accu = PTR_TO_WORD(shtring_cat(s1, s2));
       sp--;
       pc++;
     })

INSN(int_to_string,
     1,
     {
     },
     300,
     {
       char buf[30];
       ptr_t p;
       int len;

       assert(TAGGED_IS_WORD(accu));
       sprintf(buf, "%d", TAGGED_TO_SIGNED_WORD(accu));
       len = strlen(buf);
       if (!can_allocate(shtring_create_max_allocation(len)))
	 FLUSH_AND_RETRY_CONT;
       switch (shtring_create(&p, buf, len)) {
       case -1:
       case 0:
	 assert(0);
	 break;
       case 1:
	 accu = PTR_TO_WORD(p);
	 break;
       }
       pc++;
     })

#if 0
INSN(string_cmp,
     1,
     {
     },
     0,
     {
       ptr_t str1 = WORD_TO_PTR(sp[-1]);
       ptr_t str2 = WORD_TO_PTR(accu);

       if (!can_allocate(shtring_cmp_max_allocation(str1, str2)))
	 FLUSH_AND_RETRY_CONT;
       accu = WORD_TO_TAGGED(shtring_cmp(str1, str2));
       sp--;
       pc++;
     })
#else
INSN(string_cmp,
     1,
     {
     },
     0,
     {
       ptr_t str1 = WORD_TO_PTR(sp[-1]);
       ptr_t str2 = WORD_TO_PTR(accu);

       accu = WORD_TO_TAGGED(shtring_cmp_without_allocating(str1, str2));
       sp--;
       pc++;
     })
#endif

INSN(string_quote,
     1,
     {
     },
     0,
     {
       ptr_t s = WORD_TO_PTR(accu);
       word_t len = shtring_length(s);
       word_t pos;
       char buf[200];
       char *buf_begin;
       char *p;
       int c;
       
       if (!can_allocate(shtring_create_max_allocation(2*len + 2)))
	 FLUSH_AND_RETRY_CONT;
       if (2*len + 2 > 200) {
	 p = buf_begin = malloc(2*len+2);
	 if (p == NULL) {
	   fprintf(stderr, "Out of memory in INSN_string_quote\n");
	   exit(1);
	 }
       } else
	 buf_begin = p = &buf[0];
       *p++ = '"';
       for (pos = 0; pos < len; pos++) {
	 switch (c = shtring_charat(s, pos)) {
	 case '"':
	   *p++ = '\\';
	   *p++ = '"';
	   break;
	 case '\t':
	   *p++ = '\\';
	   *p++ = 't';
	   break;
	 case '\n':
	   *p++ = '\\';
	   *p++ = 'n';
	   break;
	 case '\\':
	   *p++ = '\\';
	   *p++ = '\\';
	   break;
	 default:
	   *p++ = c;
	   break;
	 }
       }
       *p++ = '"';
       switch (shtring_create(&s, buf_begin, p - buf_begin)) {
       case -1:
       case 0:
	 assert(0);
	 break;
       case 1:
	 accu = PTR_TO_WORD(s);
	 break;
       }
       if (buf_begin != &buf[0])
	 free(buf_begin);
       pc++;
     })
