;;; This file is part of the Shades main memory database system.
;;; 
;;; Copyright (c) 1996 Nokia Telecommunications
;;; All Rights Reserved.
;;;
;;; Authors: Kengatharan Sivalingam <siva@iki.fi>
;;;
	
;;; Byte code assembler source for computing Ulam´s numbers.  
;;; Run with e.g. "test_asm test_ulam.s ulam 30".

;;; The algorithm used in this implementation is given below using C:	
;;; 
;;;      int generate_ulam(int n)
;;;      {
;;;        if (n == 1)
;;;          return 0;
;;;        if (n % 2 == 0)
;;;          return generate_ulam(n / 2) + 1;
;;;        return generate_ulam(3 * n + 1) + 1;
;;;      }
;;; 
;;;      main()
;;;      {
;;;        for (ctr = 1, ulam = 0; ctr <= number; ctr++)
;;;          ulam += generate_ulam(ctr);
;;;      }

_ulam#5:
	.accu		WORD
	.stack				2
	.noescape
	print_int
	die
	.end

_ulam#4:
	.accu		WORD
	.stack		WORD WORD WORD 4
	.noescape			; n ulam ctr, last_ulam
	swap				; n ctr ulam, last_ulam
	add				; n ctr, updated_ulam
	exch				; n updated_ulam, ctr
	add_imm		1		; n updated_ulam, ctr+1
	tail_call_global _ulam#1, 3
	.end
	
_ulam#3:
	.accu		WORD
	.stack				3
	.noescape			; tmp_ulam
	add_imm		1		; , tmp_ulam+1
	return
	.end
	
_ulam#2:
	.accu		WORD
	.stack				3
	.entry
	.noescape			; , n
	push				; n, n
	push				; n n, n
	load_imm	1		; n n, 1
	ble		L3		; n, 1
	dup				; n n, 1
	load_imm	2		; n n, 2
	mod				; n, n%2
	push				; n n%2, n%2
	load_imm	0		; n n%2, 0
	beq		L2		; n, 0
	load_imm	3		; n, 3
	mul				; , n*3
	add_imm		1		; , n*3+1
	call_global	_ulam#2, 1, _ulam#3	
L2:
	load_imm	2		; n, 2
	div				; , n/2
	call_global	_ulam#2, 1, _ulam#3
L3:		
	load_imm	0		; n, 0
	return
	.end

_ulam#1:
	.accu		WORD		
	.stack		WORD WORD	4
	.entry
	.noescape			; n ulam, ctr
	push				; n ulam ctr, ctr
	push				; n ulam ctr ctr, ctr
	pick		0		; n ulam ctr ctr, n
	ble		L1		; n ulam ctr, n
	pick		1		; n ulam ctr, ulam -- Final ulam
	return
L1:		
	pick		2		; n ulam ctr, ctr
	call_global	_ulam#2, 1, _ulam#4
	.end

_ulam:
	.accu		WORD
	.stack				2
	.entry
	.noescape			; , n
	push				; n, n
	load_imm	0		; n, 0 -- Init ulam
	push				; n ulam, 0
	load_imm	1		; n ulam, ctr -- Init counter
	call_global	_ulam#1, 3, _ulam#5
	.end
