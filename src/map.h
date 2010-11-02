#ifndef __MAP_H
#define __MAP_H
//*****************************************************************************
//
// map.h
//
// similar to a hashtable, but keys as well as values can be can be anything.
//
//*****************************************************************************

typedef struct map_data                   MAP;
typedef struct map_iterator               MAP_ITERATOR;

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
MAP *newMap(void *hash_func, void *compares, int num_buckets);
void deleteMap(MAP *map);

int   mapPut    (MAP *map, void *key, void *val);
void *mapGet    (MAP *map, void *key);
void *mapRemove (MAP *map, void *key);
int   mapIn     (MAP *map, void *key);
int   mapSize   (MAP *map);

MAP_ITERATOR *newMapIterator(MAP *map);
void          deleteMapIterator(MAP_ITERATOR *I);

void  mapIteratorReset      (MAP_ITERATOR *I);
void  mapIteratorNext       (MAP_ITERATOR *I);
void *mapIteratorCurrentKey (MAP_ITERATOR *I);
void *mapIteratorCurrentVal (MAP_ITERATOR *I);

#endif // __MAP_H