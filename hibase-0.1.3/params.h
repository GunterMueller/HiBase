/* This file is part of the Shades main memory database system.
 *
 * Copyright (c) 1996 Nokia Telecommunications
 * All Rights Reserved.
 *
 * Authors: Kenneth Oksanen <cessu@iki.fi>
 *          Kengatharan Sivalingam <siva@iki.fi>
 */

/* Parameter handling in Shades. 
 */

/* This file, along with `params.c' and `params-defs.h', implements
   parameter handling in Shades. `params.c' consists of parameter 
   handling routines and `params-def.h' defines all the parameters 
   used in the program.

   Initialize:
   -----------
   Parameters are initialized by calling `init_params(argc, argv)'.
   Command line arguments which do not have any parameter information 
   have to be read BEFORE calling this function and assigned to NULL. 
   This function loads parameter values defined in various sources in 
   the following order: resource files, environment variables and 
   command line arguments. Old values are overridden by new if the
   parameter value is defined more than once.

   Resource files:
   ---------------
   The resource file `.shadesrc' is searched and read from directories 
   in the follwing order: standard library, home directory and 
   current/working directory. In resource files, lines start with a 
   '#' character are treated as comments:

                  # this is a comment
                  db_size = 20480
		  
   Environment variables:
   ----------------------
   The environment variable name of a parameter is obtained by
   concatenating a predefined prefix (PARAMS_ENV_PREFIX) with the name 
   of the parameter. For example, if the prefix is "SHADES" and the 
   name of the parameter is "db_size", the environment variable 
   will be SHADES_DB_SIZE. 
	   
                  export SHADES_DB_SIZE=20480
   
   Command line arguments:		  
   -----------------------
   New values can be assigned to parameters using command line
   arguments.

	          ./shades --db-size=20480

   Add a new parameter:
   --------------------
   A new parameter is added into `params-def.h' as the following:

	          PARAMS(Parameter type, Name, Default value)

   Delete a parameter:
   -------------------
   The line, where the parameter is defined in `params-def.h', is 
   removed. 

   Parameter name:
   ---------------
   In `params-def.h', the underscore character has to be used to 
   separate words.  In parameter sources, '-' is also accepted. Here
   is the list of all legal name formats: `DB_SIZE', `db_size', 
   `DB-SIZE', `db-size', `DBSIZE' and `dbsize'. 

   Parameter type:
   ---------------
   Currently `int', `double' and `char *' are accepted as
   parameter types.

   Parameter value:
   ----------------
   Various formats can be used to express parameter values in
   parameter sources:

	  Type		Format			Meaning
	  ====		======			=======
	  int	  	2097152			
          int,double	2G			2 Gigabytes
          int,double	2M			2 Megabytes
          int,double	2k			2 kilobytes
          int		2^20			pow(2, 20)
          int		yes			1
          int		true			1
          int		no			0
          int		false			0
	  int		0xab			Hex
	  int		0712			Octal
	  double	1.2333			
          double	2m			2 millibytes
          double	2u			2 microbytes
          double	2n			2 nanobytes
          char *	shades_db		
	  char *	"shades_db"
          char * 	~/db/shades_db		
          char * 	~user/db/shades_db      

   For integer parameters, it is also possible to combine hex, octal or 
   power type with 'G', 'M' or 'k' suffix. For example, the following 
   formats are legal: 0x10k, 020M and 2^4k. Note that the base number 
   used here is 1024 for integer values, for example 1M = 1024^3, and 
   1000 for doubles.

   `init_params' is able to expand string type values which start with
   a '~' character. The following formats are legal for giving string
   parameter values: "~", "~/dir/file", "~user" and "~user/dir/file".

   Print parameters:
   -----------------
   The function `fprint_params(fp)' prints all the parameters defined 
   with their values and sources to `fp' as shown below:

	          max_gc_effort = 10485760 (Default)
	          max_gc_limit = 20480 (Environment variable)
	          start_gc_limit = 20971520 (File: "./.shadesrc")

   If one of the command line arguments is `-v', `--verbose' or 
   `--show-params', the function `init_params()' calls 
   `fprint_params(stdout)' after loading all the parameters. */


#define PARAM(t, n, d) \
  extern t n;
#include "params-def.h"
#undef PARAM

#define PARAMS_ENV_PREFIX                   "SHADES"

/* Initialize and load Shades parameters. Values can be assigned 
   to paramters using command line, environment variables or Shades
   resource files. */
void init_params(int argc, char **argv);

/* Print current parameters to `fp'. */
void fprint_params(FILE *fp);
