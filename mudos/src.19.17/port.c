#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef LATTICE
#include <time.h>
#include <times.h>
#else
#ifdef _M_UNIX
#include <time.h>
#endif
#include <sys/time.h>
#endif
#include <signal.h>
#ifdef _SEQUENT_
#include <usclkc.h>
#endif
#ifdef LATTICE
#include <nsignal.h>
#endif

#include "config.h"
#include "lint.h"

/* for get_cpu_times() */
#ifdef GET_PROCESS_STATS
#include <sys/procstats.h>
#endif
#ifdef TIMES
#include <sys/times.h>
#endif
#ifdef RUSAGE
#include <sys/resource.h>
#endif

/* get a value for CLK_TCK for use by times() */
#if (defined(TIMES) && !defined(RUSAGE))
/* this may need #ifdef'd to handle different types of machines */
#include <limits.h>
#endif

#ifdef sun
time_t time PROT((time_t *));
#endif

#if defined(SunOS_5) || defined(__386BSD__)
#include <stdlib.h>
#endif

/*
 * Return a pseudo-random number in the range 0 .. n-1
 */
int random_number(n)
    int n;
{
#ifdef RANDOM
    return random() % n;
#else /* RANDOM */
#ifdef DRAND48
    return (int)(drand48() * n);
#else /* DRAND48 */
    extern int current_time;

    return current_time % n; /* You really don't want to use this method */
#endif /* DRAND48 */
#endif /* RANDOM */
}

/*
 * The function time() can't really be trusted to return an integer.
 * But MudOS uses the 'current_time', which is an integer number
 * of seconds. To make this more portable, the following functions
 * should be defined in such a way as to return the number of seconds since
 * some chosen year. The old behaviour of time(), is to return the number
 * of seconds since 1970.
 */

int get_current_time() {
    return (int)time(0l);	/* Just use the old time() for now */
}

char *time_string(t)
    int t;
{
    return (char *)ctime((time_t *)&t);
}

/*
 * Initialize the microsecond clock.
 */
void
init_usec_clock()
{
#ifdef _SEQUENT_
	usclk_init();
#endif
}

/*
 * Get a microsecond clock sample.
 */
void
get_usec_clock(sec, usec)
	long *sec, *usec;
{
#ifdef HAS_GETTIMEOFDAY
	struct timeval tv;
        gettimeofday(&tv, NULL);
	*sec = tv.tv_sec;
	*usec = tv.tv_usec;
#else
#ifdef _SEQUENT_
	*sec = 0;
	*usec = GETUSCLK();
#else
#ifdef LATTICE
        unsigned int clock[2];

        if (timer(clock)) {
            *sec = time(0); *usec = 0;
        } else {
            *sec = clock[0]; *usec = clock[1];
        }
#else
	*sec = time(0);
	*usec = 0;
#endif
#endif
#endif
}

#ifdef USE_POSIX_SIGNALS
int
port_sigblock(mask)
	sigset_t mask;
{
	sigset_t omask;
	sigprocmask(SIG_BLOCK, &mask, &omask);
	return (omask);
}

int
port_sigmask(sig)
	int sig;
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, sig);
	return (set);
}

void
(*port_signal(sig, func))()
	int sig;
	void (*func)();
{
	struct sigaction act, oact;
	act.sa_handler = func;
	act.sa_mask = 0;
	act.sa_flags = 0;
	if (sigaction(sig, &act, &oact) == -1)
		return ((void (*)())-1);
	return (oact.sa_handler);
}

int
port_sigsetmask(mask)
	sigset_t mask;
{
	sigset_t omask;
	sigprocmask(SIG_SETMASK, &mask, &omask);
	return (omask);
}
#endif

int
get_cpu_times(secs, usecs)
	unsigned long *secs, *usecs;
{
#ifdef RUSAGE
	struct rusage rus;
#endif
#if defined(TIMES) && !defined(RUSAGE)
	struct tms t;
	unsigned long total;
#endif
#ifdef GET_PROCESS_STATS
    struct process_stats ps;
#endif

#ifdef RUSAGE /* start RUSAGE */
	if (getrusage(RUSAGE_SELF, &rus) < 0) {
		return 0;
	}
    *secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
	*usecs = rus.ru_utime.tv_usec + rus.ru_stime.tv_usec;
	return 1;
#else /* end then RUSAGE */

#ifdef GET_PROCESS_STATS /* start GET_PROCESS_STATS */
	if (get_process_stats(NULL, PS_SELF, &ps, NULL) == -1) {
		return 0;
	}
	*secs = ps.ps_utime.tv_sec + ps.ps_stime.tv_sec;
	*usecs = ps.ps_utime.tv_usec + ps.ps_stime.tv_usec;
	return 1;
#else /* end then GET_PROCESS_STATS */

#ifdef TIMES /* start TIMES */
	times(&t);
	*secs = (total = t.tms_utime + t.tms_stime) / CLK_TCK;
	*usecs = ((total - (*secs * CLK_TCK)) * 1000000) / CLK_TCK;
	return 1;
#else /* end then TIMES */

#ifdef LATTICE /* start LATTICE */
        unsigned int clock[2];

        if (timer(clock))
            return 0;

        *secs = clock[0];
        *usecs = clock[1];
        return 1;
#else

	return 0;

#endif /* end LATTICE */

#endif /* end TIMES */

#endif /* end else GET_PROCESS_STATS */

#endif /* end else RUSAGE */
}

/* return the current working directory */
char *
get_current_dir(buf, max)
char *buf;
int max;
{
#if defined(NeXT)
	return getwd(buf); /* BSD */
#else
	extern char *getcwd();

	return getcwd(buf, max); /* POSIX */
#endif
}

#if defined(_AUX_SOURCE) || defined(SunOS_4)
/* for those systems without strerror() but with sys_errlist, sys_nerr */
extern char *sys_errlist[];
extern int sys_nerr;

char *
strerror(which)
int which;
{
	if ((which < 0) || (which >= sys_nerr)) {
		return "unknown error";
	} else {
		return sys_errlist[which];
	}
}
#endif /* STRERROR */
