//*****************************************************************************
//
// items.c
//
// Objects can serve many different functions; they might be containers, they
// might be weapons, they might be pieces of clothing... the list goes on and
// on. The classification of what type of item the object is is what handles
// these sorts of distinctions. This is a general framework for adding new
// item types. Contains functions for storing type data on objects. Item types
// are expected to be able to handle their own subtyping (if neccessary). 
// Objects can be multi-typed (e.g. a desk with a drawer can be both a piece
// of furniture and a container).
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../object.h"
#include "../auxiliary.h"
#include "../storage.h"

#include "items.h"



//*****************************************************************************
// local functions, datastructures, and defines
//*****************************************************************************
// a table of all our item types, and their assocciated new/delete/etc.. funcs
HASHTABLE *type_table = NULL;

//
// this is what will go into our type_data table
typedef struct item_func_data {
  void          *(* new)();
  void        (* delete)(void *data);
  void         *(* copy)(void *data);
  void        (* copyto)(void *from, void *to);
  STORAGE_SET *(* store)(void *data);
  void         *(* read)(STORAGE_SET *set);
} ITEM_FUNC_DATA;


//
// create a new structure for holding item type functions
ITEM_FUNC_DATA *newItemFuncData(void *new, void *delete, void *copyTo,
				void *copy, void *store, void *read) {
  ITEM_FUNC_DATA *data = malloc(sizeof(ITEM_FUNC_DATA));
  data->new    = new;
  data->delete = delete;
  data->copy   = copy;
  data->copyto = copyTo;
  data->store  = store;
  data->read   = read;
  return data;
}


//
// deletes one structure for holding item type functions
void deleteItemFuncData(ITEM_FUNC_DATA *data) {
  free(data);
}

//
// goes through an object's item table and deletes everything in it. Does
// not delete the table itself
void clearItemTable(HASHTABLE *table) {
  // if our hashtable isn't empty, we have to make sure 
  // we delete all of the data within it
  if(hashSize(table) > 0) {
    HASH_ITERATOR *hash_i = newHashIterator(table);
    const char       *key = NULL;
    void             *val = NULL;
    // delete the contents of our hashtable
    ITERATE_HASH(key, val, hash_i) {
      ITEM_FUNC_DATA *funcs = hashGet(type_table, key);
      funcs->delete(hashRemove(table, key));
    }
    deleteHashIterator(hash_i);
  }
}

//
// copies the contents of one item table over to another. clears the destination
// table first.
void copyItemTableTo(HASHTABLE *from, HASHTABLE *to) {
  clearItemTable(to);
  // if the table we're copying has contents, go throug hand copy them all
  if(hashSize(from) > 0) {
    HASH_ITERATOR *hash_i = newHashIterator(from);
    const char       *key = NULL;
    void             *val = NULL;
    // copy all the contents
    ITERATE_HASH(key, val, hash_i) {
      ITEM_FUNC_DATA *funcs = hashGet(type_table, key);
      hashPut(to, key, funcs->copy(val));
    }
    deleteHashIterator(hash_i);
  }
}

//
// Creates a new item table and copies the contents of the old one 
// over to the new one 
HASHTABLE *copyItemTable(HASHTABLE *table) {
  HASHTABLE *newtable = newHashtable();
  copyItemTableTo(table, newtable);
  return newtable;
}



//*****************************************************************************
// auxiliary data
//*****************************************************************************
typedef struct item_data {
  HASHTABLE *item_table;
} ITEM_DATA;

ITEM_DATA *newItemData() {
  ITEM_DATA *data  = malloc(sizeof(ITEM_DATA));
  data->item_table = newHashtable();
  return data;
}

void deleteItemData(ITEM_DATA *data) {
  clearItemTable(data->item_table);
  deleteHashtable(data->item_table);
  free(data);
}

void itemDataCopyTo(ITEM_DATA *from, ITEM_DATA *to) {
  copyItemTableTo(from->item_table, to->item_table);
}

ITEM_DATA *itemDataCopy(ITEM_DATA *data) {
  ITEM_DATA *newdata = newItemData();
  itemDataCopyTo(data, newdata);
  return newdata;
}

STORAGE_SET *itemDataStore(ITEM_DATA *data) {
  STORAGE_SET       *set = new_storage_set();
  // if we have a type or more, go through them all and store 'em
  if(hashSize(data->item_table) > 0) {
    STORAGE_SET_LIST *list = new_storage_list();
    HASH_ITERATOR  *hash_i = newHashIterator(data->item_table);
    ITEM_FUNC_DATA  *funcs = NULL;
    const char        *key = NULL;
    void              *val = NULL;

    // store all of our item type data
    ITERATE_HASH(key, val, hash_i) {
      STORAGE_SET *one_set = new_storage_set();
      funcs = hashGet(type_table, key);
      store_string(one_set, "type", key);
      store_set(one_set, "data", funcs->store(val));
      storage_list_put(list, one_set);
    }
    deleteHashIterator(hash_i);

    store_list(set, "types", list);
  }
  return set;
}


ITEM_DATA *itemDataRead(STORAGE_SET *set) {
  ITEM_DATA         *data = newItemData();
  STORAGE_SET_LIST *types = read_list(set, "types");
  ITEM_FUNC_DATA   *funcs = NULL;
  STORAGE_SET  *one_entry = NULL;

  // go through each entry and parse the item type
  while( (one_entry = storage_list_next(types)) != NULL) {
    const char      *type = read_string(one_entry, "type");
    // make sure the type is valid
    if((funcs = hashGet(type_table, type)) == NULL) 
      continue;
    STORAGE_SET *type_set = read_set(one_entry, "data");
    hashPut(data->item_table, type, funcs->read(type_set));
  }
  return data;
}



//*****************************************************************************
// implementation of items.h
//*****************************************************************************
void init_items(void) {
  type_table = newHashtable();
  auxiliariesInstall("type_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_OBJ, newItemData, 
				       deleteItemData, itemDataCopyTo,
				       itemDataCopy, itemDataStore, 
				       itemDataRead));

  // initialize item olc
  extern void init_item_olc(void); init_item_olc();

  // now, initialize our basic items types
  extern void init_container(); init_container();
  extern void init_portal();    init_portal();
  extern void init_furniture(); init_furniture();
  extern void init_worn();      init_worn();
}

void item_add_type(const char *type, 
		   void *new,    void *delete,
		   void *copyTo, void *copy, 
		   void *store,  void *read) {
  ITEM_FUNC_DATA *funcs = NULL;
  // first, make sure no item type by this name already
  // exists. If one does, delete it so it can be replaced
  if((funcs = hashRemove(type_table, type)) != NULL)
    deleteItemFuncData(funcs);
  funcs = newItemFuncData(new, delete, copyTo, copy, store, read);
  hashPut(type_table, type, funcs);
}

LIST *itemTypeList(void) {
  LIST *list = newList();
  HASH_ITERATOR *hash_i = newHashIterator(type_table);
  const char       *key = NULL;
  void             *val = NULL;
  ITERATE_HASH(key, val, hash_i)
    listPutWith(list, strdup(key), strcasecmp);
  deleteHashIterator(hash_i);
  return list;
}

void *objGetTypeData(OBJ_DATA *obj, const char *type) {
  ITEM_DATA *data = objGetAuxiliaryData(obj, "type_data");
  return hashGet(data->item_table, type);
}

void objSetType(OBJ_DATA *obj, const char *type) {
  ITEM_FUNC_DATA *funcs = hashGet(type_table, type);
  ITEM_DATA *data       = objGetAuxiliaryData(obj, "type_data");
  void *old_item_data   = hashGet(data->item_table, type);
  if(old_item_data) return;
  else hashPut(data->item_table, type, funcs->new());
}

void objDeleteType(OBJ_DATA *obj, const char *type) {
  ITEM_FUNC_DATA *funcs = hashGet(type_table, type);
  ITEM_DATA *data       = objGetAuxiliaryData(obj, "type_data");
  void *old_item_data   = hashRemove(data->item_table, type);
  if(old_item_data) funcs->delete(old_item_data);
}

bool objIsType(OBJ_DATA *obj, const char *type) {
  ITEM_DATA *data = objGetAuxiliaryData(obj, "type_data");
  return hashIn(data->item_table, type);
}

const char *objGetTypes(OBJ_DATA *obj) {
  static char buf[MAX_BUFFER];
  ITEM_DATA *data = objGetAuxiliaryData(obj, "type_data");
  if(hashSize(data->item_table) == 0)
    sprintf(buf, "none");
  else {
    *buf = '\0';
    HASH_ITERATOR *hash_i = newHashIterator(data->item_table);
    const char       *key = NULL;
    void             *val = NULL;
    int             count = 0;

    ITERATE_HASH(key, val, hash_i) {
      sprintf(buf, "%s%s%s", buf, (count > 0 ? ", " : ""), key);
      count++;
    }
    deleteHashIterator(hash_i);
  }
  return buf;
}
