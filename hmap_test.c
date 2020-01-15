/* BSD 2-Clause License
 *
 * Copyright (c) 2020, Wojciech Owczarek
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
 * @file   hmap_test.c
 * @date   Fri Jan 10 23:27:00 2020
 *
 * @brief  hash map implementation test code
 *
 */

/* because clock_gettime */
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#include "hmap.h"

// HMAP_MIN_LOG2SIZE 5
// HMAP_DEF_LOG2SIZE 5
// HMAP_DEF_GROW_LOAD 0.7
// HMAP_DEF_SHRINK_LOAD 0.25
// HMAP_DEF_BATCHSIZE 128
// HMAP_DEF_MAX_OFFSET_MULT 1


/* hmInitCustom: log2size, growload, shrinkload, maxoffmult, batchsize */

/* TODO: make this parameters */

/* map defaults */
//#define INIT_MAP(map) hmInitCustom(map, 5, 0.9, 0.25, 1, 4)
//#define INIT_MAP(map) hmInitSize(map,10000000)
#define INIT_MAP(map) if(itemcount > 0) hmInitSize(map, itemcount); else hmInit(map)

/* constants */
#define TESTSIZE 1000
#define KEEPSIZE 20

/* basic duration measurement macros */
#define DUR_INIT(name) unsigned long long name##_delta; struct timespec name##_t1, name##_t2;
#define DUR_START(name) clock_gettime(CLOCK_MONOTONIC,&name##_t1);
#define DUR_END(name) clock_gettime(CLOCK_MONOTONIC,&name##_t2); name##_delta = (name##_t2.tv_sec * 1000000000 + name##_t2.tv_nsec) - (name##_t1.tv_sec * 1000000000 + name##_t1.tv_nsec);
#define DUR_PRINT(name, msg) fprintf(stderr, "%s: %llu ns\n", msg, name##_delta);
#define DUR_EPRINT(name, msg) DUR_END(name); fprintf(stderr, "%s: %llu ns\n", msg, name##_delta);

enum {
	BENCH_NONE,
	BENCH_INSERT,
	BENCH_REMOVE,
	BENCH_SEARCH,
	BENCH_INC_SEARCH,
	BENCH_DEC_SEARCH
};

/* generate an array of sequential uint32s */
static uint32_t* contArrayU32(const uint32_t count) {

    int i;

    uint32_t* ret = malloc(count * sizeof(uint32_t));

    for(i = 0; i < count; i++) {
	ret[i] = i;
    }

    return ret;

}

/* generate a Fisher-Yates shuffled array of n uint32s */
static uint32_t* randArrayU32(const uint32_t count) {

    int i;
    struct timeval t;

    /* good idea oh Lord */
    gettimeofday(&t, NULL);
    /* of course it's a good idea! */
    srand(t.tv_sec + t.tv_usec);

    uint32_t* ret = malloc(count * sizeof(uint32_t));

    for(i = 0; i < count; i++) {
	ret[i] = i;
    }

    for(i = 0; i < count; i++) {
	uint32_t j = i + rand() % (count - i);
	uint32_t tmp = ret[j];
	ret[j] = ret[i];
	ret[i] = tmp;
    }

    return ret;

}

static void usage() {

    fprintf(stderr, "hmap_test (c) 2020: Wojciech Owczarek, a simple hash map implementation\n\n"
	   "usage: hmap_test [-n NUMBER] [-N NUMBER] [-r NUMBER] [-c] [-s] [-m] [-e]\n"
	   "                 [-l] [-o] [-i NUMBER]\n"
	   "\n"
	   "-c              Insert sequential keys rather than random\n"
	   "-N NUMBER       Set minimum hash map size to fit N items, default 32 slots\n"
	   "-n NUMBER       Number of keys to insert into map, default %d\n"
	   "-r NUMBER       Number of entries to leave in the map after removal, default %d\n"
	   "-s              Test insertion only, generate CSV output on stdout\n"
	   "-m              Test removal only, CSV output to stdout\n"
	   "-e              Test search only, CSV output to stdout\n"
	   "-l              Test incremental search only (during insertion), CSV output to stdout\n"
	   "-o              Test decremental search only (during removal), CSV output to stdout\n"
	   "-i NUMBER       CSV log output interval, default every 1000 nodes,  unless\n"
	   "                1000 < 1%% item count, then 1%% item count is used.\n"
	   "\n", TESTSIZE, KEEPSIZE);

}

static void runBench(Hmap *map, const int benchtype, const int testsize, int testinterval, uint32_t *iarr, uint32_t *rarr, uint32_t *sarr, const bool sequential) {

    DUR_INIT(test);
    int found = 0;
    int i;

    switch(benchtype) {

	case BENCH_REMOVE:
	case BENCH_SEARCH:
	case BENCH_DEC_SEARCH:

	    fprintf(stderr, "Inserting %d %s keys... ", testsize, sequential ? "sequential" : "random");
	    fflush(stderr);
	    for(i = 0; i < testsize; i++) {
		hmPut(map, iarr[i], i+1);
	    }
	    fprintf(stderr, "done.\n");

	    break;

	default:
	    break;

    }

    switch(benchtype) {

	case BENCH_INSERT:

	    fprintf(stderr, "Generating CSV output for insertion of %d %s keys... ", testsize, sequential ? "sequential" : "random");
	    fflush(stderr);

	    fprintf(stdout, "node_count,ns_per_insertion\n");

	    for (int j = 0; j < testsize; j += testinterval) {

		DUR_START(test);
		for(i = j; i < j + testinterval; i++) {
		    hmPut(map, iarr[i], i+1);
		}
		DUR_END(test);
		fprintf(stdout, "%d,%llu\n", ((j + testinterval) > testsize) ? testsize : j + testinterval, test_delta / testinterval);

	    }

	    fprintf(stderr, "done.\n");

	    break;

	case BENCH_REMOVE:

	    fprintf(stderr, "Generating CSV output for removal of %d %s keys... ", testsize, sequential ? "sequential" : "random");
	    fflush(stderr);

	    fprintf(stdout, "node_count,ns_per_removal\n");

	    for (int j = 0; j < testsize; j += testinterval) {

		DUR_START(test);
		for(i = j; i < j + testinterval; i++) {
		    hmRemove(map, rarr[i]);
		}
		DUR_END(test);
		fprintf(stdout, "%d,%llu\n", ((j + testinterval) > testsize) ? testsize : j + testinterval, test_delta / testinterval);

	    }

	    fprintf(stderr, "done.\n");

	    break;

	case BENCH_SEARCH:

	    found = 0;
	    fprintf(stderr, "Generating CSV output for search of %d %s keys... ", testsize, sequential ? "sequential" : "random");
	    fflush(stderr);

	    fprintf(stdout, "iterations,ns_per_search\n");

	    for (int j = 0; j < testsize; j += testinterval) {

		DUR_START(test);
		for(i = j; i < j + testinterval; i++) {

		    HmapEntry *n = hmGet(map, sarr[i]);

		    if(n != NULL && n->key == sarr[i]) {
			found++;
		    }

		}
		DUR_END(test);
		fprintf(stdout, "%d,%llu\n", ((j + testinterval) > testsize) ? testsize : j + testinterval, test_delta / testinterval);

	    }

	    fprintf(stderr, "%d found.\n", found);

	    break;

	case BENCH_INC_SEARCH:

	    found = 0;

	    fprintf(stderr, "Generating CSV output for incremental search during insertion of %d %s keys... ", testsize, sequential ? "sequential" : "random");
	    fflush(stdout);

	    fprintf(stdout, "node_count,ns_per_search\n");

	    for (int j = 0; j < testsize; j += testinterval) {

		i = 0;
		for(i = j; i < j + testinterval; i++) {
		    hmPut(map, iarr[i], i+1);
		}

		uint32_t larr[testinterval];

		/* __almost__ a n ideal random n-sample from all of current iarr */
		for(int k = 0; k < testinterval; k++) {
		    larr[k] = iarr[sarr[ j + k ] % i];
		}

		DUR_START(test);
		for(int k = 0; k < testinterval; k++) {
		    HmapEntry *n = hmGet(map, larr[k]);

		    if(n != NULL && n->key == larr[k]) {
			found++;
		    }
		}
		DUR_END(test);
		fprintf(stdout, "%d,%llu\n", ((j + testinterval) > testsize) ? testsize : j + testinterval, test_delta / testinterval);
	    }

	    fprintf(stderr, "%d found.\n", found);

	    break;

	case BENCH_DEC_SEARCH:

	    found = 0;

	    fprintf(stderr, "Generating CSV output for search during removal of %d %s keys... ", testsize, sequential ? "sequential" : "random"); 
	    fflush(stdout);

	    fprintf(stdout, "node_count,ns_per_search\n");

	    for (int j = 0; j < testsize; j += testinterval) {

		DUR_START(test);
		for(int i = j; i < j + testinterval; i++) {
		    HmapEntry *n = hmGet(map, rarr[i]);

		    if(n != NULL && n->key == rarr[i]) {
			found++;
		    }
		}
		DUR_END(test);
		fprintf(stdout, "%d,%llu\n", ((j + testinterval) > testsize) ? testsize : j + testinterval, test_delta / testinterval);

		for(i = j; i < j + testinterval; i++) {
		    hmRemove(map, rarr[i]);
		}

	    }

	    fprintf(stderr, "%d found.\n", found);


	    break;

	case BENCH_NONE:
	default:
	    break;
    }


}

int main(int argc, char **argv) {

    int c;
    int i;
    int itemcount = 0;
    int testsize = TESTSIZE;
    int keepsize = 0;
    int testinterval = 0;
    uint32_t found = 0;
    int bench = BENCH_NONE;
    char obuf[2001];
    char *buf = obuf;
    bool sequential = false;
    Hmap map;
    uint32_t *iarr, *rarr, *sarr;
    DUR_INIT(test);

    memset(obuf, 0, sizeof(obuf));

	while ((c = getopt(argc, argv, "?hn:r:N:csmeloi:")) != -1) {

	    switch(c) {
		case 'n':
		    testsize = atoi(optarg);
		    if(testsize <= 0) {
			testsize = TESTSIZE;
		    }
		    break;
		case 'N':
		    itemcount = atoi(optarg);
		    if(itemcount <= 0) {
			itemcount = 0;
		    }
		    break;
		case 'r':
		    keepsize = atoi(optarg);
		    if(keepsize < 0) {
			keepsize = 0;
		    }
		    break;
		case 'c':
		    sequential = true;
		    break;
		case 's':
		    bench = BENCH_INSERT;
		    break;
		case 'm':
		    bench = BENCH_REMOVE;
		    break;
		case 'e':
		    bench = BENCH_SEARCH;
		    break;
		case 'l':
		    bench = BENCH_INC_SEARCH;
		    break;
		case 'o':
		    bench = BENCH_DEC_SEARCH;
		    break;
		case 'i':
		    testinterval = atoi(optarg);
		    if(testinterval <= 0) {
			testinterval = testsize / 100;
		    }
		    break;
		case '?':
		case 'h':
		default:
		    usage();
		    return -1;
	    }
	}

    INIT_MAP(&map);

    if(keepsize > testsize) {
	keepsize = testsize;
    }

    if(testinterval == 0) {
	testinterval = 1000;
    }

    if((testsize / testinterval) < 100) {
	testinterval = testsize / 100;
    }

    if(testinterval < 2) {
	testinterval = 2;
    }

    fprintf(stderr, "Generating %d size %s insertion, removal and search key arrays... ", testsize, sequential ? "sequential" : "random");
    fflush(stderr);

    if(sequential) {
	iarr = contArrayU32(testsize);
	sarr = contArrayU32(testsize);
	rarr = contArrayU32(testsize);
    } else {
	iarr = randArrayU32(testsize);
	rarr = randArrayU32(testsize);
	sarr = randArrayU32(testsize);
    }

    fprintf(stderr, "done.\n");

    if(bench != BENCH_NONE) {
	runBench(&map, bench, testsize, testinterval, iarr, rarr, sarr, sequential);
	goto cleanup;
    }

    fprintf(stderr, "Inserting %d %s keys... ", testsize, sequential ? "sequential" : "random");
    fflush(stderr);

    DUR_START(test);
    for(i = 0; i < testsize; i++) {
	hmPut(&map, iarr[i], i+1);
    }
    DUR_END(test);
    fprintf(stderr, "done.\n");

    buf += sprintf(buf, "+---------------------------------+-------------+---------+\n");
    buf += sprintf(buf, "| Test                            | result      | unit    |\n");
    buf += sprintf(buf, "+---------------------------------+-------------+---------+\n");
    buf += sprintf(buf, "| Insertion, count %-10d     "   "| %-11llu "  "| ns/key  |\n", testsize, test_delta / testsize);
    buf += sprintf(buf, "| Insertion, rate                 | %-11.0f "  "| nodes/s |\n", (1000000000.0 / test_delta) * testsize );

    fprintf(stderr, "Finding all %d keys in %s order... ", testsize, sequential ? "sequential" : "random");
    fflush(stderr);

    found = 0;
    DUR_START(test);
    for(i = 0; i < testsize; i++) {

	HmapEntry *n = hmGet(&map, sarr[i]);

	if(n != NULL && n->key == sarr[i]) {
	    found++;
	}

    }
    DUR_END(test);
    fprintf(stderr, "%d found.\n", found);
    buf += sprintf(buf, "| Random search, count %-10d "   "| %-11llu "  "| ns/key  |\n", testsize, test_delta / testsize);
    buf += sprintf(buf, "| Random search, rate             "   "| %-11.0f "  "| hit/s   |\n", (1000000000.0 / test_delta) * testsize);

    fprintf(stderr, "Finding all %d keys in sequential order... ", testsize);
    fflush(stderr);

    found = 0;
    DUR_START(test);
    for(i = 0; i < testsize; i++) {

	HmapEntry *n = hmGet(&map, i);

	if(n != NULL && n->key == i) {
	    found++;
	}

    }
    DUR_END(test);
    fprintf(stderr, "%d found.\n", found);
    buf += sprintf(buf, "| Seq search, count %-10d    "   "| %-11llu "  "| ns/key  |\n", testsize, test_delta / testsize);
    buf += sprintf(buf, "| Seq search, rate                "   "| %-11.0f "  "| hit/s   |\n", (1000000000.0 / test_delta) * testsize);

    hmFree(&map);
    INIT_MAP(&map);

    fprintf(stderr, "Re-adding %d keys in %s order... ", testsize, sequential ? "sequential" : "random");
    fflush(stderr);
    for(i = 0; i < testsize; i++) {
	hmPut(&map, iarr[i], i+1);
    }
    fprintf(stderr, "done.\n");

    fprintf(stderr, "Removing all %d keys in sequential order... ", testsize);
    fflush(stderr);

    DUR_START(test);
    for(i = 0; i < testsize; i++) {
	hmRemove(&map, i);
    }
    DUR_END(test);
    fprintf(stderr, "done.\n");
    buf += sprintf(buf, "| Seq removal, count %-10d   "   "| %-11llu "  "| ns/key  |\n", testsize, test_delta / testsize);
    buf += sprintf(buf, "| Seq removal, rate               | %-11.0f "  "| nodes/s |\n", (1000000000.0 / test_delta) * testsize );

    fprintf(stderr, "Re-adding %d keys in sequential order... ", testsize);
    fflush(stderr);
    DUR_START(test);
    for(i = 0; i < testsize; i++) {
	hmPut(&map, i, i+1);
    }
    DUR_END(test);
    fprintf(stderr, "done.\n");
    buf += sprintf(buf, "| Seq insertion, count %-10d "   "| %-11llu "  "| ns/key  |\n", testsize, test_delta / testsize);
    buf += sprintf(buf, "| Seq insertion, rate             | %-11.0f "  "| nodes/s |\n", (1000000000.0 / test_delta) * testsize );

    fprintf(stderr, "Removing all %d keys in sequential order again... ", testsize);
    fflush(stderr);
    for(i = 0; i < testsize; i++) {
	hmRemove(&map, i);
    }
    fprintf(stderr, "done.\n");

    fprintf(stderr, "Re-adding %d keys in %s order... ", testsize, sequential ? "sequential" : "random");
    fflush(stderr);
    for(i = 0; i < testsize; i++) {
	hmPut(&map, iarr[i], i+1);
    }
    fprintf(stderr, "done.\n");

    if(keepsize == 0) {
	fprintf(stderr, "Removing all %d keys in %s order... ", testsize, sequential ? "sequential" : "random");
	fflush(stderr);

	DUR_START(test);
	for(i = 0; i < testsize; i++) {
	    hmRemove(&map, rarr[i]);
	}
	DUR_END(test);
	fprintf(stderr, "done.\n");
	buf += sprintf(buf, "| Removal, count %-10d       "   "| %-11llu "  "| ns/key  |\n", testsize - keepsize, (testsize <= keepsize) ? 0 : test_delta / (testsize - keepsize));
	buf += sprintf(buf, "| Removal, rate                   | %-11.0f "  "| nodes/s |\n", (1000000000.0 / test_delta) * testsize );
    } else if(keepsize < testsize) {

	fprintf(stderr, "Removing %d keys in %s order to leave %d keys... ", testsize - keepsize, sequential ? "sequential" : "random", keepsize);
	fflush(stderr);

	DUR_START(test);
	for(i = 0; i < testsize; i++) {

	    if(rarr[i] >= keepsize) {
		hmRemove(&map, rarr[i]);
	    }
	}
	DUR_END(test);
	fprintf(stderr, "done.\n");
	buf += sprintf(buf, "| Removal, count %-10d       "   "| %-11llu "  "| ns/key  |\n", testsize - keepsize, (testsize <= keepsize) ? 0 : test_delta / (testsize - keepsize));
	buf += sprintf(buf, "| Removal, rate                   | %-11.0f "  "| nodes/s |\n", (1000000000.0 / test_delta) * testsize );
    }

    buf += sprintf(buf, "+---------------------------------+-------------+---------+\n");

    fprintf(stdout, "\nTest results:\n\n%s\n", obuf);
    fflush(stderr);

    if(map.count > 0) {
	fprintf(stdout, "Final map contents:\n");
	hmDump(&map, false);
    }

cleanup:
    fprintf(stderr, "Cleaning up... ");

    fflush(stderr);

    hmFree(&map);

    free(iarr);
    free(rarr);
    free(sarr);

    fprintf(stderr, "done.\n");

    return 0;

}
