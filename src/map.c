//*****************************************************************************
//
// map.c
//
// similar to a hashtable, but keys as well as values can be can be anything.
//
//*****************************************************************************

#include <stdlib.h>
#include "list.h"
#include "map.h"

int gen_hash_cmp(const void *key1, const void *key2) {
  int val = (key2 - key1);
  if(val < 0)      return -1;
  else if(val > 0) return  1;
  else             return  0;
  return val;
}

int gen_hash_func(const void *key) {
  int val = (int)key;
  if(val < 0) return -val;
  else        return  val;
}

struct map_iterator {
  int curr_bucket;
  MAP *map;
  LIST_ITERATOR *bucket_i;
};

typedef struct map_entry {
  void *key;
  void *val;
} MAP_ENTRY;

struct map_data {
  int num_buckets;
  struct list **buckets;
  int (* hash_func)(const void *key);
  int (*  compares)(const void *key1, const void *key2);
};


//
// an internal form of hashGet that returns the entire entry (key and val)
//
MAP_ENTRY *mapGetEntry(MAP *map, void *key) {
  int bucket = map->hash_func(key) % map->num_buckets;

  if(map->buckets[bucket] == NULL)
    return NULL;
  else {
    struct list_iterator *list_i = newListIterator(map->buckets[bucket]);
    MAP_ENTRY *elem   = NULL;

    for(;(elem = listIteratorCurrent(list_i)) != NULL; listIteratorNext(list_i))
      if(!map->compares(key, elem->key))
	break;
    deleteListIterator(list_i);

    return elem;
  }
}


MAP_ENTRY *newMapEntry(void *key, void *val) {
  MAP_ENTRY *entry = malloc(sizeof(MAP_ENTRY));
  entry->key = key;
  entry->val = val;
  return entry;
}

void deleteMapEntry(MAP_ENTRY *entry) {
  free(entry);
}


//*****************************************************************************
//
// implementation of hashmap.h
// documentation in hashmap.h
//
//*****************************************************************************
MAP *newMap(void *hash_func, void *compares, int num_buckets) {
  int i;
  MAP *map = malloc(sizeof(MAP));
  map->num_buckets = num_buckets;
  map->hash_func   = (hash_func ? hash_func : gen_hash_func);
  map->compares    = (compares  ? compares  : gen_hash_cmp);
  map->buckets = malloc(sizeof(struct list *) * num_buckets);
  for(i = 0; i < num_buckets; i++)
    map->buckets[i] = NULL;
  return map;
}

void  deleteMap(MAP *map) {
  int i;
  for(i = 0; i < map->num_buckets; i++) {
    if(map->buckets[i]) {
      MAP_ENTRY *entry = NULL;
      while((entry=(MAP_ENTRY *)listPop(map->buckets[i])) !=NULL)
	deleteMapEntry(entry);
      deleteList(map->buckets[i]);
    }
  }
  free(map->buckets);
  free(map);
}

int  mapPut    (MAP *map, void *key, void *val) {
  MAP_ENTRY *elem = mapGetEntry(map, key);

  // update the val if it's already here
  if(elem) {
    elem->val = val;
    return 1;
  }
  else {
    int bucket = map->hash_func(key) % map->num_buckets;

    // if the bucket doesn't exist yet, create it
    if(map->buckets[bucket] == NULL)
      map->buckets[bucket] = newList();

    MAP_ENTRY *entry = newMapEntry(key, val);
    listPut(map->buckets[bucket], entry);
    return 1;
  }
}

void *mapGet    (MAP *map, void *key) {
  MAP_ENTRY *elem = mapGetEntry(map, key);
  if(elem)
    return elem->val;
  else
    return NULL;
}

void *mapRemove (MAP *map, void *key) {
  int bucket = map->hash_func(key) % map->num_buckets;

  if(map->buckets[bucket] == NULL)
    return NULL;
  else {
    struct list_iterator *list_i = newListIterator(map->buckets[bucket]);
    MAP_ENTRY *elem   = NULL;

    for(;(elem = listIteratorCurrent(list_i)) != NULL; listIteratorNext(list_i))
      if(!map->compares(key, elem->key))
	break;
    deleteListIterator(list_i);

    if(elem) {
      void *val = elem->val;
      listRemove(map->buckets[bucket], elem);
      deleteMapEntry(elem);
      return val;
    }
    else
      return NULL;
  }
}

int   mapIn     (MAP *map, void *key) {
  return (mapGet(map, key) != NULL);
}

int   mapSize   (MAP *map) {
  int i;
  int size = 0;

  for(i = 0; i < map->num_buckets; i++)
    if(map->buckets[i])
      size += listSize(map->buckets[i]);

  return size;
}


//*****************************************************************************
//
// implementation of the hashmap iterator
// documentation in hashmap.h
//
//*****************************************************************************
MAP_ITERATOR *newMapIterator(MAP *map) {
  MAP_ITERATOR *I = malloc(sizeof(MAP_ITERATOR));
  I->map = map;
  I->bucket_i = NULL;
  mapIteratorReset(I);

  return I;
}

void        deleteMapIterator     (MAP_ITERATOR *I) {
  if(I->bucket_i) deleteListIterator(I->bucket_i);
  free(I);
}

void        mapIteratorReset      (MAP_ITERATOR *I) {
  int i;

  if(I->bucket_i) deleteListIterator(I->bucket_i);
  I->bucket_i = NULL;
  I->curr_bucket = 0;

  // bucket_i will be NULL if there are no elements
  for(i = 0; i < I->map->num_buckets; i++) {
    if(I->map->buckets[i] != NULL &&
       listSize(I->map->buckets[i]) > 0) {
      I->curr_bucket = i;
      I->bucket_i = newListIterator(I->map->buckets[i]);
      break;
    }
  }
}


void        mapIteratorNext       (MAP_ITERATOR *I) {
  // no elements in the hashmap
  if(I->bucket_i == NULL)
    return;
  // we're at the end of our list
  else if(listIteratorNext(I->bucket_i) == NULL) {
    deleteListIterator(I->bucket_i);
    I->bucket_i = NULL;
    I->curr_bucket++;
    
    for(; I->curr_bucket < I->map->num_buckets; I->curr_bucket++) {
      if(I->map->buckets[I->curr_bucket] != NULL &&
	 listSize(I->map->buckets[I->curr_bucket]) > 0) {
	I->bucket_i = newListIterator(I->map->buckets[I->curr_bucket]);
	break;
      }
    }
  }
}

void *mapIteratorCurrentKey (MAP_ITERATOR *I) {
  if(!I->bucket_i)
    return NULL;
  else {
    MAP_ENTRY *entry = ((MAP_ENTRY *) listIteratorCurrent(I->bucket_i));
    if(entry)
      return entry->key;
    else
      return NULL;
  }
}

void       *mapIteratorCurrentVal (MAP_ITERATOR *I) {
  if(!I->bucket_i) 
    return NULL;
  else {
    MAP_ENTRY *entry = ((MAP_ENTRY *) listIteratorCurrent(I->bucket_i));
    if(entry)
      return entry->val;
    else
      return NULL;
  }
}
