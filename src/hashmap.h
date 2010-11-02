#ifndef __HASHMAP_H
#define __HASHMAP_H
//*****************************************************************************
//
// hashmap.h
//
// similar to a hashtable, but keys as well as values can be can be anything.
//
//*****************************************************************************

//
// if hash_func and/or compares are null, generic hashing and comparing
// functions are used.
//
// hash_func is expected to be a function that takes the key type used in
// this map, and returns an integer based on that key.
//
// compares is expected to be a function that takes two keys and compares
// them togher. If they are equal, 0 is returned. if key1 is less than key2,
// -1 is returned. otherwise, 1 is returned.
//
struct hashmap *newHashmap(void *hash_func,
			   void *compares,
			   int num_buckets);
void            deleteHashmap(struct hashmap *map);

int   mapPut    (struct hashmap *map, void *key, void *val);
void *mapGet    (struct hashmap *map, void *key);
void *mapRemove (struct hashmap *map, void *key);
int   mapIn     (struct hashmap *map, void *key);
int   mapSize   (struct hashmap *map);

struct map_iterator *newMapIterator(struct hashmap *map);
void              deleteMapIterator(struct map_iterator *I);

void  mapIteratorReset      (struct map_iterator *I);
void  mapIteratorNext       (struct map_iterator *I);
void *mapIteratorCurrentKey (struct map_iterator *I);
void *mapIteratorCurrentVal (struct map_iterator *I);

#endif // __MAP_H
