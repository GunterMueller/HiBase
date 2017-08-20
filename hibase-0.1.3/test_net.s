;;; This file is part of the Shades main memory database system.
;;; 
;;; Copyright (c) 1997 Nokia Telecommunications
;;; All Rights Reserved.
;;;
;;; Authors: Antti-Pekka Liedes <apl@iki.fi>
;;;
	
;;; Byte code assembler source for a server which replies to lines
;;; sent through telnet connections as such.

_daemon:
	.accu		WORD	; Contains the port number.
	.stack		2
	.noescape
	.entry
	net_listen	L1	; error, handle
	drop			; , handle
	push			; handle, handle
	goto_bcode	_daemon#1
L1:	
	die
	.end

_daemon#1:
	.accu		WORD
	.stack		WORD	2
	.noescape		; daemon_handle, daemon_handle
	net_accept	L2	; daemon_handle error, connected_handle
	drop			; daemon_handle, connected_handle
	push			; daemon_handle connected_handle, connected_handle
	spawn		_responder#1, 2, 2
				; daemon_handle connected_handle, connected_handle
	drop			; daemon_handle, connected_handle
	pick		0	; daemon_handle, daemon_handle
	goto_self	
L2:
	die
	.end

_responder#1:
	.accu		WORD
	.stack		WORD	2
	.entry
	.noescape		; handle, handle
	net_read_char	L3	; handle error, ch
	drop			; handle, ch
	push			; handle ch, ch
	pick		0	; handle ch, handle
	goto_bcode	_responder#2
L3:
	die
	.end

_responder#2:
	.accu		WORD
	.stack		WORD WORD	2
	.noescape		; handle ch, handle
	net_write_char	L4	; handle, error
	pick		0	; handle, handle
	goto_bcode 	_responder#1
L4:
	die
	.end
