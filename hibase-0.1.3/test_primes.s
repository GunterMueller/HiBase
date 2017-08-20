;;; This file is part of the Shades main memory database system.
;;;
;;; Copyright (c) 1996 Nokia Telecommunications
;;; All Rights Reserved.
;;;
;;; Authors:	Antti-Pekka Liedes <apl@iki.fi>
;;;
;;; Assembler source for a simple primality test loop.

_prime_loop#1:	
	.accu	WORD
	.stack	WORD WORD	4
	.entry
	dup			; x sqrt_x sqrt_x, d
	blt	L1		; x sqrt_x, d
	push			; x sqrt_x d, d
	push			; x sqrt_x d d, d
	pick	0		; x sqrt_x d d, x
	exch			; x sqrt_x d x, d
	mod			; x sqrt_x d, (x mod d)
	push			; x sqrt_x d (x mod d), (x mod d)
	load_imm	0	; x sqrt_x d (x mod d), 0
	beq	L2		; x sqrt_x d, 0
	pop			; x sqrt_x, d
	add_imm	1		; x sqrt_x, (d + 1)
	tail_call_global _prime_loop#1, 3
L1:
	load_imm	1	; , 1
	return
L2:
	load_imm	0	; , 0
	return
	.end

_primep#1:
	.accu	WORD
	.stack			2
	.entry
	push			; x, x
	sqrt			; x, sqrt_x
	push			; x sqrt_x, sqrt_x
	load_imm	2	; x sqrt_x, 2
	call_global	_prime_loop#1, 3, _primep#2
	.end

_primep#2:
	.accu	WORD
	.stack			2
	return
	.end

_primes_loop#1:
	.accu	WORD
	.stack	WORD		3
	.entry
	dup			; x x, xmax
	bgt	L1		; x, xmax
	push			; x xmax, xmax
	pick	0		; x xmax, x
	call_global	_primep#1, 1, _primes_loop#2
L1:
	return
	.end

_primes_loop#2:
	.accu	WORD
	.stack	WORD WORD	3
	push			; x xmax primep(x), primep(x)
	load_imm	0	; x xmax primep(x), 0
	beq	L1		; x xmax, 0
	pick	0		; x xmax, x
	print_int		; x xmax, x
L1:	
	swap			; xmax x, ?
	load_imm	1	; xmax x, 1
	add			; xmax, (x + 1)
	exch			; (x + 1), xmax
	tail_call_global _primes_loop#1, 2
	.end

_main:
	.accu	WORD
	.stack			2
	.entry
	push			; x, x
	load_imm	2	; x, 2
	push			; x 2, 2
	swap			; 2 x, 2
	pop			; 2, x
	call_global	_primes_loop#1, 2, _terminate
	.end

_terminate:
	.accu	WORD
	.stack			2
	die
	.end
	
