/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * timer.c
 * - timer functions
 */


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include <syslog.h>

#include "ip6config_utils.h"
#include "timer.h"


/**
 ** Timer callout functions
 **/

static void
timer_callout_process(CFRunLoopTimerRef timer_source, void * info)
{
    timer_callout_t *	callout = (timer_callout_t *)info;

    if (callout->func && callout->enabled) {
	callout->enabled = 0;
	(*callout->func)(callout->arg1, callout->arg2, callout->arg3);
    }
    return;
}

__private_extern__ timer_callout_t *
timer_callout_init()
{
    timer_callout_t * callout;

    callout = malloc(sizeof(*callout));
    if (callout == NULL)
	return (NULL);
    bzero(callout, sizeof(*callout));
    return (callout);
}

__private_extern__ void
timer_callout_free(timer_callout_t * * callout_p)
{
    timer_callout_t * callout = *callout_p;

    if (callout == NULL)
	return;

    timer_cancel(callout);
    free(callout);
    *callout_p = NULL;
    return;
}

__private_extern__ int
timer_set_relative(timer_callout_t * callout,
		   struct timeval rel_time, timer_func_t * func,
		   void * arg1, void * arg2, void * arg3)
{
    CFRunLoopTimerContext 	context =  { 0, NULL, NULL, NULL, NULL };
    CFAbsoluteTime 		wakeup_time;

    if (callout == NULL) {
	return (0);
    }
    timer_cancel(callout);
    callout->enabled = 0;
    if (func == NULL) {
	return (0);
    }
    callout->func = func;
    callout->arg1 = arg1;
    callout->arg2 = arg2;
    callout->arg3 = arg3;
    callout->enabled = 1;
    if (rel_time.tv_sec <= 0 && rel_time.tv_usec < 1000) {
	rel_time.tv_sec = 0;
	rel_time.tv_usec = 1000; /* force millisecond resolution */
    }
    wakeup_time = CFAbsoluteTimeGetCurrent() + rel_time.tv_sec
	  + ((double)rel_time.tv_usec / USECS_PER_SEC);
    context.info = callout;
    callout->timer_source
	= CFRunLoopTimerCreate(NULL, wakeup_time,
			       0.0, 0, 0,
			       timer_callout_process,
			       &context);
    my_log(LOG_DEBUG, "timer: wakeup time is (%d.%d) %g",
	    rel_time.tv_sec, rel_time.tv_usec, wakeup_time);
    my_log(LOG_DEBUG, "timer: adding timer source");
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), callout->timer_source,
		    kCFRunLoopDefaultMode);
    return (1);
}

__private_extern__ void
timer_cancel(timer_callout_t * callout)
{
    if (callout == NULL) {
	return;
    }
    callout->enabled = 0;
    callout->func = NULL;
    if (callout->timer_source) {
	my_log(LOG_DEBUG, "timer:  freeing timer source");
	CFRunLoopTimerInvalidate(callout->timer_source);
	CFRelease(callout->timer_source);
	callout->timer_source = 0;
    }
    return;
}
