/*
 * rc.c
 * description: runtime configuration for lpmud
 * author: erikkay@mit.edu
 * last modified: July 4, 1994 [robo]
 */

#include "config.h"
#include <stdio.h>
#if defined(__386BSD__) || defined(SunOS_5)
#include <string.h>
#endif
#include "lint.h"
#include "interpret.h"

#define MAX_LINE_LENGTH 120

static char config_str[NUM_CONFIG_STRS][MAX_LINE_LENGTH];
static int config_int[NUM_CONFIG_INTS];
static char *buff;

#define CONFIG_STR(x) config_str[x - BASE_CONFIG_STR]
#define CONFIG_INT(x) &config_int[x - BASE_CONFIG_INT]

INLINE int get_config_int (num)
int num;
{
#ifdef DEBUG
  if (num > NUM_CONFIG_INTS || num < 0) 
    {
      fatal ("Bounds error in get_config_int\n");
    }
#endif
  return config_int[num];
}

INLINE char * get_config_str (num)
     int num;
{
#ifdef DEBUG
  if (num > NUM_CONFIG_STRS || num < 0) 
    {
      fatal ("Bounds error in get_config_str\n");
    }
#endif
  return config_str[num];
}

void read_config_file (file)
     FILE * file;
{
  char str[120];
  int size = 0,len;
  buff = (char *)
	DMALLOC(MAX_LINE_LENGTH * (NUM_CONFIG_INTS + 1) * (NUM_CONFIG_STRS + 1), 92,
		"read_config_file: 1");
  strcpy(buff,"\n");
  while (1) 
    {
      if (fgets (str,120,file) == NULL)
	break;
      if (!str) break;
      len = strlen(str);
      if (len > MAX_LINE_LENGTH) {
	fprintf (stderr,"*Error in config file: line too long.\n");
	exit (-1);
      }
      if (str[0] != '#' && str[0] != '\n')
	{
	  size += len + 1;
	  if (size > (MAX_LINE_LENGTH * (NUM_CONFIG_INTS + 1) * (NUM_CONFIG_STRS + 1)))
	    buff = (char *)DREALLOC(buff, size, 93, "read_config_file: 2");
	  strcat (buff, str);
	  strcat (buff, "\n");
	}
    }
}


/*
 * If the required flag is 0, it will only give a warning if the line is
 * missing from the config file.  Otherwise, it will give an error and exit
 * if the line isn't there.
 */
void scan_config_line (start, fmt, dest, required)
     char *start;
     char *fmt;
     void *dest;
     int required;
{
  char *tmp;
  char missing_line[MAX_LINE_LENGTH];

  tmp = (char *)strchr (start,fmt[0]);
  if (tmp && (*(tmp-1) != '\n'))
    {
      scan_config_line (tmp+1,fmt,dest, required);
      return;
    }
  if (!tmp) 
    {
      strcpy (missing_line,fmt);
      tmp = (char *)strchr (missing_line,':');
      *tmp = '\0';
      if (!required)
	{
	  fprintf (stderr,"*Warning: Missing line in config file:\n\t%s\n",
		   missing_line);
	  memset(dest,0,1);
	  return;
	}
      fprintf (stderr,"*Error in config file.  Missing line:\n\t%s\n",
	       missing_line);
      exit(-1);
    }
  if (sscanf (tmp,fmt,dest) != 1)
    scan_config_line (tmp+1,fmt,dest, required);
}

void set_defaults (filename) 
     char * filename;
{
  FILE *def;
  char defaults[SMALL_STRING_SIZE];

  def = fopen(filename,"r");
  if (def) {
    fprintf(stderr,"loading config file: %s\n", filename);
  } else {
#ifdef LATTICE
    if (strchr(CONFIG_FILE_DIR, ':'))
      sprintf(defaults, "%s%s", CONFIG_FILE_DIR, filename);
    else
#endif  
      sprintf(defaults,"%s/%s",CONFIG_FILE_DIR,filename);

    def = fopen (defaults,"r");
    if (def) {
        fprintf(stderr,"loading config file: %s\n", defaults);
    }
  }
  if (!def) 
    {
      fprintf(stderr,"*Error: couldn't load config file: '%s'\n",filename);
      exit (-1);
    }
  read_config_file (def);
  
  scan_config_line(buff,"name : %[^\n]",CONFIG_STR(__MUD_NAME__),1);
  scan_config_line(buff,"address server ip : %[^\n]",CONFIG_STR(__ADDR_SERVER_IP__),1);

  scan_config_line(buff,"mudlib directory : %[^\n]",CONFIG_STR(__MUD_LIB_DIR__),1);
  scan_config_line(buff,"binary directory : %[^\n]",CONFIG_STR(__BIN_DIR__),1);

  scan_config_line(buff,"log directory : %[^\n]",CONFIG_STR(__LOG_DIR__),1);
  scan_config_line(buff,"include directories : %[^\n]",CONFIG_STR(__INCLUDE_DIRS__),1);
#ifdef SAVE_BINARIES
  scan_config_line(buff,"save binaries directory : %[^\n]",CONFIG_STR(__SAVE_BINARIES_DIR__),0);
#endif

  scan_config_line(buff,"master file : %[^\n]",CONFIG_STR(__MASTER_FILE__),1);
  scan_config_line(buff,"simulated efun file : %[^\n]",CONFIG_STR(__SIMUL_EFUN_FILE__),1);
  scan_config_line(buff,"swap file : %[^\n]",CONFIG_STR(__SWAP_FILE__),1);
  scan_config_line(buff,"debug log file : %[^\n]",CONFIG_STR(__DEBUG_LOG_FILE__),0);

  scan_config_line(buff,"default error message : %[^\n]",CONFIG_STR(__DEFAULT_ERROR_MESSAGE__),0);
  scan_config_line(buff,"default fail message : %[^\n]",CONFIG_STR(__DEFAULT_FAIL_MESSAGE__),0);
  
  scan_config_line(buff,"port number : %d\n",CONFIG_INT(__MUD_PORT__),1);
  scan_config_line(buff,"address server port : %d\n",CONFIG_INT(__ADDR_SERVER_PORT__),1);

  scan_config_line(buff,"time to clean up : %d\n",CONFIG_INT(__TIME_TO_CLEAN_UP__),1);
  scan_config_line(buff,"time to reset : %d\n",CONFIG_INT(__TIME_TO_RESET__),1);
  scan_config_line(buff,"time to swap : %d\n",CONFIG_INT(__TIME_TO_SWAP__),1);

#if 0
  /*
   * not currently used...see options.h
   */
  scan_config_line(buff,"maximum users : %d\n",CONFIG_INT(__MAX_USERS__),0);
  scan_config_line(buff,"maximum efun sockets : %d\n",CONFIG_INT(__MAX_EFUN_SOCKS__),0);
  scan_config_line(buff,"compiler stack size : %d\n",CONFIG_INT(__COMPILER_STACK_SIZE__),0);
  scan_config_line(buff,"evaluator stack size : %d\n",CONFIG_INT(__EVALUATOR_STACK_SIZE__),0);
  scan_config_line(buff,"maximum local variables : %d\n",CONFIG_INT(__MAX_LOCAL_VARIABLES__),0);
  scan_config_line(buff,"maximum call depth : %d\n",CONFIG_INT(__MAX_CALL_DEPTH__),0);
  scan_config_line(buff,"living hash table size : %d\n",CONFIG_INT(__LIVING_HASH_TABLE_SIZE__),0);
#endif

  scan_config_line(buff,"inherit chain size : %d\n",CONFIG_INT(__INHERIT_CHAIN_SIZE__),1);
  scan_config_line(buff,"maximum evaluation cost : %d\n",CONFIG_INT(__MAX_EVAL_COST__),1);

  scan_config_line(buff,"maximum array size : %d\n",CONFIG_INT(__MAX_ARRAY_SIZE__),1);
#ifndef DISALLOW_BUFFER_TYPE
  scan_config_line(buff,"maximum buffer size : %d\n",CONFIG_INT(__MAX_BUFFER_SIZE__),1);
#endif
  scan_config_line(buff,"maximum mapping size : %d\n",CONFIG_INT(__MAX_MAPPING_SIZE__),1);
  scan_config_line(buff,"maximum string length : %d\n",CONFIG_INT(__MAX_STRING_LENGTH__), 1);
  scan_config_line
    (buff,"maximum bits in a bitfield : %d\n",CONFIG_INT(__MAX_BITFIELD_BITS__),1);

  scan_config_line(buff,"maximum byte transfer : %d\n",CONFIG_INT(__MAX_BYTE_TRANSFER__),1);
  scan_config_line(buff,"maximum read file size : %d\n",CONFIG_INT(__MAX_READ_FILE_SIZE__),1);

  scan_config_line(buff,"reserved size : %d\n",CONFIG_INT(__RESERVED_MEM_SIZE__),1);

  scan_config_line(buff,"hash table size : %d\n",CONFIG_INT(__SHARED_STRING_HASH_TABLE_SIZE__),1);
  scan_config_line(buff,"object table size : %d\n",CONFIG_INT(__OBJECT_HASH_TABLE_SIZE__),1);

  FREE(buff); 
  fclose (def);

  /*
   * from options.h
   */
  config_int[__MAX_USERS__              - BASE_CONFIG_INT] = MAX_USERS;
  config_int[__MAX_EFUN_SOCKS__         - BASE_CONFIG_INT] = MAX_EFUN_SOCKS;
  config_int[__COMPILER_STACK_SIZE__    - BASE_CONFIG_INT] = COMPILER_STACK_SIZE;
  config_int[__EVALUATOR_STACK_SIZE__   - BASE_CONFIG_INT] = EVALUATOR_STACK_SIZE;
  config_int[__MAX_LOCAL_VARIABLES__    - BASE_CONFIG_INT] = MAX_LOCAL;
  config_int[__MAX_CALL_DEPTH__         - BASE_CONFIG_INT] = MAX_TRACE;
  config_int[__LIVING_HASH_TABLE_SIZE__ - BASE_CONFIG_INT] = LIVING_HASH_SIZE;
}

int get_config_item(res, arg)
struct svalue *res, *arg;
{
    int num;

    num = arg->u.number;

    if (num < RUNTIME_CONFIG_BASE || num >= RUNTIME_CONFIG_NEXT) {
        return 0;
    }

    if (num >= BASE_CONFIG_INT) {
        res->type = T_NUMBER;
        res->u.number = config_int[num - BASE_CONFIG_INT];
    } else {
        res->type = T_STRING;
        res->subtype = STRING_CONSTANT;
        res->u.string = config_str[num];
    }

    return 1;
}
