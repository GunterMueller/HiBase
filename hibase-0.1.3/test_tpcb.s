;;; This file is part of the Shades main memory database system.
;;; 
;;; Copyright (c) 1996 Nokia Telecommunications
;;; All Rights Reserved.
;;;
;;; Authors: Kenneth Oksanen <cessu@iki.fi>
;;;	     Kengatharan Sivalingam <siva@iki.fi>
;;;
	
;;; Byte code assembler source for TPC-B benchmark.
;;; Run with e.g. 'test_asm test_tpcb.s tpcb 10'.

#define NUMBER_OF_TRANSACTIONS		1000000
#define B_BAL_IDX			1
#define T_BAL_IDX			2
#define A_BAL_IDX			2
#define DELTA				1		; XXX 

;;; Create tpcb database.
_create_branch#1:
	.stack		WORD		5
	.accu		WORD
	.entry
	.noescape			; tps, ctr
	push				; tps ctr, ctr
	pick		0		; tps ctr, tps
	push				; tps ctr tps, tps
	pick		1		; tps ctr tps, ctr
	ble		L1		; tps ctr, ctr
	get_root_ptr	Rtest_branch	; tps ctr, branch
	push				; tps ctr branch, branch
	pick		1		; tps ctr branch, ctr
	push				; tps ctr branch ctr, ctr
	push				; tps ctr branch ctr ctr, ctr
	load_imm	0		; tps ctr branch ctr ctr, 0
	make_word_vector 2		; tps ctr branch ctr, wv
	trie_insert			; tps ctr, branch´
	set_root_ptr	Rtest_branch	; tps ctr, branch´
	load_imm	1		; tps ctr, 1
	add				; tps, ctr + 1
	goto_self
L1:
	return
	.end

_create_teller#1:
	.accu		WORD
	.stack		WORD		6
	.entry
	.noescape			; tps*10, ctr
	push				; tps*10 ctr, ctr
	pick		0		; tps*10 ctr, tps*10
	push				; tps*10 ctr tps*10, tps*10
	pick		1		; tps*10 ctr tps*10, ctr
	ble		L2		; tps*10 ctr, ctr
	get_root_ptr	Rtest_teller	; tps*10 ctr, teller
	push				; tps*10 ctr teller, teller
	pick		1		; tps*10 ctr teller, ctr
	push				; tps*10 ctr teller ctr, ctr
	push				; tps*10 ctr teller ctr ctr, ctr
	load_imm	0		; tps*10 ctr teller ctr ctr, 0
	push				; tps*10 ctr teller ctr ctr 0, 0
	make_word_vector 3		; tps*10 ctr teller ctr, wv
  	trie_insert			; tps*10 ctr, teller´
 	set_root_ptr	Rtest_teller	; tps*10 ctr, teller´
	load_imm	1		; tps*10 ctr, 1
	add				; tps*10, ctr + 1
	goto_self
L2:
 	return
	.end

_create_account#1:
	.accu		WORD
	.stack		WORD		6
	.entry
	.noescape
	push
	pick		0
	push
	pick		1
	ble		L3
	get_root_ptr	Rtest_account
	push
	pick		1
	push
	push
	load_imm	0
	push				
	make_word_vector 3
    	trie_insert		
	set_root_ptr	Rtest_account
	load_imm	1
	add
	goto_self
L3:
	return
	.end

_create_tables#3:	
	.accu		VOID
	.stack		WORD		2
	.noescape
	pick		0
	mul_imm		100000
	push
	load_imm	0
	set_root_ptr	Rtest_account
	tail_call_global _create_account#1, 2
	.end

_create_tables#2:	
	.accu		VOID
	.stack		WORD		2
	.noescape			; tps,  
	pick		0		; tps, tps 
	mul_imm		10		; tps, tps*10 
	push				; tps tps*10, tps*10
	load_imm	0		; tps tps*10, 0 
	set_root_ptr	Rtest_teller	; tps tps*10, teller
	call_global	_create_teller#1, 2, _create_tables#3
	.end

_create_tables#1:	
	.accu		VOID
	.stack		WORD		2
	.entry
	.noescape			; tps, 
	pick		0		; tps, tps
	push				; tps tps, tps 
	load_imm	0		; tps tps, 0 
	set_root_ptr	Rtest_branch	; tps tps, branch
	call_global	_create_branch#1, 2, _create_tables#2
	.end
	
_create_tpcb#2:
	.accu		VOID
	.stack		WORD		2
	.noescape
; 	pick		0
; 	print_int
; 	get_root_ptr	Rtest_branch
;   	print_cell		
	pop
	return
	.end

_create_tpcb#1:
	.accu		WORD
	.stack				2
	.entry
	.noescape			; , tps
	push				; tps, tps 
	push				; tps tps, tps
	call_global	_create_tables#1, 2, _create_tpcb#2
	.end

;;; Run tpcb.
_run_tpcb#2:
	.accu		WORD
	.stack		WORD WORD	8
	.entry
	.noescape			; tps trans, ctr
	push				; tps trans ctr, ctr
	push				; tps trans ctr ctr, ctr
	pick		1		; tps trans ctr ctr, trans
	bge		L4		; tps trans ctr, trans
;;; Update Branch.
	get_root_ptr	Rtest_branch	; tps trans ctr, branch
	push				; tps trans ctr branch, branch
	pick		0		; tps trans ctr branch, tps
  	random_number			; tps trans ctr branch, bid
	push				; tps trans ctr branch bid, bid
	pick		3		; tps trans ctr branch bid, branch
	push				; tps trans ctr branch bid branch, br.
	pick		4		; tps trans ctr branch bid branch, bid
	trie_find			; tps trans ctr branch bid, branch´
	push				; tps trans ctr branch bid branch´, br.´
	load_imm	0		; tps trans ctr branch bid branch´, 0
	push				; tps trans ctr branch bid branch´ 0, 0
	make_word_vector 2		; tps trans ctr branch bid branch´, wv
	copy_word_vector 2		; tps trans ctr branch bid, wv
	push				; tps trans ctr branch bid wv, wv
	push				; tps trans ctr branch bid wv wv, wv
	load_imm	B_BAL_IDX	; tps trans ctr branch bid wv wv, IDX
	get_field_value			; tps trans ctr branch bid wv, fv
	add_imm		DELTA		; tps trans ctr branch bid wv, fv´
	push				; tps trans ctr branch bid wv fv´, fv´
	load_imm	B_BAL_IDX	; tps trans ctr branch bid wv fv´, IDX
	set_field_value			; tps trans ctr branch bid, wv
	trie_insert			; tps trans ctr, branch´´
	set_root_ptr	Rtest_branch	; tps trans ctr, branch´´
;;; Update Teller
	get_root_ptr	Rtest_teller	; tps trans ctr, teller
	push				; tps trans ctr teller, teller
	pick		0		; tps trans ctr teller, tps
	mul_imm		10		; tps trans ctr teller, tps*10
  	random_number			; tps trans ctr teller, tid
	push				; tps trans ctr teller tid, tid
	pick		3		; tps trans ctr teller tid, teller
	push				; tps trans ctr teller tid teller, br.
	pick		4		; tps trans ctr teller tid teller, tid
	trie_find			; tps trans ctr teller tid, teller´
	push				; tps trans ctr teller tid teller´, br.´
	load_imm	0		; tps trans ctr teller tid teller´, 0
	push				; tps trans ctr teller tid teller´ 0, 0
	push				; tps trans ctr teller tid teller´ 0 0,0
	make_word_vector 3		; tps trans ctr teller tid teller´, wv
	copy_word_vector 3		; tps trans ctr teller tid, wv
	push				; tps trans ctr teller tid wv, wv
	push				; tps trans ctr teller tid wv wv, wv
	load_imm	T_BAL_IDX	; tps trans ctr teller tid wv wv, IDX
	get_field_value			; tps trans ctr teller tid wv, fv
	add_imm		DELTA		; tps trans ctr teller tid wv, fv´
	push				; tps trans ctr teller tid wv fv´, fv´
	load_imm	T_BAL_IDX	; tps trans ctr teller tid wv fv´, IDX
	set_field_value			; tps trans ctr teller tid, wv
	trie_insert			; tps trans ctr, teller´´
	set_root_ptr	Rtest_teller	; tps trans ctr, teller´´
;;; Update Account
	get_root_ptr	Rtest_account	
	push				
	pick		0		
	mul_imm		100000		
  	random_number			
	push				
	pick		3		
	push				
	pick		4		
	trie_find			
	push				
	load_imm	0		
	push				
	push				
	make_word_vector 3		
	copy_word_vector 3		
	push				
	push				
	load_imm	A_BAL_IDX		
	get_field_value			
	add_imm		DELTA		
	push				
	load_imm	A_BAL_IDX		
	set_field_value			
	trie_insert			
	set_root_ptr	Rtest_account	
;;; Update counter.
	load_imm	1		; tps trans ctr, 1
	add				; tps trans, ctr + 1
	goto_self
L4:
	return
	.end

_run_tpcb#3:	
	.accu		VOID
	.stack		WORD		2
	.noescape			; tps, -- Number of records in Branch
  	load_imm	2		; tps, 2 -- Record size of Branch
 	push				; tps 2, 2
 	get_root_ptr	Rtest_branch	; tps 2, branch
    	print_table			; , branch -- Print updated Branch
	return
	.end
	
_run_tpcb#1:
	.accu		WORD
	.stack				2
	.noescape
	.entry
	push				; tps, tps
	load_imm	NUMBER_OF_TRANSACTIONS
	push				; tps trans, trans
	load_imm	0		; tps trans, ctr
  	init_random			; tps trans, ctr
	call_global	_run_tpcb#2, 3, _run_tpcb#3
	.end

_tpcb#3:
	.accu		WORD
	.stack				0
	.noescape
	die
	.end

_tpcb#2:
	.accu		WORD
	.stack				0
	.noescape		
	call_global	_run_tpcb#1, 1, _tpcb#3
	.end

_tpcb:
	.accu		WORD
	.stack				0
	.noescape
	.entry
	call_global	_create_tpcb#1, 1, _tpcb#2
	.end
