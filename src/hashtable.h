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

//
// create a new hashtable with the specified number of buckets in it
HASHTABLE *newHashtableSize(int num_buckets);

//
// create a new hashtable. the default number of buckets are used
HASHTABLE *newHashtable(void);

void  deleteHashtable(HASHTABLE *table);

int   hashPut    (HASHTABLE *table, const char *key, void *val);
void *hashGet    (HASHTABLE *table, const char *key);
void *hashRemove (HASHTABLE *table, const char *key);
int   hashIn     (HASHTABLE *table, const char *key);
int   hashSize   (HASHTABLE *table);

//
// expand a hashtable to the new size. Hashtables will automagically expand
// themselves as needed, but if you know that you are going to need a rather
// large hashtable apriori, you may wish to call this function after the table
// is created, to prevent unneccessary deallocations and reallocations of memory
void hashExpand(HASHTABLE *table, int size);

//
// returns a list of all the keys in the hashtable. The keys and the list
// must be deleted after use. Try: deleteListWith(list, free)
LIST *hashCollect(HASHTABLE *table);

HASH_ITERATOR *newHashIterator     (HASHTABLE *table);
void        deleteHashIterator     (HASH_ITERATOR *I);

void        hashIteratorReset      (HASH_ITERATOR *I);
void        hashIteratorNext       (HASH_ITERATOR *I);
const char *hashIteratorCurrentKey (HASH_ITERATOR *I);
void       *hashIteratorCurrentVal (HASH_ITERATOR *I);

#endif // __HASHTABLE_H
