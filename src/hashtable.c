//*****************************************************************************
//
// hashtable.c
//
// Your friendly neighbourhood hashtable. Maps a <key> to a <value>. *sigh*
// why am I not writing this MUD in C++, again?
//
//*****************************************************************************

#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "hashtable.h"

struct hashtable_iterator {
  int curr_bucket;
  struct hashtable *table;
  struct list_iterator *bucket_i;
};

struct hashtable_entry {
  char *key;
  void *val;
};

struct hashtable {
  int num_buckets;
  struct list **buckets;
};


int hash(const char *key) {
  int i;
  const int BASE = 2;
  int base = 1;
  int len = strlen(key);
  int hvalue = 0;

  for (i = 0; i < len; i++) {
    base *= BASE;
    hvalue += key[i] * base;
  }

  return (hvalue < 0 ? hvalue * -1 : hvalue);
};


//
// an internal form of hashGet that returns the entire entry (key and val)
//
struct hashtable_entry *hashGetEntry(struct hashtable *table, const char *key){
  int bucket = hash(key) % table->num_buckets;

  if(table->buckets[bucket] == NULL)
    return NULL;
  else {
    struct list_iterator *list_i = newListIterator(table->buckets[bucket]);
    struct hashtable_entry *elem = NULL;

    for(;(elem = listIteratorCurrent(list_i)) != NULL; listIteratorNext(list_i))
      if(!strcmp(key, elem->key))
	break;
    deleteListIterator(list_i);

    return elem;
  }
}

struct hashtable_entry *newHashtableEntry(const char *key, void *val) {
  struct hashtable_entry *entry = malloc(sizeof(struct hashtable_entry));
  entry->key = strdup(key);
  entry->val = val;
  return entry;
}

void deleteHashtableEntry(struct hashtable_entry *entry) {
  if(entry->key) free(entry->key);
  free(entry);
}


//*****************************************************************************
//
// implementation of hashtable.h
// documentation in hashtable.h
//
//*****************************************************************************
struct hashtable *newHashtable(int num_buckets) {
  int i;
  struct hashtable *table = malloc(sizeof(struct hashtable));
  table->num_buckets = num_buckets;
  table->buckets = malloc(sizeof(struct list *) * num_buckets);
  for(i = 0; i < num_buckets; i++)
    table->buckets[i] = NULL;
  return table;
}

void  deleteHashtable(struct hashtable *table) {
  int i;
  for(i = 0; i < table->num_buckets; i++) {
    if(table->buckets[i]) {
      struct hashtable_entry *entry = NULL;
      while((entry=(struct hashtable_entry *)listPop(table->buckets[i])) !=NULL)
	deleteHashtableEntry(entry);
      deleteList(table->buckets[i]);
    }
  }

  free(table->buckets);
  free(table);
}

int  hashPut    (struct hashtable *table, const char *key, void *val) {
  struct hashtable_entry *elem = hashGetEntry(table, key);

  // if it's already in, update the value
  if(elem) {
    elem->val = val;
    return 1;
  }
  else {
    int bucket = hash(key) % table->num_buckets;

    // if the bucket doesn't exist yet, create it
    if(table->buckets[bucket] == NULL)
      table->buckets[bucket] = newList();

    struct hashtable_entry *entry = newHashtableEntry(key, val);
    listPut(table->buckets[bucket], entry);
    return 1;
  }
}

void *hashGet    (struct hashtable *table, const char *key) {
  struct hashtable_entry *elem = hashGetEntry(table, key);
  if(elem != NULL)
    return elem->val;
  else
    return NULL;
}

void *hashRemove (struct hashtable *table, const char *key) {
  int bucket = hash(key) % table->num_buckets;

  if(table->buckets[bucket] == NULL)
    return NULL;
  else {
    struct list_iterator *list_i = newListIterator(table->buckets[bucket]);
    struct hashtable_entry *elem = NULL;

    for(;(elem = listIteratorCurrent(list_i)) != NULL; listIteratorNext(list_i))
      if(!strcmp(key, elem->key))
	break;
    deleteListIterator(list_i);

    if(elem) {
      void *val = elem->val;
      listRemove(table->buckets[bucket], elem);
      deleteHashtableEntry(elem);
      return val;
    }
    else
      return NULL;
  }
}

int   hashIn     (struct hashtable *table, const char *key) {
  return (hashGet(table, key) != NULL);
}

int   hashSize   (struct hashtable *table) {
  int i;
  int size = 0;

  for(i = 0; i < table->num_buckets; i++)
    if(table->buckets[i])
      size += listSize(table->buckets[i]);

  return size;
}


//*****************************************************************************
//
// implementation of the hashtable iterator
// documentation in hashtable.h
//
//*****************************************************************************
struct hashtable_iterator *newHashIterator(struct hashtable *table) {
  struct hashtable_iterator *I = malloc(sizeof(struct hashtable_iterator));
  I->table = table;
  I->bucket_i = NULL;
  hashIteratorReset(I);

  return I;
}

void        deleteHashIterator     (struct hashtable_iterator *I) {
  if(I->bucket_i) deleteListIterator(I->bucket_i);
  free(I);
}

void        hashIteratorReset      (struct hashtable_iterator *I) {
  int i;

  if(I->bucket_i) deleteListIterator(I->bucket_i);
  I->bucket_i = NULL;
  I->curr_bucket = 0;

  // bucket_i will be NULL if there are no elements
  for(i = 0; i < I->table->num_buckets; i++) {
    if(I->table->buckets[i] != NULL &&
       listSize(I->table->buckets[i]) > 0) {
      I->curr_bucket = i;
      I->bucket_i = newListIterator(I->table->buckets[i]);
      break;
    }
  }
}


void        hashIteratorNext       (struct hashtable_iterator *I) {
  // no elements in the hashtable
  if(I->bucket_i == NULL) 
    return;
  // we're at the end of our list
  else if(listIteratorNext(I->bucket_i) == NULL) {
    deleteListIterator(I->bucket_i);
    I->bucket_i = NULL;
    I->curr_bucket++;
    
    for(; I->curr_bucket < I->table->num_buckets; I->curr_bucket++) {
      if(I->table->buckets[I->curr_bucket] != NULL &&
	 listSize(I->table->buckets[I->curr_bucket]) > 0) {
	I->bucket_i = newListIterator(I->table->buckets[I->curr_bucket]);
	break;
      }
    }
  }
}


const char *hashIteratorCurrentKey (struct hashtable_iterator *I) {
  if(!I->bucket_i) 
    return NULL;
  else {
    struct hashtable_entry *entry = ((struct hashtable_entry *)
				     listIteratorCurrent(I->bucket_i));
    if(entry)
      return entry->key;
    else
      return NULL;
  }
}


void       *hashIteratorCurrentVal (struct hashtable_iterator *I) {
  if(!I->bucket_i) 
    return NULL;
  else {
    struct hashtable_entry *entry = ((struct hashtable_entry *)
				     listIteratorCurrent(I->bucket_i));
    if(entry)
      return entry->val;
    else
      return NULL;
  }
}
