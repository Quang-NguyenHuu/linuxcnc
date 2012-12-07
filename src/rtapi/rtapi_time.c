/********************************************************************
* Description:  rtapi_time.c
*               This file, 'rtapi_time.c', implements the timing-
*               related functions for realtime modules.  See rtapi.h
*               for more info.
********************************************************************/

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"	// these functions

#ifndef MODULE  // kernel threads systems have own timer functions
#  ifdef RTAPI
#    include <time.h>		// clock_getres(), clock_gettime()
#  else  /* ULAPI */
#    include <sys/time.h>	// gettimeofday()
#  endif /* RTAPI */
#endif


// find a useable time stamp counter
#ifndef HAVE_RTAPI_GET_CLOCKS_HOOK  // only if thread system uses the default
#ifdef MSR_H_USABLE
#include <asm/msr.h>
#elif defined(__i386__) || defined(__x86_64__)
#define rdtscll(val) \
         __asm__ __volatile__("rdtsc" : "=A" (val))
#else
#warning No implementation of rtapi_get_clocks available
#define rdtscll(val) (val)=0
#endif
#endif /* HAVE_RTAPI_GET_CLOCKS_HOOK */


#ifdef RTAPI  /* hide most functions from ULAPI */

int period = 0;
#ifndef BUILD_SYS_USER_DSO
long int max_delay = DEFAULT_MAX_DELAY;
#endif

// Actual number of counts of the periodic timer
unsigned long timer_counts;

#ifdef HAVE_RTAPI_CLOCK_SET_PERIOD_HOOK
void rtapi_clock_set_period_hook(long int nsecs, RTIME *counts, 
				 RTIME *got_counts);
#endif

#ifdef BUILD_SYS_USER_DSO
long int rtapi_clock_set_period(long int nsecs) {
#ifndef RTAPI_TIME_NO_CLOCK_MONOTONIC
    struct timespec res = { 0, 0 };
#endif

    if (nsecs == 0)
	return period;
    if (period != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "attempt to set period twice\n");
	return -EINVAL;
    }

#ifdef RTAPI_TIME_NO_CLOCK_MONOTONIC
    period = nsecs;
#else
    clock_getres(CLOCK_MONOTONIC, &res);
    period = (nsecs / res.tv_nsec) * res.tv_nsec;
    if (period < 1)
	period = res.tv_nsec;

    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rtapi_clock_set_period (res=%ld) -> %d\n", res.tv_nsec,
		    period);
#endif  /* ! RTAPI_TIME_NO_CLOCK_MONOTONIC */

    return period;
}
#else  /* BUILD_SYS_KBUILD  */
long int rtapi_clock_set_period(long int nsecs) {
    RTIME counts, got_counts;

    if (nsecs == 0) {
	/* it's a query, not a command */
	return rtapi_data->timer_period;
    }
    if (rtapi_data->timer_running) {
	/* already started, can't restart */
	return -EINVAL;
    }
    /* limit period to 2 micro-seconds min, 1 second max */
    if ((nsecs < 2000) || (nsecs > 1000000000L)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERR: clock_set_period: %ld nsecs,  out of range\n",
	    nsecs);
	return -EINVAL;
    }

    /* kernel thread systems should init counts, timer_counts and
       rtapi_data->timer_period using their own timer functions */
#ifdef HAVE_RTAPI_CLOCK_SET_PERIOD_HOOK
    rtapi_clock_set_period_hook(nsecs, &counts, &got_counts);
    timer_counts = got_counts;
#endif

    rtapi_print_msg(RTAPI_MSG_DBG,
		    "RTAPI: clock_set_period requested: %ld  actual: %ld  "
		    "counts requested: %d  actual: %d\n",
		    nsecs, rtapi_data->timer_period,
		    (int)counts, (int)got_counts);

    rtapi_data->timer_running = 1;
    max_delay = rtapi_data->timer_period / 4;
    return rtapi_data->timer_period;
}
#endif  /* BUILD_SYS_KBUILD  */

#endif /* RTAPI */

/* The following functions are common to both RTAPI and ULAPI */

#ifdef HAVE_RTAPI_GET_TIME_HOOK
long long int rtapi_get_time_hook(void);

long long int rtapi_get_time(void) {
    return rtapi_get_time_hook();
}
#elif defined(RTAPI)
long long int rtapi_get_time(void) {

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

#else /* ULAPI */
long long rtapi_get_time(void)
{
	struct timeval tv;
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ulapi get_time\n");
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}
#endif /* HAVE_RTAPI_GET_TIME_HOOK */

#ifdef HAVE_RTAPI_GET_CLOCKS_HOOK
long long int rtapi_get_clocks_hook(void);
#endif

long long int rtapi_get_clocks(void) {
#ifndef HAVE_RTAPI_GET_CLOCKS_HOOK
    long long int retval;

    rdtscll(retval);
    return retval;
#else
    return rtapi_get_clocks_hook();
#endif  /* HAVE_RTAPI_GET_CLOCKS_HOOK */
}


/*  these are only found in kernel thread system modules  */
#ifdef MODULE
void rtapi_delay(long int nsec)
{
    if (nsec > max_delay) {
	nsec = max_delay;
    }
    udelay(nsec / 1000);
}

long int rtapi_delay_max(void)
{
    return max_delay;
}

EXPORT_SYMBOL(rtapi_clock_set_period);
EXPORT_SYMBOL(rtapi_get_time);
EXPORT_SYMBOL(rtapi_get_clocks);
EXPORT_SYMBOL(rtapi_delay);
EXPORT_SYMBOL(rtapi_delay_max);
#endif  /* MODULE */
