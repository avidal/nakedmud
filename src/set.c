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
  int    num_buckets;
  int    size;
  LIST **buckets;
};

struct set_iterator {
  int                curr_bucket; // the bucket number we're currently on
  struct set_data           *set; // the set we're iterating over
  LIST_ITERATOR *bucket_i;        // the iterator for our current bucket
};



//*****************************************************************************
// local functions
//*****************************************************************************


//
// Find the bucket the set element belongs to
int set_elem_bucket(void *elem, int num_buckets) {
  // simple for now: just take the modulo
  return ((int)elem) % num_buckets;
};



//*****************************************************************************
// implementation of set.h
//*****************************************************************************
SET *newSet(int num_buckets) {
  int i;

  SET *set = malloc(sizeof(SET));
  set->buckets = malloc(sizeof(LIST *) * num_buckets);
  set->size    = 0;

  // all NULL until they actually get a content
  for(i = 0; i < num_buckets; i++)
    set->buckets[i] = NULL;
  set->num_buckets = num_buckets;

  return set;
};

void deleteSet(SET *set) {
  int i;

  for(i = 0; i < set->num_buckets; i++)
    if(set->buckets[i] != NULL)
      deleteList(set->buckets[i]);

  free(set->buckets);
  free(set);
};

void setPut(SET *set, void *elem) {
  // find out what bucket we belong to
  int hash_bucket = set_elem_bucket(elem, set->num_buckets);

  // add us to the bucket
  if(set->buckets[hash_bucket] == NULL)
    set->buckets[hash_bucket] = newList();
  // listPut ensures only one copy is in the list
  listPut(set->buckets[hash_bucket], elem);
};

void setRemove(SET *set, void *elem) {
  // find out what bucket we belong to
  int hash_bucket = set_elem_bucket(elem, set->num_buckets);

  // see if the bucket exists
  if(set->buckets[hash_bucket] != NULL)
    listRemove(set->buckets[hash_bucket], elem);
};

int setIn(SET *set, void *elem) {
  // find out what bucket we belong to
  int hash_bucket = set_elem_bucket(elem, set->num_buckets);

  if(set->buckets[hash_bucket] != NULL)
    return listIn(set->buckets[hash_bucket], elem);
  else
    return 0;
};



//*****************************************************************************
// set iterator
//
// we may sometimes want to iterate across all of the elements in a set.
// this lets us do so.
//*****************************************************************************
SET_ITERATOR *newSetIterator(SET *S) {
  SET_ITERATOR *I = malloc(sizeof(SET_ITERATOR));

  I->set = S;
  I->curr_bucket = 0;
  I->bucket_i = NULL;
  setIteratorReset(I);

  return I;
}


void deleteSetIterator(SET_ITERATOR *I) {
  if(I->bucket_i) deleteListIterator(I->bucket_i);
  free(I);
}


void setIteratorReset(SET_ITERATOR *I) {
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


void *setIteratorNext(SET_ITERATOR *I) {
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


void *setIteratorCurrent(SET_ITERATOR *I) {
  // we have no elements!
  if(I->bucket_i == NULL)
    return NULL;
  else
    return listIteratorCurrent(I->bucket_i);
}
