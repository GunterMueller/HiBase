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

#include <math.h>
#include <pwd.h>
#include "includes.h"

static char *rev_id = "$Id: params.c,v 1.33 1998/03/03 18:13:21 cessu Exp $";
static char *rev_host = SHADES_REV_HOST;
static char *rev_date = SHADES_REV_DATE;
static char *rev_by = SHADES_REV_BY;
static char *rev_cc = SHADES_REV_CC;

typedef enum param_source_t {
  DEFAULT, SRC_FILE, ENVIRONMENT, COMMAND_LINE_ARGUMENT
} param_source_t;

#define PARAM(type, name, data) \
  type name = data;
#include "params-def.h"
#undef PARAM


#define PARAM(type, name, data) \
  {#name, #type, (void *) &name, NULL, DEFAULT},
static struct {
  char *name;
  char *type;
  void *value;
  char *src_file;
  param_source_t source;
} params[] = {
#include "params-def.h"
  { NULL, NULL, NULL, DEFAULT }
};
#undef PARAM

#define SHADES_PARAM_FILE_HOME_DIR          "~/.shadesrc"
#define SHADES_PARAM_FILE_CURRENT_DIR       "./.shadesrc"
#define SHADES_PARAM_FILE_LIB_DIR           LIBDIR "/.shadesrc"

#define SUFFIX_FORMAT_INTEGER               0
#define SUFFIX_FORMAT_DOUBLE                1

#define MAX_STR_LEN                         255

static void load_command_line_params(int, char **);
static void load_param_file(void);
static void load_env_values(void);
static void set_param_value(char *, char *, char *, param_source_t);
static void set_value_int(int, char *);
static void set_value_double(int, char *);
static void set_value_str(int, char *);
static char *verify_env_value(char *);
static int verify_integer_format(char *, int *);
static int verify_double_format(char *, double *);
static char *strip_string(char *);
static int expand_tilde(char *, char *, size_t);
static void remove_word_separator(char *, int, char *, int);
static double get_suffix_value(int, int);
static void replace_character(char *, int, int);


void init_params(int argc, char **argv)
{
  static int params_are_loaded = 0;

  /* This function should be called only once. */
  assert(!params_are_loaded);
  params_are_loaded = 1;

  load_param_file();
  load_env_values();
  load_command_line_params(argc, argv);

  if (be_verbose)
    fprint_params(stdout);
}

void fprint_params(FILE *fp)
{
  int i;

  assert(fp != NULL);

  for (i = 0; params[i].name != NULL; i++) {
    /* Print name. */
    fprintf(fp, "%s = ", params[i].name);
    
    /* Print type and value. */
    if (strcmp(params[i].type, "int") == 0)
      fprintf(fp, "%d", * (int *) params[i].value);
    else if (strcmp(params[i].type, "double") == 0)
      /* The defualt precision of double values is 10. */
      fprintf(fp, "%.10f", * (double *) params[i].value);
    else if (strcmp(params[i].type, "char *") == 0)
      fprintf(fp, "\"%s\"", * (char **) params[i].value);

    /* Print source. If the source is a file, print also the name 
       and the path of the file. */
    switch(params[i].source) {
    case DEFAULT:
      fprintf(fp, " (%s)\n", "Default");
      break;

    case SRC_FILE:
      fprintf(fp, " (%s: \"%s\")\n", "File", params[i].src_file);
      break;

    case ENVIRONMENT:
      fprintf(fp, " (%s)\n", "Environment variable");
      break;

    case COMMAND_LINE_ARGUMENT:
      fprintf(fp, " (%s)\n", "Command line argument");
      break;
    }
  }
}


/* Verify command line arguments for parameters. The arguments which are 
   not equal to NULL (already read) are considered as parameters. */ 
static void load_command_line_params(int argc, char **argv)
{
  int i, j;
  char name[MAX_STR_LEN + 1], value[MAX_STR_LEN + 1];
  char *cp;

  if (argc < 2)
    return;

  for (i = 1; i < argc; i++) {
    cp = argv[i];

    if (cp == NULL)
      continue;
    
    /* The verbose-mode arguments shouldn't be assigned to NULL after
       reading. Because some other modules still use these arguments
       directly, the assignment is intentionally skipped here. XXX */
    if (strcmp(cp, "-v") == 0 || strcmp(cp, "--verbose") == 0 ||
	strcmp(cp, "--show-params") == 0) {
      for (j = 0; params[j].name != NULL; j++)
	if (strcmp(params[j].name, "be_verbose") == 0) {
	  * (int *) params[j].value = 1;
	  params[j].source = COMMAND_LINE_ARGUMENT;
	  break;
	}
      continue;
    }

    if (*cp == '-' && *(cp + 1) == '-') {
      cp += 2;

      if (strlen(cp) > MAX_STR_LEN) {
	fprintf(stderr, "Fatal: Too long parameter definition \"%s\"\n", 
		cp);
	exit(1);
      }
      else if (sscanf(cp, "%[^=]=%s", name, value) != 2) {
	fprintf(stderr, "Fatal: Bad parameter syntax \"%s\"\n", cp);
	exit(1);
      }
      else 
	set_param_value(name, value, NULL, COMMAND_LINE_ARGUMENT);
    }
    else {
      fprintf(stderr, "Fatal: Unknown parameter syntax \"%s\"\n", cp); 
      exit(1);
    }

    argv[i] = NULL;
  }
  return;
}

/* Read source file ".shadesrc". Search directories in the following 
   order: system's library, home and current directory. */
static void load_param_file()
{
  int i;
  FILE *fp;
  char *file, *cp;
  char line[MAX_STR_LEN + 1], name[MAX_STR_LEN + 1];
  char value[MAX_STR_LEN + 1], file_path[MAX_STR_LEN + 1];
  
  for (i = 0; i < 3; i++) {
    if (i == 0)
      file = SHADES_PARAM_FILE_LIB_DIR;
    else if (i == 1)
      file = SHADES_PARAM_FILE_HOME_DIR;      
    else 
      file = SHADES_PARAM_FILE_CURRENT_DIR;

    if (!expand_tilde(file, file_path, MAX_STR_LEN + 1))
      continue;

    fp = fopen(file_path, "r");
    if (fp == NULL) {
#if 0      
      perror(file_path);
#endif
      errno = 0;
      continue;
    }

    while (fgets(line, sizeof(line), fp)) {
      for (cp = line; *cp && isspace(*cp); cp++)
	;
      if (!*cp || *cp == '#')    /* A comment or empty line detected. */
	continue;
      if (sscanf(line, "%[^=]=%s", name, value) != 2) {
	/* Remove newline character if found. */
	cp = strchr(line, '\n');
	if (cp)
	  *cp = '\0';
	fprintf(stderr, "Fatal: Bad parameter file syntax \"%s\"\n", line);
	exit(1);
      }
      set_param_value(name, value, file, SRC_FILE);
    }
    fclose(fp);
  }
}

/* Load values for the parameters whose values are defined using 
   environment variables. */
static void load_env_values()
{
  set_param_value(NULL, NULL, NULL, ENVIRONMENT);
}

/* Set a new value to the parameter `name'. If the source is a file, 
   also the file path has to be passed in `src_file'. */
static void set_param_value(char *name, char *value, char* src_file,
			    param_source_t src)
{
  int i;
  char name_no_underscore[MAX_STR_LEN + 1];  

  for (i = 0; params[i].name != NULL; i++) {
    if (src == ENVIRONMENT) {
      /* Loop through the entire parameter list for possible values
	 defined in environment variables. */
      if (!(value = verify_env_value(params[i].name)))
	continue;
    }
    else {
      name = strip_string(name);
      if (!name)
	return;
      replace_character(name, '-', '_');

      if (strcasecmp(name, params[i].name) != 0) {
	/* Get parameter name without underscore. */
	remove_word_separator(params[i].name, '_', name_no_underscore, 
			      MAX_STR_LEN + 1);
	if (strcasecmp(name, name_no_underscore) != 0)
	  continue;
      }
    }
    
    value = strip_string(value);
    if (!value)
      return;

    if (strcmp(params[i].type, "int") == 0)
      set_value_int(i, value);
    else if (strcmp(params[i].type, "double") == 0)
      set_value_double(i, value);
    else if (strcmp(params[i].type, "char *") == 0)
      set_value_str(i, value);

    /* Assign the new value and the source to the parameter. */
    if (src == SRC_FILE)
      params[i].src_file = src_file;
    params[i].source = src;

    if (src == ENVIRONMENT)
      continue;
    return;
  }

  if (src != ENVIRONMENT) {
    fprintf(stderr, "Fatal: Unknown parameter \"%s\"\n", name);
    exit(1);
  }
}

/* Set integer value to parameter. */
static void set_value_int(int param_idx, char *val)
{
  int int_value;

  if (!verify_integer_format(val, &int_value))
    {
      fprintf(stderr, "Fatal: Unknown integer format \"%s\"\n", val);
      exit(1);
    }

  * (int *) params[param_idx].value = int_value;
}

/* Set double value to parameter. */
static void set_value_double(int param_idx, char *val)
{
  double d_val;

  if (!verify_double_format(val, &d_val))
    {
      fprintf(stderr, "Fatal: Unknown double format \"%s\"\n", val);
      exit(1);
    }

  * (double *) params[param_idx].value = d_val;
}

/* Set string value to parameter. */
static void set_value_str(int param_idx, char *value)
{
  char *str_value, *value_p;
  char path[MAX_STR_LEN + 1];

  /* If the first character happens to be tilde, then expand it to
     get the corresponding absolute path. */
  if (*value == '~') {
    strcpy(path, "");
    if (!expand_tilde(value, path, MAX_STR_LEN + 1)) {
      fprintf(stderr, "Fatal: Unknown string format \"%s\"\n", value);
      exit(1);
    }
    value_p = path;
  } else 
    value_p = value;
  
  /* Now allocate memory for the new value. Note that the memory
     allocated here for new strings is never explicitly freed. */
  str_value = malloc(strlen(value_p) + 1);
  if (str_value == NULL) {
    fprintf(stderr, "Fatal: malloc failed.\n");
    exit(1);
  }

  strcpy(str_value, value_p);
  * (char **) params[param_idx].value = str_value;
}

/* Verify and get value for `param_name' defined using environment
   variable. A prefix added to the name before it is converted into
   six possible formats. The function returns the value returned
   by a successful `getenv' call, otherwise returns NULL. */
static char *verify_env_value(char *param_name)
{
  int i;
  char *cp, *s1, *s2, *value;
  char env_name_upper[MAX_STR_LEN + 1];
  char env_name_lower[MAX_STR_LEN + 1];
  char no_separator_upper[MAX_STR_LEN + 1]; 
  char no_separator_lower[MAX_STR_LEN + 1];

  /* Add a predefined prefix to the name in order to get the 
     corresponding environment variable name. Note that the resulted 
     string might contain both lower and upper cases. */
  strcpy(env_name_upper, PARAMS_ENV_PREFIX);
  strcat(env_name_upper, "_");
  strncat(env_name_upper, param_name, MAX_STR_LEN - strlen(env_name_upper));

  s1 = s2 = NULL;
  for (i = 0; i < 3; i++) {
    if (i == 0) {
      /* Convert all the lower letters to upper, e.g. `ENV_VARIABLE_NAME' */
      for (cp = env_name_upper; *cp; cp++)
	if (islower(*cp))
	  *cp = toupper(*cp);

      /* Convert now upper case to lower, e.g. `env_varaible_name' */
      strcpy(env_name_lower, env_name_upper);
      for (cp = env_name_lower; *cp; cp++)
	if (isalpha(*cp))
	  *cp = tolower(*cp);
      
      s1 = env_name_upper;
      s2 = env_name_lower;
    }
    else if (i == 1) {
      /* Replace underscore with minus to get `ENV-VARIABLE-NAME' and 
	 `env-variable-name' */
      replace_character(s1, '_', '-');
      replace_character(s2, '_', '-');
    }
    else {
      /* Now remove minus to get `ENVVARIABLENAME' and `envvariablename' */
      remove_word_separator(s1, '-', no_separator_upper, MAX_STR_LEN + 1);
      remove_word_separator(s2, '-', no_separator_lower, MAX_STR_LEN + 1);
      s1 = no_separator_upper;
      s2 = no_separator_lower;
    }

    if ((value = getenv(s1)) || (value = getenv(s2)))
      return value;
  }

  return NULL;
}

/* This function verifies and extracts an integer value from `value' and 
   returns it in `int_value'. */
static int verify_integer_format(char *value, int *int_value)
{
  double d, power;
  int i, suffix, ret_value = 0;
  char c1, c2;
  
  /* Boolean -> Format: "true" || "yes" */
  if (strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0) {
    *int_value = 1;
    return 1;
  }
  
  /* Boolean -> Format: "false" || "no" */
  if (strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0) {
    *int_value = 0;
    return 1;
  }

  if (*value == '0') {
    /* Hex -> Format: 0xab || 0xabM */
    if (*(value + 1) == 'x') {
      ret_value = sscanf(value, "%x%c", &i, &c1);
      d = i;
    }
    else if (isdigit(*(value + 1))) {
      /* Octal -> Format: 0123 || 0123k */
      ret_value = sscanf(value, "%o%c", &i, &c1);
      d = i;
    }
    else 
      /* Format: 0k || 0.2M. */
      ret_value = sscanf(value, "%lf%c", &d, &c1);
  }

  if (ret_value == 0) {
    /* Decimal || Power ->  Format: 2 || 2M || 2^7 || 2^7k */
    ret_value = sscanf(value, "%lf%c%lf%c", &d, &c1, &power, &c2);
  }

  if (ret_value == 1)
    *int_value = d;
  else if (ret_value == 2) {
    /* Number is followed by a suffix letter. */
    suffix = get_suffix_value(c1, SUFFIX_FORMAT_INTEGER);
    if (suffix >= 0)
      *int_value = d * suffix;
    else 
      return 0;
  }
  else if ((ret_value == 3 || ret_value == 4) && c1 == '^') {
    *int_value = pow(d, power);
    if (ret_value == 4) {
      suffix = get_suffix_value(c2, SUFFIX_FORMAT_INTEGER);
      if (suffix >= 0)
	*int_value = *int_value * suffix;
      else 
	return 0;
    }
  }
  else 
    return 0;
  return 1;
}

/* This function verifies and extracts a double value from `value' and 
   returns it in `double_value'. */
static int verify_double_format(char *value, double *double_value)
{
  double d, suffix;
  int ret_value;
  char c;

  ret_value = sscanf(value, "%lf%c", &d, &c);
  if (ret_value == 1)
    *double_value = d;
  else if (ret_value == 2) {
    /* Number is followed by a suffix letter. */
    suffix = get_suffix_value(c, SUFFIX_FORMAT_DOUBLE);
    if (suffix >= 0)
      *double_value = d * suffix;
    else 
      return 0;
  }
  else 
    return 0;
  return 1;
}

/* Remove leading and trailing space and quote characters from `str'. */
static char *strip_string(char *str)
{
  char *cp;

  if (!str)
    return NULL;

  /* Remove leading space and quote characters. */
  for (; isspace(*str) || *str == '"'; str++)  
    ;

  /* Verify that the string is not empty after removing leading spaces. */
  if (*str == '\0')
    return NULL;

  /* Remove trailing space and quote characters. */
  cp = &str[strlen(str) - 1];
  for (; isspace(*cp) || *cp == '"'; cp--)
    ;
  *++cp = '\0';

  return str;
}

/* Expand '~' (tilde) character in a string. Note that the tilde must be the 
   first character of the string. This function returns the new string in 
   `path' after a successful call. Four different formats are accepted by 
   this function: "~", "~/dir/file", "~user" and "~usr/dir/file". 

   After each `strncpy' call, a test is added to verify that the 
   dest-string contains a null character at the end, otherwise a null 
   character is added. */
static int expand_tilde(char *value, char *path, size_t path_len)
{
  size_t user_name_length, value_len;
  char *cp;
  char user[MAX_STR_LEN + 1];
  struct passwd *usr_pw;

  value_len = strlen(value);

  /* If the first character is not tilde, simply copy the `value' 
     string to `path' and return. */
  if (*value != '~') {
    strncpy(path, value, path_len - 1);
    if (value_len > path_len - 1)
      path[path_len - 1] = '\0';
    return 1;
  }

  /* Format: "~" || "~/dir/file" */
  if (value_len == 1 || value[1] == '/') {    
    cp = getenv("HOME");
    if (!cp)
      return 0;

    if (value_len == 1) {                        /* Format: "~" */
      strncpy(path, cp, path_len - 1);
      if (strlen(cp) > path_len - 1)
	path[path_len - 1] = '\0';
      return 1;
    }

    value++;                                     /* Format: "~/dir/file" */
    strncpy(path, cp, path_len - strlen(value) - 1);
    if (strlen(cp) > path_len - strlen(value) - 1)
      path[path_len - strlen(value) - 1] = '\0';
    strcat(path, value);
    return 1;
  }

  cp = strchr(value, '/');
  if (cp) {
    user_name_length = cp - strchr(value, '~') - 1;
    strncpy(user, value + 1, user_name_length);  /* Format: "~usr/dir/file" */
    user[user_name_length] = '\0';
  }
  else {
    strncpy(user, value + 1, MAX_STR_LEN);       /* Format: "~user" */
    if (strlen(value + 1) > MAX_STR_LEN)
      user[MAX_STR_LEN] = '\0';
  }

  usr_pw = getpwnam(user);                       /* Get user's home dir. */
  if (usr_pw) {
    if (cp) {
      strncpy(path, usr_pw->pw_dir, path_len - strlen(cp) - 1);
      if (strlen(usr_pw->pw_dir) > path_len - strlen(cp) - 1)
	path[path_len - strlen(cp) - 1] = '\0';
      strcat(path, cp);
    }
    else {
      strncpy(path, usr_pw->pw_dir, path_len - 1);
      if (strlen(usr_pw->pw_dir) > path_len - 1)
	path[path_len - 1] = '\0';
    }
    return 1;
  }

  return 0;
}

/* Remove separator characters from `str', e.g. `PARAM_NAME' -> `PARAMNAME' */
static void remove_word_separator(char *str, int separator,
				  char *str_no_separator, 
				  int str_no_separator_len)
{
  int i;
  char *cp;
  
  for (i = 0, cp = str; *cp && i < str_no_separator_len - 1; cp++)
    if (*cp != separator) {
      str_no_separator[i] = *cp;
      i++;
    }
  str_no_separator[i] = '\0';
}

/* Interpret and return appropriate values for suffix letters used with 
   integer and double values. The following formats are identified:

   G - Giga    = 1024^3 (for int) or 10^9 (for double)
   M - Mega    = 1024^2 (for int) or 10^6 (for double)
   k - Kilo    = 1024 (for int) or 10^3 (for double)

   m - Milli   = 10^-3 (only for double)
   u - Micro   = 10^-6 (only for double)
   n - Nano    = 10^-9 (only for double)

   This function returns -1 if the suffix is not identified or it
   is not compatible with the given format. 
   */

static double get_suffix_value(int suffix, int format)
{  
  double value = -1;

  switch (suffix) {
  case 'G':
    if (format == SUFFIX_FORMAT_INTEGER)
      value = 1024 * 1024 * 1024;
    else 
      value = 1000 * 1000 * 1000;
    break;

  case 'M':
    if (format == SUFFIX_FORMAT_INTEGER)
      value = 1024 * 1024;
    else 
      value = 1000 * 1000;
    break;

  case 'k':
    if (format == SUFFIX_FORMAT_INTEGER)
      value = 1024;
    else 
      value = 1000;
    break;

  case 'm':
    if (format == SUFFIX_FORMAT_DOUBLE)
      value = (double) 1 / 1000;
    break;

  case 'u':
    if (format == SUFFIX_FORMAT_DOUBLE)
      value = (double) 1 / (1000 * 1000);
    break;

  case 'n':
    if (format == SUFFIX_FORMAT_DOUBLE)
      value = (double) 1 / (1000 * 1000 * 1000);
    break;

  default:
    break;
  }

  return value;
}

/* Replace all the occurrences of `current_char' in `str' with `new_char'. */
static void replace_character(char *str, int current_char, int new_char)
{
  char *cp;

  assert(str != NULL);

  if ((cp = strchr(str, current_char)))
    for (; *cp; cp++)
      if (*cp == current_char)
	*cp = new_char;
}
