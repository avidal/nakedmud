//*****************************************************************************
//
// set.h
//
// a non-ordered container that has constant lookup time.
//
//*****************************************************************************

#include <stdlib.h>
#include "list.h"
#include "set.h"

struct set_data {
  int       num_buckets;
  struct list **buckets;
};

struct set_iterator {
  int                curr_bucket; // the bucket number we're currently on
  struct set_data           *set; // the set we're iterating over
  struct list_iterator *bucket_i; // the iterator for our current bucket
};


//*****************************************************************************
//
// local functions
//
//*****************************************************************************


//
// Find the bucket the set element belongs to
//
int set_elem_bucket(void *elem, int num_buckets) {
  // simple for now: just take the modulo
  return ((int)elem) % num_buckets;
};


//*****************************************************************************
//
// implementation of set.h
//
//*****************************************************************************
struct set_data *newSet(int num_buckets) {
  int i;

  struct set_data *set = malloc(sizeof(struct set_data));
  set->buckets = malloc(sizeof(struct list *) * num_buckets);

  // all NULL until they actually get a content
  for(i = 0; i < num_buckets; i++)
    set->buckets[i] = NULL;
  set->num_buckets = num_buckets;

  return set;
};


void deleteSet(struct set_data *set) {
  int i;

  for(i = 0; i < set->num_buckets; i++)
    if(set->buckets[i] != NULL)
      deleteList(set->buckets[i]);

  free(set->buckets);
  free(set);
};


void setPut(struct set_data *set, void *elem) {
  // find out what bucket we belong to
  int hash_bucket = set_elem_bucket(elem, set->num_buckets);

  // add us to the bucket
  if(set->buckets[hash_bucket] == NULL)
    set->buckets[hash_bucket] = newList();
  // listPut ensures only one copy is in the list
  listPut(set->buckets[hash_bucket], elem);
};


void setRemove(struct set_data *set, void *elem) {
  // find out what bucket we belong to
  int hash_bucket = set_elem_bucket(elem, set->num_buckets);

  // see if the bucket exists
  if(set->buckets[hash_bucket] != NULL)
    listRemove(set->buckets[hash_bucket], elem);
};


int setIn(struct set_data *set, void *elem) {
  // find out what bucket we belong to
  int hash_bucket = set_elem_bucket(elem, set->num_buckets);

  if(set->buckets[hash_bucket] != NULL)
    return listIn(set->buckets[hash_bucket], elem);
  else
    return 0;
};


//*****************************************************************************
//
// set iterator
//
// we may sometimes want to iterate across all of the elements in a set.
// this lets us do so.
//
//*****************************************************************************
struct set_iterator *newSetIterator(struct set_data *S) {
  struct set_iterator *I = malloc(sizeof(struct set_iterator));

  I->set = S;
  I->curr_bucket = 0;
  I->bucket_i = NULL;
  setIteratorReset(I);

  return I;
}


void deleteSetIterator(struct set_iterator *I) {
  if(I->bucket_i) deleteListIterator(I->bucket_i);
  free(I);
}


void setIteratorReset(struct set_iterator *I) {
  int i;

  if(I->bucket_i) 
    deleteListIterator(I->bucket_i);
  I->bucket_i = NULL;
  I->curr_bucket = 0;

  for(i = 0; i < I->set->num_buckets; i++) {
    if(I->set->buckets[i] == NULL)
      continue;
    if(isListEmpty(I->set->buckets[i]))
      continue;
    else {
      I->curr_bucket = i;
      I->bucket_i = newListIterator(I->set->buckets[i]);
      break;
    }
  }
}


void *setIteratorNext(struct set_iterator *I) {
  // we have no iterator ... we have no elements left to iterate over!
  if(I->bucket_i == NULL)
    return NULL;
  else {
    void *elem = listIteratorNext(I->bucket_i);
    // we're okay ... we have an element
    if(elem != NULL)
      return elem;
    // otherwise, we need to move onto a new bucket
    else {
      deleteListIterator(I->bucket_i);
      I->bucket_i = NULL;

      I->curr_bucket++;
      // find the next non-empty list
      for(; I->curr_bucket < I->set->num_buckets; I->curr_bucket++) {
	if(I->set->buckets[I->curr_bucket] == NULL)
	  continue;
	if(isListEmpty(I->set->buckets[I->curr_bucket]))
	  continue;
	I->bucket_i = newListIterator(I->set->buckets[I->curr_bucket]);
	break;
      }

      // we've ran out of buckets!
      if(I->curr_bucket == I->set->num_buckets)
	return NULL;
      else
	return listIteratorCurrent(I->bucket_i);
    }
  }
}


void *setIteratorCurrent(struct set_iterator *I) {
  // we have no elements!
  if(I->bucket_i == NULL)
    return NULL;
  else
    return listIteratorCurrent(I->bucket_i);
}