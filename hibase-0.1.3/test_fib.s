;;; This file is part of the Shades main memory database system.
;;; 
;;; Copyright (c) 1996 Nokia Telecommunications
;;; All Rights Reserved.
;;;
;;; Authors: Kenneth Oksanen <cessu@iki.fi>
;;;
	
;;; Byte code assembler source for the stupid way of computing
;;; Fibonacci numbers.  Run with e.g. 'test_asm test_fib.s fib 30'.

_fib#3:
	.accu		WORD
	;; The types of the contents of the stack, from lowest to topmost item.
	.stack 		WORD WORD 	3
	;; The frame does not escape from the call, i.e. it is not reusable
	.noescape
	add
	return
	.end

_fib#2:
	.accu		WORD
	.stack 		WORD		3
	.noescape
	push
	pick		0
	push
	load_imm	2
	sub
	call_global	_fib#1, 1, _fib#3
	.end

_fib#1:
	.accu		WORD
	.stack				3
	.noescape
	.entry
	push
	push
	load_imm	1
	bgt		L1
	return
L1:	
	pick		0
	push
	load_imm	1
	sub
	call_global	_fib#1, 1, _fib#2
	.end
	
_fib#4:
	.accu		WORD
	.stack				0
	.noescape
	print_int
	die
	.end

_fib:
	.accu		WORD
	.stack				0
	.noescape
	.entry
	call_global	_fib#1, 1, _fib#4
	.end
