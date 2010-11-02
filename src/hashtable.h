#ifndef __HASHTABLE_H
#define __HASHTABLE_H
//*****************************************************************************
//
// hashtable.h
//
// Your friendly neighbourhood hashtable. Maps a <key> to a <value>. *sigh*
// why am I not writing this MUD in C++, again?
//
//*****************************************************************************

typedef struct hashtable                  HASHTABLE;
typedef struct hashtable_iterator         HASH_ITERATOR;


HASHTABLE *newHashtable(int num_buckets);
void  deleteHashtable(HASHTABLE *table);

int   hashPut    (HASHTABLE *table, const char *key, void *val);
void *hashGet    (HASHTABLE *table, const char *key);
void *hashRemove (HASHTABLE *table, const char *key);
int   hashIn     (HASHTABLE *table, const char *key);
int   hashSize   (HASHTABLE *table);

HASH_ITERATOR *newHashIterator     (HASHTABLE *table);
void        deleteHashIterator     (HASH_ITERATOR *I);

void        hashIteratorReset      (HASH_ITERATOR *I);
void        hashIteratorNext       (HASH_ITERATOR *I);
const char *hashIteratorCurrentKey (HASH_ITERATOR *I);
void       *hashIteratorCurrentVal (HASH_ITERATOR *I);


#endif // __HASHTABLE_H
