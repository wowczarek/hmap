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
 * @brief  hash map type and function declarations
 *
 */

#ifndef HMAP_H_
#define HMAP_H_

#include <stdint.h>
#include <stdbool.h>

/* constants */
#define HMAP_GROW 1
#define HMAP_SHRINK -1
#define HMAP_MIGRATE_ALL 0
#define HMAP_NONE 0

/* a single hash map entry */
typedef struct {
    uint32_t key;
    uint32_t offset;
    int value;
    bool inuse;
} HmapEntry;

/* a hash map space */
typedef struct {
    HmapEntry* buckets;
    uint32_t mask;
    uint32_t log2size;
    uint32_t shift;
    uint32_t size;
    uint32_t offsetLimit;
    uint32_t maxOffset;
} HmapSpace;

/* hash map */
typedef struct {
    HmapSpace spaces[2];
    uint32_t count;
    uint32_t minSize;
    uint32_t growSize;
    uint32_t shrinkSize;
    uint32_t toMigrate;
    uint32_t migratePos;
    uint32_t offsetMult;
    uint32_t batchSize;
    int migrateDir;
    double growLoad;
    double shrinkLoad;
    uint8_t current;
} Hmap;

/* result of inserts and retrievals */
typedef struct {
    HmapEntry *entry;
    bool exists;
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
 * put an entry into map, result has pointer to insered entry and found = false,
 * or pointer to existing entry and found = true if key is already in the map
 */
HmapResult hmPut(Hmap* map, const uint32_t key, const int value);

/* get entry from map, result has pointer to the entry and a 'found' bool */
HmapResult hmGet(Hmap* map, const uint32_t key);

/* dump the contents of a hash map to stdout */
void hmDump(Hmap* map, const bool empties);

#endif /* HMAP_H_ */
