#ifndef __AUXILIARY_H
#define __AUXILIARY_H
//*****************************************************************************
//
// auxiliary.h
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

#define AUXILIARY_TYPE_CHAR      (1 << 0)
#define AUXILIARY_TYPE_ROOM      (1 << 1)
#define AUXILIARY_TYPE_OBJ       (1 << 2)


//
// A structure containing 6 functions that are needed for storing and saving
// auxiliary data that is used by auxiliarys outside the core of the MUD.
//
// aux_type is a list of the types of datastructures this auxiliary data is
// intended for.
//
// new should be a pointer to a function that creates a new copy of the
// auxiliary data. New should take no arguments, and return a pointer to
// the auxiliary data.
//
// delete should take one argument - the auxiliary data to be deleted. It
// should not return anything.
//
// copyTo should take two arguments. a "from" and a "to". It copies all of the
// auxiliary data from "from" to "to".
//
// copy should take one argument - the auxiliary data to be copied. It returns
// a copy of that data.
//
// store should be a function that takes the data as an argument, and returns
// a storage set of the data contained within the object.
//
// read should be a function that takes a storage set, and parses auxiliary
// data from that.
//
typedef struct auxiliary_functions {
  bitvector_t aux_type;
  void          *(* new)();
  void        (* delete)(void *data);
  void        (* copyTo)(void *from, void *to);
  void        *(*  copy)(void *data);
  STORAGE_SET *(* store)(void *data);
  void        *(*  read)(STORAGE_SET *set);
} AUXILIARY_FUNCS;


//
// prepare auxiliary data for usage
//
void init_auxiliaries();


//
// create a new set of functions for handling auxiliary data
//
AUXILIARY_FUNCS *
newAuxiliaryFuncs(bitvector_t aux_type, void *new, void *delete, 
		  void *copyTo, void *copy, void *store, void *read);


//
// clear up memory used to hold the functions for handling auxiliary data
//
void
deleteAuxiliaryFuncs(AUXILIARY_FUNCS *funcs);


//
// install the functions used for handling auxiliary data. "name" should be
// a unique tag for holding the auxiliary data in the datastructure and funcs 
// should be the set of functions used to load/save the auxiliary data.
//
void
auxiliariesInstall(const char *name, AUXILIARY_FUNCS *funcs);


//
// remove the functions for manipulating the auxiliary data
//
void
auxiliariesUnintall(const char *name);


//
// Return a pointer to the set of functions used for handling the auxiliary
// data. If the functions have not been installed, return NULL
//
AUXILIARY_FUNCS *
auxiliariesGetFuncs(const char *name);


//
// Create a new hashtable of auxiliary data for the 
// datatype specified in aux_type 
//
HASHTABLE *
newAuxiliaryData(bitvector_t aux_type);


//
// when a room/obj/char needs to be deleted, a hashtable with their auxiliary
// data is passed into here and we handle it.
//
void
deleteAuxiliaryData(HASHTABLE *data);


//
// Put all of the auxiliary data into a storage set
//
STORAGE_SET *
auxiliaryDataStore(HASHTABLE *data);


//
// read the auxiliary data for a specified datatype in from the set
//
HASHTABLE *
auxiliaryDataRead(STORAGE_SET *set, bitvector_t aux_type);


//
// Copy the auxiliary data from one hashtable to another
//
void
auxiliaryDataCopyTo(HASHTABLE *from, HASHTABLE *to);


//
// Make a copy of the auxiliary data
//
HASHTABLE *
auxiliaryDataCopy(HASHTABLE *data);


#endif // __AUXILIARY_H
