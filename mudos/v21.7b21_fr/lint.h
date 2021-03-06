/*
 * lint.h
 *
 * This file is now exclusively for prototypes for system functions where
 * we haven't found the right header yet ...
 *
 * The goal is to have as little as possible in this file
 */

#ifndef LINT_H_
#define LINT_H_

#include "std.h"

#if defined(_SEQUENT_) || defined(SVR4)
#  include <malloc.h>
#endif

/*******************************************************
 *  Declarations and/or prototypes for lib functions.  *
 *******************************************************/

#ifdef USE_POSIX_SIGNALS
   int port_sigblock();
   int port_sigmask();

#  ifndef _M_UNIX
     void (*port_signal()) ();
#  endif

   int port_sigsetmask();
#endif

#ifdef NeXT
   int _setjmp();
   void _longjmp();
#endif

/* SunOS is missing alot of prototypes ... */
#if defined(sun) && !defined(SunOS_5) && defined(__STDC__)
#  ifdef BUFSIZ
     int fprintf(FILE *, char *,...);
     int fputs(char *, FILE *);
     int fputc(char, FILE *);
     int fwrite(char *, int, int, FILE *);
     int fread(char *, int, int, FILE *);
#  endif
   int printf(char *,...);
   int sscanf(char *, char *,...);
   void perror(char *);
/* Dseven says SunOS has a mkdir() prototype now.
   int mkdir PROT((const char *, unsigned short));
*/
   int system PROT((char *));
   int atoi PROT((const char *));
   void abort PROT(());
/* configure now finds the qsort prototype for sun
   void qsort PROT((char *, int, int, int (*) ()));
*/
   int setsockopt PROT((int, int, int, char *, int));
   int fseek PROT_STDIO((FILE *, long, int));
   int wait PROT((int *));
   unsigned int alarm PROT((unsigned int));
   int ioctl PROT((int,...));
   extern int rename PROT((char *, char *));
   int readlink PROT((char *, char *, int));
   int symlink PROT((char *, char *));
   extern int fchmod PROT((int, int));
#endif

#if (defined(SVR4) && !defined(sgi)) || defined(__386BSD__)
   void exit PROT((int));
#endif

#if !defined(SunOS_5) && !defined(NeXT)
#  ifndef sgi
#    ifndef LATTICE
       int fclose PROT_STDIO((FILE *));
#    endif
     int pclose PROT_STDIO((FILE *));
#  endif

#  if !defined(LATTICE) && !defined(sgi)
     int fflush PROT_STDIO((FILE *));
     int fclose PROT_STDIO((FILE *));
#  endif

   int pipe PROT((int *));
   int dup2 PROT((int, int));

#  ifndef LATTICE
     int close PROT((int));
#  endif
#  ifndef sgi
     int _filbuf();
#  endif

#  ifdef sun
     char *_crypt PROT((char *, char *));
#  endif

#  ifdef DRAND48
#    ifndef sgi
       double drand48 PROT((void));
       void srand48 PROT((long));
#    endif
#  endif
#  ifdef RANDOM
     long random PROT((void));
#  endif

/* SGI is missing some prototypes in the .h files */
#ifdef LINT
#  ifdef sgi
     void ualarm PROT((int, int));
#  endif
#endif

#endif

#endif
