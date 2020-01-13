/* BSD 2-Clause License
 *
 * Copyright (c) 2018, Wojciech Owczarek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file   duration.h
 * @date   Sun Apr15 13:40:12 2018
 *
 * @brief  Simple duration measurement and conversion macros
 *
 */

#ifndef __X_DURATION_H_
#define __X_DURATION_H_

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L /* because clock_gettime */
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define HUMANTIME_WIDTH 31

/* helper data */
char _humantime [HUMANTIME_WIDTH];
char _humantime1[HUMANTIME_WIDTH];
char _humantime2[HUMANTIME_WIDTH];
char _humantime3[HUMANTIME_WIDTH];
char _humantime4[HUMANTIME_WIDTH];
char _humantime5[HUMANTIME_WIDTH];

/* basic duration measurement macros */

/* initialise a duration measurement instance of given name */
#define DUR_INIT(name) unsigned long long name##_delta; struct timespec name##_t1, name##_t2;
/* start duration measurement */
#define DUR_START(name) clock_gettime(CLOCK_MONOTONIC,&name##_t1);
/* end duration measurement */
#define DUR_END(name) clock_gettime(CLOCK_MONOTONIC,&name##_t2); name##_delta = (name##_t2.tv_sec * 1000000000 + name##_t2.tv_nsec) - (name##_t1.tv_sec * 1000000000 + name##_t1.tv_nsec);
/* print duration in nanoseconds with a message prefix*/
#define DUR_PRINT(name, msg) fprintf(stderr, "%s: %llu ns\n", msg, name##_delta);
/* end measurement and print duration as above */
#define DUR_EPRINT(name, msg) DUR_END(name); fprintf(stderr, "%s: %llu ns\n", msg, name##_delta);

/* get duration in human time */
#define DUR_HTIME(var,str) ( memset(str, 0, HUMANTIME_WIDTH),\
	    (void) ( ((var) > 1000000000) ? snprintf(str, HUMANTIME_WIDTH, "%.09f s", (var) / 1000000000.0) :\
	    ((var) > 1000000) ? snprintf(str, HUMANTIME_WIDTH, "%.06f ms", (var) / 1000000.0) :\
	    ((var) > 1000) ? snprintf(str, HUMANTIME_WIDTH, "%.03f us", (var) / 1000.0) :\
	    snprintf(str, HUMANTIME_WIDTH, "%.0f ns", (var) / 1.0 )),\
	    str )
#define DUR_HUMANTIME(var)  DUR_HTIME((var),_humantime)
#define DUR_HUMANTIME1(var) DUR_HTIME((var),_humantime1)
#define DUR_HUMANTIME2(var) DUR_HTIME((var),_humantime2)
#define DUR_HUMANTIME3(var) DUR_HTIME((var),_humantime3)
#define DUR_HUMANTIME4(var) DUR_HTIME((var),_humantime4)
#define DUR_HUMANTIME5(var) DUR_HTIME((var),_humantime5)

/* end measurement and print duration in human time */
#define DUR_EHUMANTIME(name, msg) DUR_END(name) fprintf(stderr, "%s: %s\n", msg, DUR_HUMANTIME(name##_delta));

/* iters/sec and time/iter statistics */
#define DUR_ITERSTATS(name, count) \
    fprintf(stderr, "%u iter, %.0f iter/s, %s/iter\n",\
	count, (1000000000.0 / name##_delta) * count, DUR_HUMANTIME(name##_delta / count));
/* same but end measurement at the same time */
#define DUR_EITERSTATS(name, count) \
    DUR_END(name) fprintf(stderr, "%u iter, %.0f iter/s, %s/iter\n",\
	count, (1000000000.0 / name##_delta) * count, DUR_HUMANTIME(name##_delta / count));

#endif /* __X_DURATION_H_ */
