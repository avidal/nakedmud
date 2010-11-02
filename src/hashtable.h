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

struct hashtable *newHashtable(int num_buckets);
void  deleteHashtable(struct hashtable *table);

int   hashPut    (struct hashtable *table, const char *key, void *val);
void *hashGet    (struct hashtable *table, const char *key);
void *hashRemove (struct hashtable *table, const char *key);
int   hashIn     (struct hashtable *table, const char *key);
int   hashSize   (struct hashtable *table);


struct hashtable_iterator *newHashIterator(struct hashtable *table);
void        deleteHashIterator     (struct hashtable_iterator *I);

void        hashIteratorReset      (struct hashtable_iterator *I);
void        hashIteratorNext       (struct hashtable_iterator *I);
const char *hashIteratorCurrentKey (struct hashtable_iterator *I);
void       *hashIteratorCurrentVal (struct hashtable_iterator *I);


#endif // __HASHTABLE_H
