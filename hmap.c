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
 * @file   hmap.c
 * @date   Fri Jan 10 23:27:00 2020
 *
 * @brief  implementation of a simple hash map:
 *
 *                            - linear probing, power of 2 size, uint32_t keys
 *                            - Robin Hood with backwards shift, no tombstones
 *                            - Dynamic resizing (full migration or in batches)
 *                            - Search probe length limited to actual maximum
 *                            - Configurable load factors, minimum size, batch size
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "xalloc.h"

#include "hmap.h"

/* Fibonacci mapping bases */
#define FIB32_BASE 2654435769               /* 2^32 / phi */
#define FIB64_BASE 11400714819323198485llu  /* 2^64 / phi */

/* defaults */
#define HMAP_MIN_LOG2SIZE        5
#define HMAP_DEF_LOG2SIZE        5
#define HMAP_DEF_GROW_LOAD       0.7
#define HMAP_DEF_SHRINK_LOAD     0.25
#define HMAP_MIN_BATCHSIZE       4
#define HMAP_DEF_MAX_OFFSET_MULT 1

/* only a helper for bit shifts for if/when this is implemented for 64-bit keys */
#define HMAP_MAX_BITS 32

/* convenience constants for triggerResize */
#define HMAP_GROW 1
#define HMAP_SHRINK -1

/* the shit */
static inline uint32_t roundPow2_32(uint32_t n);
static inline uint32_t log2_32(uint32_t n);
static inline uint32_t hindex32(const uint32_t key, const uint32_t shift, const uint32_t mask);
static HmapResult hsInsert(HmapSpace* space, const uint32_t key, const int value);
static inline HmapEntry* hsFetch(HmapSpace* space, const uint32_t key, const uint32_t offsetLimit);
static bool hsRemove(HmapSpace* space, const uint32_t key);
static void hsInit(Hmap* map, HmapSpace* space, const uint32_t log2size);
static void hsMigrate(Hmap* map, uint32_t batchSize);
static void triggerResize(Hmap* map, const int dir);

/* round n to power of 2, recipe off the internets (Sean Eron Anderson's Bit Twiddling Hacks) */
static inline uint32_t roundPow2_32(uint32_t n) {

    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;

    return n;

}

/* DeBrujin sequence for bit positions */
static const int _db32[32] = {
    0, 9, 1, 10, 13, 21, 2, 29,
    11, 14, 16, 18, 22, 25, 3, 30,
    8, 12, 20, 28, 15, 17, 24, 7,
    19, 27, 23, 6, 26, 5, 4,31
};

/* log2 in 32 bits also from Bit Twiddling Hacks */
static inline uint32_t log2_32(uint32_t n) {

    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;

    return _db32[(uint32_t)(n * 0x07C4ACDD) >> 27];

}

/*
 * Return the key index for a map of given bit width / mask: Fibonacci index with some XOR mixing.
 * As per definition this is a "hash function", just used for uint32_t keys... It's a mapping function.
 * The keys themselves could / would / should be hashes of another data type (e.g. string),
 * computed using some hash function like XXHash, murmur, FNV, etc...
 *
 * Fibonacci hashing / indexing ripped from Malte Skarupke (probablydance.com):
 * https://probablydance.com/2018/06/16/fibonacci-hashing-the-optimization-that-the-world-forgot-or-a-better-alternative-to-integer-modulo/
 *
 */
static inline uint32_t hindex32(const uint32_t key, const uint32_t shift, const uint32_t mask) {
#if 0
    return ((uint32_t)(key * FIB32_BASE) >> shift); /* basic Fibonacci index */
#elif 1
printf("yo\n");
    return (uint32_t)((key ^ (key >> shift)) * FIB32_BASE) >> shift; /* Fibonacci index with XOR mixing */
#elif 0
	return key & mask; /* totally basic modulo (also why we need the mask) */
#endif /* 0 */
}

/* insert a key + value into a hash map space */
static HmapResult hsInsert(HmapSpace* space, const uint32_t key, const int value) {

    HmapResult ret = { NULL, false };
    uint32_t index = hindex32(key, space->shift, space->mask);
    bool placed = false;	/* have we found a home for the new entry yet? */
    uint32_t myindex = index;	/* slot where we _actually_ placed our new entry! */
    HmapEntry me = { .key = key, .value = value, .offset = 0, .inuse = true };
    HmapEntry tmp;
    HmapEntry* buckets = space->buckets;

    if(buckets == NULL) {
	xcalloc(space->buckets, space->size, sizeof(HmapEntry));
	buckets = space->buckets;
    }

    /* yes, this has the potential for an endless loop, but the init/resize logic ensures the map always grows before it is full */
    while(buckets[index].inuse) {

	/* entry already exists */
	if(buckets[index].key == me.key) {
	    ret.exists = true;
	    ret.entry = &buckets[index];
	    return ret;
	}

	/* this is all there is to Robin Hood on insert */
	if(buckets[index].offset < me.offset) {
	    /* minus this, which is only to return the inserted bucket */
	    if(!placed) {
		myindex = index;
		placed = true;
	    }
	    tmp = me;
	    me = buckets[index];
	    buckets[index] = tmp;
	}

	index++;
	index &= space->mask;
	me.offset++;

    }

    /* keep the running max offset, which will limit searches */
    if(me.offset > space->maxOffset) {
	space->maxOffset = me.offset;
    }

    /* no rich buckets encountered */
    if(!placed) {
	myindex = index;
    }

    /* commit the new / pushed down entry to table */
    buckets[index] = me;

    /* return the actual entry we placed */
    ret.entry = &buckets[myindex];
    return ret;

}

/* fetch an entry from hash map space, return NULL if not found */
static inline HmapEntry* hsFetch(HmapSpace* space, const uint32_t key, const uint32_t offsetLimit) {

    uint32_t index = hindex32(key, space->shift, space->mask);
    uint32_t offset = 0;

    HmapEntry* buckets = space->buckets;

    if(buckets == NULL) {
	return NULL;
    }

    /* do not stop on an unused entry because we delete without backwards shift during migration */
    /* TODO: investigate proper delete on migration and how this will affect fetches durinf migration */
    while(offset <= offsetLimit) {
	if(buckets[index].inuse && buckets[index].key == key) {
	    return &buckets[index];
	}
	index++;
	index &= space->mask;
	offset++;
    }

    return NULL;
}

/* remove an entry from hash map space, return false if not found */
static bool hsRemove(HmapSpace* space, const uint32_t key) {

    static const HmapEntry emptyentry = { 0,0,0,0 };

    uint32_t index = hindex32(key, space->shift, space->mask);
    uint32_t previndex;

    HmapEntry* buckets = space->buckets;
    uint32_t offset = 0;

    /* find the entry and empty it: note this will not work in the lazy-emptied secondary space, but it's never used on it, yet */
    while(offset < space->offsetLimit && buckets[index].inuse) {

	/* keep the index of the previous entry (that gets emptied) */
	previndex = index;

	index++;
	index &= space->mask;
	offset++;

	/* we've found our key, let's empty it */
	if(buckets[previndex].key == key) {
	    buckets[previndex] = emptyentry;
	    goto found;
	}

    }

    /* entry not found */
    return false;

found:

    /* keep shifting consecutive entries left until we find an empty or offset 0 (Righteous Ruler a.k.a. JAH) */
    while(buckets[index].offset > 0 && buckets[index].inuse) {
	buckets[previndex] = buckets[index];
	buckets[previndex].offset--;
	buckets[index] = emptyentry;
	previndex = index;
	index++;
	index &= space->mask;
    }

    return true;

}

/* initialise a hash map space */
static void hsInit(Hmap* map, HmapSpace* space, const uint32_t log2size) {

    uint32_t newsize = log2size;

    /* don't shrink below minimum */
    if(newsize < map->minSize) {
	newsize = map->minSize;
    }

    space->log2size = newsize;
    space->size = 1 << newsize;
    space->mask = space->size - 1;
    space->shift = HMAP_MAX_BITS - newsize;
    space->offsetLimit = map->offsetMult * newsize;
    space->maxOffset = 0;

    /* establish shrink and grow watermarks - saves a floating point multiplication on every insert and delete */
    map->shrinkCount = space->size * map->shrinkLoad;
    map->growCount = space->size * map->growLoad;

    /* this way we will ALWAYS grow before the map is completely full */
    if(map->growCount > space->mask) {
	map->growCount = space->mask;
    }

    space->buckets = NULL;

}

/* migrate up to @batchSize entries from secondary space to primary space */
static void hsMigrate(Hmap* map, const uint32_t batchSize) {

    uint32_t migrated = 0;
    uint32_t left = map->toMigrate;
    uint32_t pos = map->migratePos;

    HmapSpace* current = &map->spaces[map->current];
    HmapSpace* other = &map->spaces[!map->current];

    /* keep migrating until nothing left or we've done our batch */
    /* TODO: investigate limiting migration to (occupied) item count, not slot count, may shave off some time */
    while((left > 0) && (migrated < batchSize)) {
	HmapEntry *entry = &other->buckets[pos];
	if(entry->inuse) {
	    /* properly insert the entry into current (new) space */
	    hsInsert(current, entry->key, entry->value);
	    /* lazy-delete the entry in old space */
	    entry->inuse = false;
	}
	pos++;
	migrated++;
	left--;
    }

    /* migration complete: we can free up the secondary space */
    if(left == 0) {
	map->migrateDir = 0;
	map->toMigrate = 0;
	map->migratePos = 0;
	free(other->buckets);
	other->buckets = NULL;
    /* otherwise only update migration state */
    } else {
	map->toMigrate = left;
	map->migratePos = pos;
    }

}

/* start a resize of the hash map to begin migration, dir 1 = grow, -1 = shrink */
static void triggerResize(Hmap* map, const int dir) {

    uint8_t current = map->current;
    HmapSpace* space = &map->spaces[current];
    uint32_t newsize = space->log2size + dir;

    /* space currently used has entries and needs to be migrated */
    if(map->count) {
#ifdef HMAP_DEBUG
	fprintf(stderr, "# triggering resize from %d to %d at count %d, load %.02f max offset %d/%d\n",1<<space->log2size,1<<newsize, map->count, (map->count+0.0) / (space->size+0.0), space->maxOffset,space->log2size);
#endif
	map->migrateDir = dir;
	map->toMigrate = space->size;
	map->migratePos = 0;
    /* special case: empty map, we free up all space */
    } else {

	/*
	 * ...yes, the map will cycle through calloc/free if we continue adding and removing one item,
	 * but does one really need a hash map if one is to store one item only?
	 * [ note that no resize is triggered if we are at minSize, so this won't happen unless minsize is 1 ]
	 */

	if(map->spaces[0].buckets != NULL) {
	    free(map->spaces[0].buckets);
	    map->spaces[0].buckets = NULL;
	}
	if(map->spaces[1].buckets != NULL) {
	    free(map->spaces[1].buckets);
	    map->spaces[1].buckets = NULL;
	}
	/* init from minimum size */
	newsize = map->minSize;
    }

    /* flip spaces around */
    current = !current;
    map->current = current;
    space = &map->spaces[current];

    /* initialise other (new current) (current current??) space */
    hsInit(map, space, newsize);

    /* migrate all at once if we need to, classic hash table */
    if(map->batchSize == HMAP_MIGRATE_ALL && map->count > 0) {
	hsMigrate(map, map->toMigrate);
    }

}

/* get entry from map: result has pointer to the entry and an 'exists' bool */
HmapResult hmGet(Hmap* map, const uint32_t key) {

    HmapResult ret = { NULL, true };

    /* TODO: investigate fetch from secondary space first if migrated less than half */

    /* fetch from primary space */
    ret.entry = hsFetch(&map->spaces[map->current], key, map->spaces[map->current].maxOffset);

    /* not found and stil migrating: see if it's in the secondary space */
    if(ret.entry == NULL && map->toMigrate) {
	ret.entry = hsFetch(&map->spaces[!map->current], key, map->spaces[!map->current].maxOffset);
    }

    if(ret.entry == NULL) {
	ret.exists = false;
    }

    return ret;

}

/*
 * put an entry into map, result has pointer to insered entry and exists = false,
 * or pointer to existing entry and exists = true if key is already in the map
 */
HmapResult hmPut(Hmap* map, const uint32_t key, const int value) {

    HmapResult ret;
    HmapSpace* current = &map->spaces[map->current];
    HmapSpace* other = &map->spaces[!map->current];

    /* we have some entries to migrate */
    if(map->toMigrate) {

	/* try fetch from previous space first */
	HmapEntry* entry = hsFetch(other, key, other->maxOffset);
	if(entry != NULL) {
	    ret.entry = entry;
	    /* key exists */
	    ret.exists = true;
	    return ret;
	}

	/* continue with migration */
	hsMigrate(map, map->batchSize);

    }

    /* insert (only) into current space */
    ret = hsInsert(current, key, value);

    /* key exists, evac */
    if(ret.exists) {
	return ret;
    }

    map->count++;

    /* if we have hit a / the limit, start growing, but not when already migrating */
    if((map->toMigrate == 0) && (current->maxOffset == current->offsetLimit || map->count >= map->growCount)) {
	triggerResize(map, HMAP_GROW);
    }

    return ret;

}

/* remove key from map, return false if not found */
bool hmRemove(Hmap* map, const uint32_t key) {

    HmapSpace* current = &map->spaces[map->current];
    HmapSpace* other = &map->spaces[!map->current];

    /* we have some entries to migrate */
    if(map->toMigrate) {

	/* try removing from previous space first */
	/* TODO: this is inconsistent, we should lazy-remove the existing item since this is what we do on migration */
	if(hsRemove(other,key)) {
	    map->count--;
	    /* continue with migration */
	    hsMigrate(map, map->batchSize);
	    return true;
	}

	hsMigrate(map, map->batchSize);

    }

    /* properly remove from current space */
    if(hsRemove(current,key)) {
	map->count--;
	/* trigger resize if we have hit the/a limit and not already migrating, do not shrink below minimum size */
	if(map->toMigrate == 0 && map->count <= map->shrinkCount && current->log2size > map->minSize) {
	    triggerResize(map, HMAP_SHRINK);
	}
	return true;
    }

    /* nope */
    return false;

}

/* free a hash map */
void hmFree(Hmap* map) {

    if(map != NULL) {
	if(map->spaces[0].buckets != NULL) {
	    free(map->spaces[0].buckets);
	}
	if(map->spaces[1].buckets != NULL) {
	    free(map->spaces[1].buckets);
	}

	memset(map, 0, sizeof(Hmap));

    }

}

/* init a hash map with custom parameters */
Hmap* hmInitCustom(Hmap* map, const uint32_t minsize_log2, const double growLoad, const double shrinkLoad,
                   const uint32_t offsetLimitMult, const uint32_t batchSize) {

    /* good boi! */
    memset(map, 0, sizeof(Hmap));

    /* keep the size sane */
    map->minSize = minsize_log2;
    if(map->minSize < HMAP_MIN_LOG2SIZE) {
	map->minSize = HMAP_MIN_LOG2SIZE;
    }
    if(map->minSize > HMAP_MAX_BITS) {
	map->minSize = HMAP_MAX_BITS;
    }

    map->growLoad = growLoad;
    map->shrinkLoad = shrinkLoad;
    map->offsetMult = offsetLimitMult;

    /* sanitise load factors */
    if(map->growLoad <= 0.0 || map->growLoad >= 1.0) {
	map->growLoad = HMAP_DEF_GROW_LOAD;
    }
    if(map->shrinkLoad <= 0.0 || map->shrinkLoad >= 1.0) {
	map->shrinkLoad = HMAP_DEF_SHRINK_LOAD;
    }

    /* keep shrink load at at least 0.5 * grow load */
    if(map->shrinkLoad > (map->growLoad / 2.0)) {
	map->shrinkLoad = map->growLoad / 2.0;
    }

    /* sanitise batch size */
    map->batchSize = batchSize;
    if(batchSize != HMAP_MIGRATE_ALL) {
	/* make sure we will have migrated in time */
	if(batchSize < (growLoad / shrinkLoad + 1.0)) {
	    map->batchSize = (growLoad / shrinkLoad) + 1.0;
	}
	/* ...and enforce minimum */
	if(map->batchSize < HMAP_MIN_BATCHSIZE) {
	    map->batchSize = HMAP_MIN_BATCHSIZE;
	}
    }

    /* ...NULL != 0? Just in case... */
    map->spaces[0].buckets = NULL;
    map->spaces[1].buckets = NULL;

    /* initialise current space */
    hsInit(map, &map->spaces[0], map->minSize);

    return map;
}

/* init a hash map to hold specified minimum item count */
Hmap* hmInitSize(Hmap* map, const uint32_t minsize) {

    size_t log2size = log2_32(minsize);

    if(minsize > (1<<log2size)) {
	log2size++;
    }

    /* make sure we do not grow when reaching the recommended size */
    while(minsize >= (HMAP_DEF_GROW_LOAD * ((1 << log2size) + 0.0))) {
	log2size++;
    }

    return hmInitCustom(map, log2size, HMAP_DEF_GROW_LOAD, HMAP_DEF_SHRINK_LOAD, HMAP_DEF_MAX_OFFSET_MULT, HMAP_MIN_BATCHSIZE);
}

/* init a hash map with specified minimum size in log2 */
Hmap* hmInitLog2Size(Hmap* map, const uint32_t log2size) {
    return hmInitCustom(map, log2size, HMAP_DEF_GROW_LOAD, HMAP_DEF_SHRINK_LOAD, HMAP_DEF_MAX_OFFSET_MULT, HMAP_MIN_BATCHSIZE);
}

/* init a hash map with defaults */
Hmap* hmInit(Hmap* map) {
    return hmInitCustom(map, HMAP_DEF_LOG2SIZE, HMAP_DEF_GROW_LOAD, HMAP_DEF_SHRINK_LOAD, HMAP_DEF_MAX_OFFSET_MULT, HMAP_MIN_BATCHSIZE);
}

/* dump the contents of a hash map to stdout */
void hmDump(Hmap* map, const bool empties) {

    HmapSpace* current = &map->spaces[map->current];
    HmapSpace* other = &map->spaces[!map->current];
    HmapEntry* bucket;

    printf("# In map: %d keys, primary space size %d, bits %d, max probe length %d\n", map->count, current->size, current->log2size, current->maxOffset);
    printf("# space, slot, state, key, value, offset\n");
    bucket = current->buckets;

    if(bucket == NULL) {
	goto other;
    }

    for(int i = 0; i < current->size; i++) {
	if(bucket->inuse || empties) {
	    printf("pri, #%06d, %s, 0x%08x (%010d), %06d, %06d\n", i, bucket->inuse ? "full " : "empty", bucket->key, bucket->key, bucket->value, bucket->offset);
	}
	bucket++;
    }

other:
    if(other->buckets == NULL) {
	return;
    }

    printf("# Table still migrating, left %d, old size %d bits %d max probe length %d\n", map->toMigrate, other->size, other->log2size, other->maxOffset);
    printf("# space, slot, state, key, value, offset\n");
    bucket = other->buckets;

    for(int i = 0; i < other->size; i++) {
	if(bucket->inuse || empties) {
	    printf("sec, #%06d, %s, 0x%08x (%010d), %06d, %06d\n", i, bucket->inuse ? "full " : "empty", bucket->key, bucket->key, bucket->value, bucket->offset);
	}
	bucket++;
    }

}
