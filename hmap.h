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
 * @file   hmap.h
 * @date   Fri Jan 10 23:27:00 2020
 *
 * @brief  type and function declarations for a simple hash map:
 *
 *                            - linear probing, power of 2 size, uint32_t keys
 *                            - Robin Hood with backwards shift, no tombstones
 *                            - Dynamic resizing (full migration or in batches)
 *                            - Search probe length limited to actual maximum
 *                            - Configurable load factors, minimum size, batch size
 *
 */

#ifndef HMAP_H_
#define HMAP_H_

#include <stdint.h>
#include <stdbool.h>

/* convenience constants */
#define HMAP_MIGRATE_ALL 0	/* when set as batchSize on init, map migrates / rehashes everything on shring/grow */
#define HMAP_NONE 0		/* universal zero constant */

/* a single hash map entry */
typedef struct {
    uint32_t key;		/* 32-bit key */
    uint32_t offset;		/* offset from ideal position = DIB = PL etc. */
    int value;			/* value, whatever */
    bool inuse;			/* slot is in use */
} HmapEntry;

/* a hash map space, the map has two - primary and secondary, used for growing and shrinking */
typedef struct {
    HmapEntry* buckets;		/* pointer to bucket memory */
    uint32_t mask;		/* mask, for faster index computation (size -1) */
    uint32_t log2size;		/* size, log2 */
    uint32_t shift;		/* shift, for faster index computation (key size - log2size) */
    uint32_t size;		/* max slot count = 1 << log2size */
    uint32_t offsetLimit;	/* offset = DIB = probe length limit for this space */
    uint32_t maxOffset;		/* maximum offset = DIB = probe length encountered during inserts, limits fetches */
} HmapSpace;

/* hash map */
typedef struct {
    HmapSpace spaces[2];	/* data space */
    uint32_t count;		/* current item count */
    uint32_t minSize;		/* minimum size, log2 */
    uint32_t growCount;		/* item count at which we grow the map, based on [current space size] * growLoad */
    uint32_t shrinkCount;	/* item count at which we shrink the map, based on [current space size] * shrinkLoad */
    uint32_t toMigrate;		/* how many slots (not items) we still have to migrate */
    HmapEntry* migratePos;	/* current position in migrating the previous space */
    uint32_t offsetMult;	/* maximum offset multiplier where we grow (n * space->log2size) */
    uint32_t batchSize;		/* migrate n slots per insert/delete */
    int migrateDir;		/* direction of migration (TODO: experiment with realloc, then migrate left-right on grow, right-left on shrink) */
    double growLoad;		/* load factor to grow the map */
    double shrinkLoad;		/* load factor to shrink the map */
    uint8_t current;		/* current space indicator, flips 0/1 as spaces are swapped around */
} Hmap;

/* Insertion result so we can indicate existing item and always return the new/existing entry */
typedef struct {
    HmapEntry *entry;		/* pointer to bucket, NULL on fetch when key not found */
    bool exists;		/* on insert: true = already exists and entry points to existing entry */
} HmapResult;

/* init a hash map with custom parameters */
Hmap* hmInitCustom(Hmap* map, const uint32_t log2size, const double growLoad, const double shrinkLoad,
                   const uint32_t offsetLimitMult, const uint32_t batchSize);

/* init a hash map to hold specified minimum item count */
Hmap* hmInitSize(Hmap* map, const uint32_t minsize);

/* init a hash map with specified minimum size in log2 */
Hmap* hmInitLog2Size(Hmap* map, const uint32_t log2size);

/* init a hash map with defaults */
Hmap* hmInit(Hmap* map);

/* free hash map */
void hmFree(Hmap* map);

/* remove key from map, return false if not found */
bool hmRemove(Hmap* map, const uint32_t key);

/*
 * put an entry into map, result has pointer to insered entry and exists = false,
 * or pointer to existing entry and exists = true if key is already in the map
 */
HmapResult hmPut(Hmap* map, const uint32_t key, const int value);

/* get entry from map, NULL if not found */
HmapEntry* hmGet(Hmap* map, const uint32_t key);

/* dump the contents of a hash map to stdout, dump empty slots if empties == true */
void hmDump(Hmap* map, const bool empties);

#endif /* HMAP_H_ */
