;;; This file is part of the Shades main memory database system.
;;; 
;;; Copyright (c) 1996 Nokia Telecommunications
;;; All Rights Reserved.
;;;
;;; Authors: Sirkku-Liisa Paajanen <sirkku@iki.fi>
;;;
	
;;; Byte code assembler source for the algorithm mergesort.
;;; Run with e.g. 'test_asm test_mergesort.s mergesort 50'.

_makelist#2:
	.accu			PTR
	.stack					1
	.noescape
	push
	;; l, l
	load_imm		100
	random_number
	;; rand, l
	make_word_vector	1
	;; new, l
	exch
	;; l, new
	cons
	return
	.end
	
_makelist#1:
	.accu			WORD
	.stack					2
	.noescape
	.entry
	push
	push
	;; n, n n
	load_imm		0
	beq			L2
	;; 0, n
	pop
	;; n,
	sub_imm			1
	;; n-1,
	call_global		_makelist#1, 1, _makelist#2
L2:
	return
	.end	

	
_merge#2:
	.accu			PTR
	.stack			PTR		1
	.noescape
	;; merge, cara/carb
	cons
	;; cons, -
	return
	.end
	
_merge#1:
	.accu			PTR
	.stack			PTR		6
	.noescape
	.entry
	push
	;; a, b a
	swap
	;; a, a b
	push
	;; a, a b a
	load_imm		0
	beq			L5
	;; 0, a b
	pop
	push
	;; b, a b
	swap
	;; b, b a
	push
	;; b, b a b
	load_imm		0
	beq			L5		
	;; 0, b a
	pop
	push
	;; a, b a
	car
	;; cara, b a
	push
	;; cara, b a cara
	pick			0
	car
	;; carb, b a cara
	push
	push
	;; carb, b a cara carb carb
	load_imm		0
	;; 0, b a cara carb carb
	get_field_value
	;; valb, b a cara carb
	swap
	;; valb, b a carb cara
	exch
	;; cara, b a carb valb
	push
	push
	;; cara, b a carb valb cara cara
	load_imm		0
	get_field_value
	;; vala, b a carb valb cara
	swap
	;; vala, b a carb cara valb
	exch
	;; valb, b a carb cara vala 
	blt			L6
	;; valb, b a carb cara
	pop
	;; cara, b a carb
	pop
	;; carb, b a
	swap
	;; carb, a b
	exch
	;; b, a carb
	cdr
	;; cdrb, a carb
	swap
	;; cdrb, carb a
	exch
	;; a, carb cdrb
	call_global		_merge#1, 2, _merge#2
L6:
	;; valb, b a carb cara
	swap
	;; valb, b a cara carb
	pop
	;; carb, b a cara
	swap
	;; carb, b cara a
	pop
	;; a, b cara
	cdr
	;; cdra, b cara
	swap
	;; cdra, cara b
	call_global		_merge#1, 2, _merge#2
L5:
	pop
	;; a/b, b/a
	return
	.end

	
_alt#2:
	.accu			PTR
	.stack			PTR		1
	.noescape
	cons
	return
	.end

_alt#1:
	.accu			PTR
	.stack					2
	.noescape
	.entry
	push
	push
	;; l l
	load_imm		0
	beq			L3
	;; l
	pop
	push
	;; l
	cdr
	push
	;; l cdr
	load_imm		0
	beq			L3
	;; l 
	pop
	;; 
	push
	;; l
	car
	exch
	;; car
	cddr
	call_global		_alt#1, 1, _alt#2
L3:
	pop
	;; l
	return
	.end
	
	
_sort#6:
	.accu			PTR
	.stack					0
	.noescape
	;; l, -
	return
	.end

_sort#5:
	.accu			PTR
	.stack			PTR 		1
	.noescape
	;; sort(alt(cdr(l))), sort(alt(l))
	exch
	;; sort(alt(l)), sort(alt(cdr(l)))
	call_global		_merge#1, 2, _sort#6
	.end

_sort#4:
	.accu			PTR
	.stack			PTR 		1
	.noescape
	;; alt(cdr(l)), sort(alt(l))
	call_global		_sort#1, 1, _sort#5
	.end

_sort#3:
	.accu			PTR
	.stack			PTR		1
	.noescape
	;; sort(alt(l)), cdr
	exch
	;; cdr, sort(alt(l))
	call_global		_alt#1, 1, _sort#4
	.end

_sort#2:
	.accu			PTR
	.stack			PTR		1
	.noescape
	;; alt(l), cdr
	call_global		_sort#1, 1, _sort#3
	.end

_sort#1:
	.accu			PTR
	.stack					3
	.noescape
	.entry
	push
	push
	;; l l
	load_imm		0
	beq			L4
	;; l
	pop
	push
	;; l, l
	cdr
	;; cdr, l
	push
	;; cdr, l cdr
	swap
	;; cdr, cdr l
	push
	;; cdr, cdr l cdr
	load_imm		0
	beq			L4
	;; 0, cdr l
	pop
	call_global		_alt#1, 1, _sort#2
L4:
	pop
	;; l, 
	return
	.end
	
_mergesort#1:
	.accu			PTR
	.stack					0
	.noescape
	print_list
	die
	.end
	
_mergesort#0:
	.accu			PTR
	.stack					0
	.noescape
	print_list
	call_global		_sort#1, 1, _mergesort#1
	.end

_mergesort:
	.accu			WORD
	.stack					2
	.noescape
	.entry
	push
	push
	;; n, n n
	load_imm		0
	ble			L1
	;; 0, n
	pop
	;; n
	init_random
	call_global		_makelist#1, 2, _mergesort#0
L1:
	die
	.end
