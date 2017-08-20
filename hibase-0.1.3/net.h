/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996, 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Antti-Pekka Liedes <apl@iki.fi>
 */

/* Network interface.  This module implements TCP(/IP) networking services
 * (read/write to a connection, connecting to remote host/port, and
 * accepting connections on the local host) for Shades using BSD sockets
 * found on most current UNIX-like operating systems.  Usage is based
 * on events; each request creates a network event.  If the request could
 * not be fulfilled immediatly (ie. the system call "blocked"), it
 * becomes a pending event.  The resumability for pending events can be
 * checked with `net_number_of_wakeups' that returns the number of
 * potentially resumable events that can be queried with `net_get_wakeup'.
 * When a potential wakeup is encountered, the blocked call should
 * be resumed with the exact same call that caused the original block.
 * A wakeup does not guarantee that the resumable call will not block
 * again, it only means it's worth a new try.
 * Information of event-oriented calls is returned in `net_return_t'
 * (see below).
 *
 * Network connections are distinguished using handles of type
 * `net_handle_t'.  The handle is guaranteed to fit into one shades word.
 * Note that the term "connection" is used for all sockets, even if they
 * do not represent an actual connection (like a listening socket).
 *
 * If a connection generates an error while it has an event pending,
 * the error status is returned by the next net_-call that can block.
 * This way the error is returned by resuming the call that caused the
 * original block and created a pending event.
 *
 * All data is temporarily stored in internal buffers.  Buffering
 * guarantees on data loss or multiplication if blocked calls are
 * resumed properly (ie. with the same call and parameters that
 * caused the original block).
 */


#ifndef INCL_NET_H
#define INCL_NET_H 1

#include "includes.h"
#include "params.h"
#include "shades.h"

#include <sys/time.h>

/* Internal initializations.  MUST be called before anything
   else in the net module. */
void net_init (void);

/* Possible network events. */
typedef enum {
  NET_NO_EVENT,
  NET_READ_EVENT,
  NET_WRITE_EVENT,
  NET_ACCEPT_EVENT,
  NET_CONNECT_EVENT,
  NET_CLOSE_EVENT,
  NET_ERROR_EVENT,
  NUMBER_OF_NET_EVENTS,
} net_event_t;

/* A handle type used to index the sockets. */
typedef word_t net_handle_t;

/* `net_return_t' is the return type of most net_* commands, in particular
   those that may block. */
typedef struct net_return {
  /* Handle of the network connection in question.  Meaning depends on
     the context in which this was returned. */
  net_handle_t handle;
  /* Corresponding thread ID; used to index the thread that was
     suspended because of a call it made blocked.  This value is
     saved for the caller to identify the thread it probably wants
     to resume to retry the blocked call. */
  word_t thread_id;
  /* Error state.  Positive values are system errors (in which
     case error equals to the errno given by the failed system call),
     negative values are internal errors as defined below, and
     0 means no error.
     If the returned error is "fatal", the connection is not alive
     anymore.  It is closed automatically by net, and may not be used
     for further operations.  The macro `NET_IS_FATAL_ERROR' can be
     used to check if a returned error value means a fatal error. */
  int error;
  /* Event that can be retried or that blocked. */
  net_event_t event;
  /* Number of bytes left in the buffer, used for different
     purposes depending on context, see `net_read_mem()' and
     `net_write_mem()'.  From this value the caller can determine
     how fast a connection is operating and thus speed up/slow down
     the corresponding data rate. */
  long bytes_left;
  /* Pointer to the shtring that a succesful call created. */
  ptr_t shtring;
} net_return_t;

/* Possible error states returned by net_*.  These are internal
   errors in net.c, system errors are reported via positive
   error values.  NET_BLOCKED is not really an error. */
/* This operation could not be carried out immediatly.  All functions
   guarantee that if this status is returned, the same call can be made
   with same arguments without any data duplication in buffers. */
/* This is not a fatal (considering the connectivity of the socket) error. */
#define NET_BLOCKED              -1

/* EOF was received from the socket. */
/* This is a fatal error. */
#define NET_EOF                  -2

/* A connection could not be established, because the remote host name
   was unknown. */
/* This is a fatal error. */
#define NET_UNKNOWN_HOST         -3

/* Buffer could not be increased in size to hold all the data to be copied
   or read into it. Buffers of other connections should be decreased in
   order to allow this buffer to grow. */
/* This is not a fatal (considering the connectivity of the socket) error. */
#define NET_BUFFER_ERROR         -4

/* There is not enough space to allocate a shtring to contain the
   requested data.  This most probably means a flush and collection of the
   first generation should be done. */
/* This is not a fatal error. */
#define NET_ALLOCATION_ERROR     -5

/* A shtring_* -call returned error status. */
/* This is not a fatal error. */
#define NET_SHTRING_ERROR        -6

/* No more wakeups are available.  Returned only by `net_get_wakeup'. */
#define NET_NO_MORE_WAKEUPS      -7

#define NET_IS_FATAL_ERROR(error)  (error > 0 ? 1 : \
				    ((error == NET_EOF \
				     || error == NET_UNKNOWN_HOST) ? 1 : 0))


/* Network write of a block of size `number_of_bytes' pointed to by `p' to
   network connection `handle'.
   First check if there are already `write_block_threshold' bytes in the
   buffer, if so, return a blocked status.  Otherwise copy all data give
   into the write buffer, attempt to write as much as possible right away,
   and continue trying every time `net_number_of_wakeups()' is called
   (and the operating system tells us this connection can be written to).
   When the whole contents of the write buffer has been succesfully written,
   the connection wakes up and a wakeup of the write event is returned
   by `net_get_wakeup()'.
   Wakeups have less meaning for writing than for other types of events,
   because further succesful write calls can be made even if a blocked
   write hasn't waken up.
   The byte order of the data is not changed from host byte order.
   return value contains:
   error:      the error status; if fatal, other fields are not filled in.
   handle:     nothing.
   thread_id:  nothing.
   event:      nothing.
   bytes_left: amount of data that could not be written immediatly and
               was left in the buffer. */
net_return_t net_write_mem (word_t thread_id,
			    net_handle_t handle, ptr_t p,
			    unsigned long number_of_bytes);

/* Network write of one character, otherwise like above. */
net_return_t net_write_char (word_t thread_id, net_handle_t handle, char c);

/* Network write of `number_of_bytes' bytes from shtring `shtring' to
   network connection `handle'.
   Like `net_write_mem' above, but writes the content of a shtring rather
   than a memory area.  If `number_of_bytes' is 0 or more than the length
   of the shtring, the whole shtring will be written. */
net_return_t net_write_shtring (word_t thread_id,
				net_handle_t handle, ptr_t shtring,
				unsigned long number_of_bytes);

/* Network read of `number_of_bytes' bytes from network connection `handle'
   into memory area pointed to by `p'. 
   First attempts to read up to the requested amount of bytes to the
   connection's read buffer.  If enough bytes could be read without errors
   (including a blocked system call), the call is succesful and the data is
   copied to the area pointed to by `p'.  If an error occured, the error
   status is returned, and in case of blocking, the connection is marked
   to be pending for reading.  If blocked, the connection may be returned
   as a read wakeup by `net_get_wakeup' later, in which case the same
   read call that blocked should be retried.
   return value contains:
   error:      the error status; if fatal, other fields are not filled in.
   handle:     nothing.
   thread_id:  nothing.
   event:      nothing.
   bytes_left: amount of data that was left missing from the requested amount.
               negative value denotes that more than requested amount of
	       data was read from the connection. */
net_return_t net_read_mem (word_t thread_id,
			   net_handle_t handle, ptr_t p,
			   unsigned long number_of_bytes);

/* Network read of one character, otherwise like above. */
net_return_t net_read_char (word_t thread_id, net_handle_t handle, char *c);

/* Read `number_of_bytes' bytes from network connection `handle' and
   create a shtring from the data.
   Symmetric to `net_read_mem' above, but `shtring' field of the return
   value contains the created shtring, if the call was succesful.  This
   call may fail because not enough space could be allocated, in which
   case `error' field of the return value contains NET_ALLOCATION_ERROR.
   If this happens, the first generation should be collected and this
   call remade.
   The read data is shtringified and concatenated to the end of the given
   `shtring'.  If an allocation error occurs, the shtring already made is
   returned in the return values so that it can be passed as `shtring'
   when `net_read_shtring' is reattempted. */
net_return_t net_read_shtring (word_t thread_id, ptr_t shtring,
			       net_handle_t handle,
			       unsigned long number_of_bytes);

/* Bind a socket to given port and INADDR_ANY on local host to listen
   connections on it.  Port is specified in host byte order.  This function
   will not block.
   return value contains:
   error:      the error status, if fatal, other fields are not filled in.
   handle:     the handle to be used to accept connections.
   thread_id:  nothing.
   event:      nothing.
   bytes_left: nothing. */
/* "Binding" a socket means giving it a name in the corresponding address
   space, ie. for TCP/IP sockets giving it a port and an IP address in the
   localhost.  The IP address of a listening socket denotes the foreign
   IP addresses that are allowed to connect to this socket, NOT the address
   of this endpoint of the actual connection (obviously this endpoint of the
   connection takes the IP address of one of the network interfaces of
   this host).  Setting the socket to "listen" means it will accept
   connections from other endpoints. */
net_return_t net_listen (int port);

/* Accept a connection on a socket that has been set to listen with
   `net_listen'.  If attempt to accept blocks, return blocking status
   and set the socket to be polled for succesful accept.  When a new
   connection is waiting, the socket wakes up and is returned by
   `net_get_wakeup'.
   return value contains:
   error:      the error status, if fatal, other fields are not filled in.
   handle:     handle of the newly established connection, if succesful.
   thread_id:  nothing.
   event:      nothing.
   bytes_left: nothing. */
/* "Accepting" a connection on a socket means creating a new socket
   as the local endpoint of the new connection and establishing the
   connection between it and the foreign endpoint connecting to
   a listening socket on this host.  The result is a new socket
   that is connected to the foreign endpoint. */
net_return_t net_accept (word_t thread_id, net_handle_t handle);

/* `net_prepare_connect' creates a socket and prepares it to be
   connected to the given host and port.  Host is provided as an
   ASCII name, which can be either the DNS name of the host, or
   the IP-address in dotted decimal notation.  Port is provided as
   a short int in host byte order.  `net_prepare_connect' will not
   block.  If an error condition is met, only the `error' field of the
   return value is valid, otherwise all values.
   The actual connecting of the network handle given by
   `net_prepare_connect' can be attempted with `net_do_connect'.
   Only those file descriptors that are prepared for connecting may be
   attempted to connect.  `net_do_connect' may block.
   return value contains:
   error:      the error status, if fatal, other fields are not filled in.
   handle:     handle to be used for `net_do_connect'
               (from `net_prepare_connect'), or
	       handle of the newly established connection
	       (from `net_do_connect')
   thread_id:  nothing.
   event:      nothing.
   bytes_left: nothing.
   NOTE: the current version of the DNS lookup performed in
   `net_do_connect()' will block and *stop the process* indefinetly. */
/* "Connecting" a socket means actively trying to reach a connection
   endpoint in a foreign host (which of course may be physically same
   computer).  The foreign endpoint is addressed using the target's
   IP address and a port number to distinguish the correct endpoint from
   all the possible ones in the target host. */
net_return_t net_prepare_connect (char *host_name, short port);
net_return_t net_do_connect (word_t thread_id, net_handle_t handle);

/* Close the connection, release buffers.  Blocks until all pending
   data to be written is written to the socket, possible pending
   reads are ignored.
   return value contains:
   error:      block (if data is pending), or 0.
   handle:     nothing.
   thread_id:  nothing.
   event:      nothing.
   bytes_left: bytes left in the write buffer. */
net_return_t net_close (word_t thread_id, net_handle_t handle);

/* Set the size of empty space to keep in the buffers.  Normally chunks
   are freed immediately when they contain no unread/written data.  If
   empty_space is > 0, chunks are freed only after there are empty chunks
   of total size of empty_space.  Greater empty_space results in memory
   wastage and improved performance. */
void net_set_read_empty_buffer_size (net_handle_t handle,
				     unsigned long size);
void net_set_write_empty_buffer_size (net_handle_t handle,
				      unsigned long size);

/* Function to set the write blocking threshold, see `net_write_mem()'. */
void net_set_write_block_threshold (net_handle_t handle,
				    unsigned long number_of_bytes);

/* Poll through all connections with pending events.  Try to write
   all pending data into write buffer to those connections that can be
   written to, clearing the pending status if the buffer could be emptied.
   Other types of pending events are not handled here.  After calling
   this `net_get_wakeup()' can be used to iterate all possible wakeups.
   If `timeout' is a NULL pointer, the call will block until at least
   one connection is resumable.  Otherwise `timeout' should be a pointer
   to a timeval indicating how long `net_number_of_wakeups' should wait
   for at least one wakeup (zero can be used for polling only).
   NOTE: Do not assume the value of `timeout' will be the same after
   a call of `net_number_of_wakeups'. */
unsigned long net_number_of_wakeups (struct timeval *timeout);

/* Return the next resumable connection information
   in a net_return_t.  The resumability of the connection is assumed
   to be acknowledged when this function is called, so the
   blocking status of the connection is removed.  If a connection is
   returned as a wakeup, it does NOT mean resuming the call that blocked
   previously will not block again, just that the call has a better chance
   of succeeding than before.  However, the same call can be safely made
   several times, until a non-blocking situation is reached.
   return value contains:
   error:      the error status.
   handle:     handle of the connection that woke up.
   thread_id:  the thread_id that was given with the last function call
               that blocked, used to index the thread to resume.
   event:      type of event that can be retried.
   bytes_left: nothing. */
net_return_t net_get_wakeup (void);

#endif /* INCL_NET_H */
