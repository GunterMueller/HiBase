/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1997 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 */

/* Recover the database and try to resume a previous byte code
   thread. */

#include "includes.h"
#include "shades.h"
#include "asm.h"
#include "test_aux.h"
#include "interp.h"

static char *rev_id = "$Id: test_recovery.c,v 1.3 1997/04/07 04:43:56 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;


int main(int argc, char **argv)
{
  double t1, t2;

  t1 = give_time();
  srandom(0);
  init_interp(argc, argv);
  init_shades(argc, argv);
  t2 = give_time();
  fprintf(stderr, "Startup time  = %.2f\n", t2 - t1);
  recover_db();
  fprintf(stderr, "Recovery time = %.2f\n", give_time() - t2);
  interp(NULL_PTR, 0, 0);
  return 0;
}
