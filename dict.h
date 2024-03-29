/* Hash Tables Implementation.
 *
 * This file implements in memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)

typedef struct dictEntry {
    void *key;
    void *val;
    struct dictEntry *next;
} dictEntry;

typedef struct dictType {
    unsigned int (*hashFunction)(const void *key);
    void *(*keyDup)(void *privdata, const void *key);
    void *(*valDup)(void *privdata, const void *obj);
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    void (*keyDestructor)(void *privdata, void *key);
    void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
typedef struct dictht {
    dictEntry **table;
    unsigned long size;
    unsigned long sizemask;
    unsigned long used;
} dictht;

typedef struct dict {
    dictType *type;
    void *privdata;
    dictht ht[2];
    int rehashidx; /* rehashing not in progress if rehashidx == -1 */
    int iterators; /* number of iterators currently running */
} dict;

typedef struct dictIterator {
    dict *d;
    int table;
    int index;
    dictEntry *entry, *nextEntry;
} dictIterator;

/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
#define dictFreeEntryVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->val)

#define dictSetHashVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        entry->val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        entry->val = (_val_); \
} while(0)

#define dictFreeEntryKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

#define dictSetHashKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        entry->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        entry->key = (_key_); \
} while(0)

#define dictCompareHashKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

#define dictHashKey(d, key) (d)->type->hashFunction(key)

#define dictGetEntryKey(he) ((he)->key)
#define dictGetEntryVal(he) ((he)->val)
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)

// if rehash is in progress, then return ture
#define dictIsRehashing(ht) ((ht)->rehashidx != -1)

/* API */
/**
 * Allocate memory for data structure "struct dict"
 * and initial the fields of it
 */
dict *dictCreate(dictType *type, void *privDataPtr);

/**
 * Create another hash table with the size is power of 2
 * e.g.: 60  => 64
 *     : 110 => 128
 */
int dictExpand(dict *d, unsigned long size);

/**
 * Add an element to the target hash table
 * Need handle the case of rehashing
 */
int dictAdd(dict *d, void *key, void *val);

/**
 * Add an element, discarding the old if the key already
 * exists
 *
 * Set the new value and free the old one
 */
int dictReplace(dict *d, void *key, void *val);

/**
 * Search and remove an element
 */
int dictDelete(dict *d, const void *key);
int dictDeleteNoFree(dict *d, const void *key);

/**
 * Free dict and its hash tables
 */
void dictRelease(dict *d);

/**
 * Find the dictEntry according to key
 */
dictEntry * dictFind(dict *d, const void *key);

/**
 * key => dictEntry => value
 */
void *dictFetchValue(dict *d, const void *key);

/**
 * Resize the table to the minimal size that contains
 * all the elements
 */
int dictResize(dict *d);

/**
 * Allocate memory for dictIterator and zero the fields
 */
dictIterator *dictGetIterator(dict *d);

/**
 * Get the next dictEntry
 */
dictEntry *dictNext(dictIterator *iter);

/**
 * Decrease the iterator counter
 * and free the space of dictIterator
 */
void dictReleaseIterator(dictIterator *iter);

/**
 * Return a random entry from the hash table
 */
dictEntry *dictGetRandomKey(dict *d);

/**
 * Show the stats info of dict
 */
void dictPrintStats(dict *d);

/**
 * Get the hash value of char *
 */
unsigned int dictGenHashFunction(const unsigned char *buf, int len);

/**
 * Empty the hash table and zero the fields of dict
 */
void dictEmpty(dict *d);

/**
 * set dict_can_resize to 1
 */
void dictEnableResize(void);

/**
 * set dict_can_resize to 0
 */
void dictDisableResize(void);

/**
 * Performs N steps of incremental rehashing
 */
int dictRehash(dict *d, int n);

/**
 * Rehash for an amount of time between ms milliseconds
 * and ms+1 milliseconds
 */
int dictRehashMilliseconds(dict *d, int ms);

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
