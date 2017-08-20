/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Definitions of the contents of the root block.
 */


#include "cookies.h"


/* THESE MUST BE FIRST!

   A magic cookie and a 64-bit time stamp.  The cookie distinguishes
   this the root page from other pages and is also used to fix byte a
   difference in byte orders.  The time stamps are needed in order to
   find the newest possible root block in the database. */
ROOT_WORD(magic_cookie, ROOT_MAGIC_COOKIE)
ROOT_WORD(time_stamp_hi, 0)
ROOT_WORD(time_stamp_lo, 1)

ROOT_PTR(interned_shtrings)

/* Byte code interpreter's data. */
ROOT_PTR(bcodes)
ROOT_PTR(globals)
ROOT_PTR(suspended_cont)
ROOT_WORD(suspended_accu_type, 0)
ROOT_WORD(suspended_accu, 0)
ROOT_WORD(suspended_thread_id, 0)
ROOT_WORD(suspended_priority, 0)

/* Context table. */
#ifndef NUMBER_OF_CONTEXT_PRIORITIES
#define NUMBER_OF_CONTEXT_PRIORITIES  4
#endif
ROOT_PTR(contexts)
ROOT_PTR(context1)
ROOT_PTR(context2)
ROOT_PTR(context3)

/* A trie containing blocked byte code threads. */
ROOT_PTR(blocked_threads)
/* A running counter for byte code threads unique identifiers. */
ROOT_WORD(highest_thread_id, 0)

/* For mapping object identities to values and allocating new object
   identies. */
ROOT_PTR(oid_map)
ROOT_PTR(oid_freelist)
ROOT_WORD(oid_in_use, 0)
ROOT_WORD(oid_max, 0)
ROOT_WORD(oid_allocation_cursor, 0)
ROOT_WORD(oid_prev_random, 0)


/* A current view from the C++ world */
ROOT_PTR(cpp_view)

/* A transaction consistent view from the C++ world */
ROOT_PTR(cpp_consistent)


/* These are used for testing purposes somewhere in `test_X.c's. */
ROOT_PTR(test1)
ROOT_PTR(test2)
ROOT_PTR(test3)
ROOT_PTR(test4)

/* TPC-B tables */
ROOT_PTR(test_branch)
ROOT_PTR(test_teller)
ROOT_PTR(test_account)
ROOT_PTR(test_history)

/* SystemM tables */
ROOT_PTR(test_acc_info)
ROOT_PTR(test_cust_info)
ROOT_PTR(test_hot_info)
ROOT_PTR(test_store_info)

/* THESE MUST BE LAST!

   These are the equivalents of the `generation_map' of the latest
   commit group.  See `cells-def.h' and `shades.c' for details. */

ROOT_PTR(generation_pinfo_list)
/* `prev_generation_list' and `prev_prev_generation_list' can be
   declared as words because they needn't be saved from garbage
   collection, although they must reside on disk and addressable on
   it. */
ROOT_WORD(prev_generation_pinfo_list, 0)
ROOT_WORD(prev_prev_generation_pinfo_list, 0)
ROOT_WORD(youngest_generation_number_of_pages, 0)
ROOT_WORD(youngest_generation_number, ((word_t) ~0L))
ROOT_WORD(youngest_generation_number_of_referring_ptrs, 0)
#ifndef MAX_GENERATION_SIZE
#define MAX_GENERATION_SIZE  				\
  ((512/sizeof(word_t) - NUMBER_OF_ROOT_IXS) / 2)
#endif
ROOT_WORD(youngest_generation_page_number0, 0)
ROOT_WORD(youngest_generation_disk_page_number0, 0)
