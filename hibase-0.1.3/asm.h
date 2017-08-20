/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* External interface of the byte code assembler.
 */


#ifndef INCL_ASM
#define INCL_ASM 1


/* Read the given assembler file and declare all the byte codes in it.
   Return non-zero on error, zero on success. */

int asm_file(char *filename);


#endif /* INCL_ASM */
