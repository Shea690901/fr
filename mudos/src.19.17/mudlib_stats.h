/*
 * mudlib_stats.h
 * created by: Erik Kay
 * last modified: 11/1/92
 * this file is a replacement for wiz_list.h and all of its purposes
 * the idea is that it will be more domain based, rather than user based
 * and will be a little more general purpose than wiz_list was
 */

#ifndef _MUDLIB_STATS_H
#define _MUDLIB_STATS_H

typedef struct _mstats
{
  /* the name of the stat group */
  char *name;
  int length; 
  /* the next data element in the linked list */
  struct _mstats *next;
  /* statistics */
  int moves;
  int heart_beats;
  int size_array;
  int errors;
  int objects;
} mudlib_stats_t;


typedef struct _statgroup
{
  mudlib_stats_t *domain;
  mudlib_stats_t *author;
} statgroup_t;


#define DOMAIN_STATS_FILE_NAME "domain_stats"
#define AUTHOR_STATS_FILE_NAME "author_stats"

#endif
