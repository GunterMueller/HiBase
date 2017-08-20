/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Antti-Pekka Liedes <apl@iki.fi>
 */

/* Various magic cookies used in the program.
 */

#ifndef INCL_COOKIES
#define INCL_COOKIES  1


/* The first word of the root block should contain this. */
#define ROOT_MAGIC_COOKIE  (0x50073A61L)

/* The first word of each page should contain this. */
#define PAGE_MAGIC_COOKIE  (0x4A6E3A61L)

/* The first word of an unused page on disk should contain this. */
#define UNUSED_PAGE_COOKIE  (0xDEAD1541L)

/* Denotes the tail of the list containing the remembered set. */
#define REM_SET_TAIL_COOKIE  ((rem_set_t *) (ptr_as_scalar_t) 0xF5E35E7FL)


#ifndef NDEBUG

/* Clear the first generation and free parts of the mature generation
   with this deadbeef. 

   Note that the first generation deadbeef could be understood as a
   pointer value with correct alignment.  This allows us to delay the
   full initialization of cells before calling `allocate' a second
   time, but they will have to be fully initialized before
   `flush_batch' is called. */
#define FIRST_GENERATION_DEADBEEF  (0xF6BEEF00L)
#define PAGE_DEADBEEF  (0x4A9EBEEFL)

/* When debugging with red zones enabled, pad each allocation in the
   first generation with this "red zone" in the highest 16 bits of the
   word and the size of the allocation in the lowest 16 bits of the
   word. */
#define FIRST_GENERATION_RED_ZONE  (0xF642)

/* Clobber each undeclared word in checked cells with this cookie.
   This helps detecting incorrect cell declarations, particularly
   continuation frames. */
#define UNDECL_WORD_CLOBBER_COOKIE  (0xC2DEC7FFL)

#endif


#endif  /* INCL_COOKIES */
