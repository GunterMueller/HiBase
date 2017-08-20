/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Kengatharan Sivalingam <siva@iki.fi>
 */

/* Shades' parameter definitions.
 */


/* Parameters related to memory size and garbage collection. */

/* Database size in memory in bytes.  Must be an integer multiple of
   `PAGE_SIZE'. */
PARAM(int, db_size, 20*1024*1024)

/* Size of the first generation, in bytes.  Also serves as the upper
   limit of the commit group. */
PARAM(int, first_generation_size, 1*1024*1024)

/* Merge two mature generations if their size before collection is
   less than `first_generation_size * relative_mature_generation_size'. */
PARAM(double, relative_mature_generation_size, 0.7)

/* Maximum writing for mature garbage collection during each group
   commit.  */
PARAM(int, max_gc_effort, 20*1024*1024)

/* Amount of free memory when the maximum mature garbage collection
   effort is reached. */
PARAM(int, max_gc_limit, 1.5*1024*1024)

/* Amount of free memory when mature garbage collection is initiated.
   Idle-time mature garbage collection is done regardless of this
   parameter.  */
PARAM(int, start_gc_limit, (int) 2.5*1024*1024)

/* Is additional generationality allowed?  

   XXX Currently unrecoverable. */
PARAM(int, allow_additional_generationality, 0)

/* Given a average generation shrinkage of `avg_generation_shrinkage',
   then collect a mature generation if it's previous shrinkage was
   less than `avg_generation_shrinkage + generation_shrinkage_margin'. */
PARAM(double, generation_shrinkage_margin, 0.2)

/* How many remembered sets do we allocate with a single malloc
   call.  See remembered sets in `shades.c'. */
PARAM(int, rem_sets_per_malloc, 24)


/* Parameters for IO.
 */


/* Backup file IO.
 */

/* Name of the disk backup file. */
PARAM(char *, disk_filename, "/dev/null")

/* Permissions of the disk backup file, as octal.  Default is
   `rw-rw-r--' */
PARAM(int, disk_file_permissions,
      (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH))

/* Group `gid_t' of the disk backup file. */
PARAM(int, disk_file_group, 4521)

/* Size of the database files, given in the same order as disk filenames. */
/* If there are not enough sizes in the list, the last entry is duplicated
   to remaining files. */
PARAM(char *, disk_filesize, "300M")

/* Don't use the following number of first bytes in the raw disk in
   order to prevent overwriting partition tables.  Suggest using 1 MB
   for raw devices. */
PARAM(int, disk_skip_nbytes, 1*1024*1024)

/* If this is set, displays root timestamps on reading and writing
   of root block. */
PARAM(int, root_timestamp_is_displayed, 0)

/* If this is set, displays extra information on root search. */
PARAM(int, root_search_is_verbose, 0)

/* This enables lots of extraneous output from the pthread asynchronous
   IO implementation. */
PARAM(int, pthread_io_is_verbose, 0)

/* This enables displaying of load for files when they are asked for
   from asyncio.  Meaningful only when FILE_LOAD_BALANCING is enabled. */
PARAM(int, file_load_is_displayed, 0)

/* Enable display of file usage statistics.  Meaningful only when
   FILE_USAGE_STATISTICS is enabled. */
PARAM(int, file_usage_is_displayed, 0)

/* Network IO.
 */

/* Size of the buffer chunks. */
PARAM(int, buffer_chunk_size, 1024)

/* Listen port.  The port the shades server listens to. */
PARAM(int, listen_port, 7777)


/* Parameters that control the byte code interpreter.
 */

/* How many byte code sequences to execute before a context switch. */
PARAM(int, jiffies_between_yields, 100)

/* If there were no runnable threads, then how long shall we wait in
   the network code? */
PARAM(int, usecs_for_network_select_when_idle, 1000)


/* Parameters for verbosity and testing of Shades itself.
 */

/* Parameter for setting verbose mode. */
PARAM(int, be_verbose, 0)

/* Is bcode instruction output disabled? */
PARAM(int, print_insns_are_disabled, 0)

/* Parameters for controlling debugging and verbosity. */
PARAM(int, first_generation_assertion_heaviness, 60)


/* Parameters of various benchmarks and test programs.  These will
   eventually be removed or pruned. */


/* `test_tpcb': Maximum number of records the History table can
   contain. As default, the table is not created during TPC-B
   tests. */
PARAM(int, test_tpcb_history_length, 0)

/* `test_tpcb': Flush-batch latency measurement intervals in TPC-B
   given in seconds.  If the value of this parameter is zero, no
   latency will be measured. */
PARAM(int, number_of_latency_measurement_intervals, 20)

/* `test_tpcb': Maximum latency time given in seconds. */
PARAM(double, max_latency_measurement, 2)


/* `test_shtring': What to test. */
PARAM(char *, test_shtring, "all")

/* `test_shtring': Maximum number of chunks to use. */
PARAM(int, max_shtring_chunks, 0)

/* `test_shtring_speed': How many times to repeat a loop. */
PARAM(int, shtring_loops, 1)

/* `test_shtring_speed': How big shtrings to create for tests, in bytes. */
PARAM(int, shtring_size, 1024)


/* `test_access_method': Name of the access method. */
PARAM(char *, test_am_name, "trie")

/* `test_access_method': Test type. */
PARAM(char *, test_am_type, "insert")

/* `test_access_method': The number of keys added and removed 
   alternately during insert/deletion tests. */
PARAM(int, test_am_key_set_size, 50000)


