//*****************************************************************************
//
// auxiliary.c
//
// when installing a new module, it is often required that extra data be 
// stored in the basic datastructures (rooms, characters, objects, etc...). 
// However, we want to keep the modules as removed from the core of the MUD
// as possible, and having to put in extra data into the basic datastructures 
// seems like an undesirable thing. Provided below are a set of functions that 
// can be used for "installing" new data into the datastructures without having
// to directly edit the code.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "storage.h"
#include "auxiliary.h"


//*****************************************************************************
//
// local variables, datastructures, and functions
//
//*****************************************************************************

//
// the function sets used for loading and saving 
// auxiliary functions on our datatypes
//
HASHTABLE *auxiliary_manip_funcs = NULL;



//*****************************************************************************
//
// implementation of auxiliary.h
// documentation in auxiliary.h
//
//*****************************************************************************

void init_auxiliaries() {
  auxiliary_manip_funcs = newHashtable(100);
}

AUXILIARY_FUNCS *
newAuxiliaryFuncs(bitvector_t aux_type, void *new, void *delete, 
		  void *copyTo, void *copy, void *store, void *read) {
  AUXILIARY_FUNCS *newfuncs = malloc(sizeof(AUXILIARY_FUNCS));
  newfuncs->aux_type = aux_type;
  newfuncs->new      = new;
  newfuncs->delete   = delete;
  newfuncs->copyTo   = copyTo;
  newfuncs->copy     = copy;
  newfuncs->store    = store;
  newfuncs->read     = read;
  return newfuncs;
}

void
deleteAuxiliaryFuncs(AUXILIARY_FUNCS *funcs) {
  free(funcs);
}


void
auxiliariesInstall(const char *name, AUXILIARY_FUNCS *funcs) {
  hashPut(auxiliary_manip_funcs, name, funcs);
}


void
auxiliariesUninstall(const char *name) {
  AUXILIARY_FUNCS *funcs = hashRemove(auxiliary_manip_funcs, name);
  if(funcs != NULL)
    deleteAuxiliaryFuncs(funcs);
}


AUXILIARY_FUNCS *
auxiliariesGetFuncs(const char *name) {
  return hashGet(auxiliary_manip_funcs, name);
}


HASHTABLE *
newAuxiliaryData(bitvector_t aux_type) {
  HASHTABLE        *data = newHashtable(10);
  HASH_ITERATOR  *hash_i = newHashIterator(auxiliary_manip_funcs);
  AUXILIARY_FUNCS *funcs = NULL;
  const char       *name = NULL;

  while( (name = hashIteratorCurrentKey(hash_i)) != NULL) {
    funcs = hashIteratorCurrentVal(hash_i);
    hashIteratorNext(hash_i);
    if(IS_SET(funcs->aux_type, aux_type))
      hashPut(data, name, funcs->new());
  }
  deleteHashIterator(hash_i);
  return data;
}


void
auxiliaryEnsureDataComplete(HASHTABLE *data, bitvector_t aux_type) {
  HASH_ITERATOR  *hash_i = newHashIterator(auxiliary_manip_funcs);
  AUXILIARY_FUNCS *funcs = NULL;
  const char       *name = NULL;

  while( (name = hashIteratorCurrentKey(hash_i)) != NULL) {
    funcs = hashIteratorCurrentVal(hash_i);
    hashIteratorNext(hash_i);
    if(IS_SET(funcs->aux_type, aux_type) && !hashGet(data, name))
      hashPut(data, name, funcs->new());
  }
  deleteHashIterator(hash_i);
}


void
deleteAuxiliaryData(HASHTABLE *data) {
  // go across all of the data in the hashtable, and delete it
  AUXILIARY_FUNCS *funcs  = NULL;
  HASH_ITERATOR   *hash_i = newHashIterator(data);
  void            *entry  = NULL;
  const char      *name   = NULL;

  while( (name = hashIteratorCurrentKey(hash_i)) != NULL) {
    entry = hashIteratorCurrentVal(hash_i);
    hashIteratorNext(hash_i);
    funcs = auxiliariesGetFuncs(name);
    funcs->delete(entry);
  }
  deleteHashIterator(hash_i);
  deleteHashtable(data);
}


STORAGE_SET *
auxiliaryDataStore(HASHTABLE *data) {
  STORAGE_SET *set = new_storage_set();
  AUXILIARY_FUNCS *funcs  = NULL;
  HASH_ITERATOR   *hash_i = newHashIterator(data);
  void            *entry  = NULL;
  const char      *name   = NULL;

  while( (name = hashIteratorCurrentKey(hash_i)) != NULL) {
    entry = hashIteratorCurrentVal(hash_i);
    hashIteratorNext(hash_i);
    funcs = auxiliariesGetFuncs(name);
    store_set(set, name, funcs->store(entry));
  }
  deleteHashIterator(hash_i);
  return set;
}


HASHTABLE *
auxiliaryDataRead(STORAGE_SET *set, bitvector_t aux_type) {
  HASHTABLE        *data = newHashtable(10);
  HASH_ITERATOR  *hash_i = newHashIterator(auxiliary_manip_funcs);
  AUXILIARY_FUNCS *funcs = NULL;
  const char       *name = NULL;

  while( (name = hashIteratorCurrentKey(hash_i)) != NULL) {
    funcs = hashIteratorCurrentVal(hash_i);
    hashIteratorNext(hash_i);
    if(!IS_SET(funcs->aux_type, aux_type))
      continue;
    hashPut(data, name, funcs->read(read_set(set, name)));
  }
  deleteHashIterator(hash_i);
  return data;
}


void
auxiliaryDataCopyTo(HASHTABLE *from, HASHTABLE *to) {
  AUXILIARY_FUNCS *funcs = NULL;
  HASH_ITERATOR  *hash_i = NULL;
  void            *entry = NULL;
  const char       *name = NULL;

  // first, delete all of the old data
  if(hashSize(to) > 0) {
    hash_i = newHashIterator(to);
    while( (name = hashIteratorCurrentKey(hash_i)) != NULL) {
      entry = hashIteratorCurrentVal(hash_i);
      hashIteratorNext(hash_i);
      funcs = auxiliariesGetFuncs(name);
      funcs->delete(entry);
    }
    deleteHashIterator(hash_i);
  }

  // now, copy in all of the new data
  if(hashSize(from) > 0) {
    hash_i = newHashIterator(from);
    while( (name = hashIteratorCurrentKey(hash_i)) != NULL) {
      entry = hashIteratorCurrentVal(hash_i);
      hashIteratorNext(hash_i);
      funcs = auxiliariesGetFuncs(name);
      hashPut(to, name, funcs->copy(entry));
    }
    deleteHashIterator(hash_i);
  }
}


HASHTABLE *
auxiliaryDataCopy(HASHTABLE *data) {
  HASHTABLE *newdata = newHashtable(10);
  auxiliaryDataCopyTo(data, newdata);
  return newdata;
}
