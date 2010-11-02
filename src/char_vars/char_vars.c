//*****************************************************************************
//
// char_vars.c
//
// This module allows key/value pairs to be stored on a character. The values
// can be strings, integers, or doubles. If a value is called for in the wrong
// type (e.g. you're trying to get a string as an integer) the module will
// automagically handle the conversion. Variable types default to ints.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../character.h"
#include "../storage.h"
#include "../auxiliary.h"

#include "char_vars.h"


//*****************************************************************************
//
// local functions and datastructurs
//
//*****************************************************************************

// the size of the hashtable we store char vars in
#define CHAR_VAR_TABLE_SIZE   10

// used in storage_sets to keep track of what kind of data we're saving
const char *char_var_types[] = {
  "string",
  "int",
  "long",
  "double"
};


typedef struct char_var {
  char *str_val;
  int   type;
} CHAR_VAR;


CHAR_VAR *newCharVarString(const char *str) {
  CHAR_VAR *data = malloc(sizeof(CHAR_VAR));
  data->str_val  = strdup(str ? str : "");
  data->type     = CHAR_VAR_STRING;
  return data;
}

CHAR_VAR *newCharVarInt(int val) {
  CHAR_VAR *data = malloc(sizeof(CHAR_VAR));
  char str_val[20]; sprintf(str_val, "%d", val);
  data->str_val  = strdup(str_val);
  data->type     = CHAR_VAR_INT;
  return data;
}

CHAR_VAR *newCharVarLong(long val) {
  CHAR_VAR *data = malloc(sizeof(CHAR_VAR));
  char str_val[20]; sprintf(str_val, "%ld", val);
  data->str_val  = strdup(str_val);
  data->type     = CHAR_VAR_LONG;
  return data;
}

CHAR_VAR *newCharVarDouble(double val) {
  CHAR_VAR *data = malloc(sizeof(CHAR_VAR));
  char str_val[20]; sprintf(str_val, "%lf", val);
  data->str_val  = strdup(str_val);
  data->type     = CHAR_VAR_DOUBLE;
  return data;
}

void deleteCharVar(CHAR_VAR *data) {
  if(data->str_val) free(data->str_val);
  free(data);
}

CHAR_VAR *charVarCopy(CHAR_VAR *data) {
  CHAR_VAR *new_data = malloc(sizeof(CHAR_VAR));
  new_data->str_val = strdup(data->str_val);
  new_data->type    = data->type;
  return new_data;
}

//
// delete a hashtable of char vars
//
void deleteCharVarTable(HASHTABLE *table) {
  HASH_ITERATOR *hash_i = newHashIterator(table);
  const char       *key = NULL;
  CHAR_VAR         *val = NULL;
  
  ITERATE_HASH(key, val, hash_i)
    deleteCharVar(val);
  deleteHashIterator(hash_i);
  deleteHashtable(table);
}



//*****************************************************************************
//
// char_var auxiliary data
//
//*****************************************************************************
typedef struct char_var_aux_data {
  HASHTABLE *char_vars;
} CHAR_VAR_AUX_DATA;


CHAR_VAR_AUX_DATA *
newCharVarAuxData() {
  CHAR_VAR_AUX_DATA *data = malloc(sizeof(CHAR_VAR_AUX_DATA));
  // Hashtables can take up lots of storage space. Because of this, let's
  // not create any tables until it's actually needed. This will cut down
  // on lots of memory usage w.r.t. NPCs who do not use character variables
  //  data->char_vars        = newHashtable(CHAR_VAR_TABLE_SIZE);
  data->char_vars         = NULL;
  return data;
}


void
deleteCharVarAuxData(CHAR_VAR_AUX_DATA *data) {
  if(data->char_vars)
    deleteCharVarTable(data->char_vars);
  free(data);
}


void
charVarAuxDataCopyTo(CHAR_VAR_AUX_DATA *from, CHAR_VAR_AUX_DATA *to) {
  int from_size = (from->char_vars ? hashSize(from->char_vars) : 0);
  int to_size   = (to->char_vars   ? hashSize(to->char_vars)   : 0);

  // clear out our current data
  if(to_size > 0) {
    deleteCharVarTable(to->char_vars);
    to->char_vars = NULL;
  }

  // check to see if the "from" table exists
  if(from_size > 0) {
    // make sure the "to" table exists
    if(to->char_vars == NULL)
      to->char_vars = newHashtable(CHAR_VAR_TABLE_SIZE);

    // copy everything over
    HASH_ITERATOR *from_i = newHashIterator(from->char_vars);
    const char       *key = NULL;
    CHAR_VAR         *val = NULL;

    ITERATE_HASH(key, val, from_i)
      hashPut(to->char_vars, key, charVarCopy(val));
    deleteHashIterator(from_i);
  }
}


CHAR_VAR_AUX_DATA *
charVarAuxDataCopy(CHAR_VAR_AUX_DATA *data) {
  CHAR_VAR_AUX_DATA *new_data = newCharVarAuxData();
  charVarAuxDataCopyTo(data, new_data);
  return new_data;
}


STORAGE_SET *charVarAuxDataStore(CHAR_VAR_AUX_DATA *data) {
  // first, check if the table even exists
  if(data->char_vars == NULL || hashSize(data->char_vars) == 0)
    return new_storage_set();

  STORAGE_SET       *set = new_storage_set();
  HASH_ITERATOR  *hash_i = newHashIterator(data->char_vars);
  STORAGE_SET_LIST *list = new_storage_list();
  const char        *key = NULL;
  CHAR_VAR          *val = NULL;

  store_list(set, "variables", list);
  // iterate across all the entries and add them
  ITERATE_HASH(key, val, hash_i) {
    STORAGE_SET *var_set = new_storage_set();
    store_string(var_set, "key",  key);
    store_string(var_set, "val",  val->str_val);
    store_string(var_set, "type", char_var_types[val->type]);
    storage_list_put(list, var_set);
  }
  deleteHashIterator(hash_i);
  return set;
}


HASHTABLE *variableRead(STORAGE_SET *set) {
  HASHTABLE *table       = newHashtable(CHAR_VAR_TABLE_SIZE);
  STORAGE_SET_LIST *list = read_list(set, "list");
  STORAGE_SET *var       = NULL;

  while( (var = storage_list_next(list)) != NULL)
    hashPut(table, read_string(var, "key"), (void *)read_int(var, "val"));
  return table;
}


CHAR_VAR_AUX_DATA *charVarAuxDataRead(STORAGE_SET *set) {
  // if the set doesn't contain any entries, don't bother trying to parse
  if(!storage_contains(set, "variables"))
    return newCharVarAuxData();

  CHAR_VAR_AUX_DATA *data = newCharVarAuxData();
  STORAGE_SET_LIST  *list = read_list(set, "variables");
  STORAGE_SET    *var_set = NULL;
  data->char_vars         = newHashtable(CHAR_VAR_TABLE_SIZE);
  
  while( (var_set = storage_list_next(list)) != NULL) {
    const char *var_type = read_string(var_set, "type");
    if(!strcasecmp(var_type, "int"))
      hashPut(data->char_vars, read_string(var_set, "key"),
	      newCharVarInt(read_int(var_set, "val")));
    else if(!strcasecmp(var_type, "long"))
      hashPut(data->char_vars, read_string(var_set, "key"),
	      newCharVarLong(read_long(var_set, "val")));
    else if(!strcasecmp(var_type, "double"))
      hashPut(data->char_vars, read_string(var_set, "key"),
	      newCharVarDouble(read_double(var_set, "val")));
    else if(!strcasecmp(var_type, "string"))
      hashPut(data->char_vars, read_string(var_set, "key"),
	      newCharVarString(read_string(var_set, "val")));
    else
      log_string("ERROR: Tried to read unknown char_var type, %s.", var_type);
  }
    
  return data;
}


void init_char_vars() {
  // install char vars on the character datastructure
  auxiliariesInstall("char_var_aux_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_CHAR,
				       newCharVarAuxData, deleteCharVarAuxData,
				       charVarAuxDataCopyTo, charVarAuxDataCopy,
				       charVarAuxDataStore,charVarAuxDataRead));
}




//*****************************************************************************
//
// functions for interacting with char_vars
//
//*****************************************************************************
int charGetVarType(CHAR_DATA *ch, const char *key) {
  CHAR_VAR_AUX_DATA *data = charGetAuxiliaryData(ch, "char_var_aux_data");
  CHAR_VAR *var = (data->char_vars ? hashGet(data->char_vars, key) : NULL);
  return (var ? var->type : CHAR_VAR_INT);
}


int charGetInt(CHAR_DATA *ch, const char *key) {
  CHAR_VAR_AUX_DATA *data = charGetAuxiliaryData(ch, "char_var_aux_data");
  CHAR_VAR *var = (data->char_vars ? hashGet(data->char_vars, key) : NULL);
  return (var ? atoi(var->str_val) : 0);
}


long charGetLong(CHAR_DATA *ch, const char *key) {
  CHAR_VAR_AUX_DATA *data = charGetAuxiliaryData(ch, "char_var_aux_data");
  CHAR_VAR *var = (data->char_vars ? hashGet(data->char_vars, key) : NULL);
  return (var ? atol(var->str_val) : 0);
}


double charGetDouble(CHAR_DATA *ch, const char *key) {
  CHAR_VAR_AUX_DATA *data = charGetAuxiliaryData(ch, "char_var_aux_data");
  CHAR_VAR *var = (data->char_vars ? hashGet(data->char_vars, key) : NULL);
  return (var ? atof(var->str_val) : 0);
}


const char *charGetString(CHAR_DATA *ch, const char *key) {
  CHAR_VAR_AUX_DATA *data = charGetAuxiliaryData(ch, "char_var_aux_data");
  CHAR_VAR *var = (data->char_vars ? hashGet(data->char_vars, key) : NULL);
  return (var ? var->str_val : "");
}


void charSetInt(CHAR_DATA *ch, const char *key, int val) {
  CHAR_VAR_AUX_DATA *data = charGetAuxiliaryData(ch, "char_var_aux_data");
  CHAR_VAR *old = (data->char_vars ? hashRemove(data->char_vars, key) : NULL);
  if(data->char_vars == NULL && val != 0)
    data->char_vars = newHashtable(CHAR_VAR_TABLE_SIZE);
  if(old != NULL) 
    deleteCharVar(old);
  if(val != 0)
    hashPut(data->char_vars, key, newCharVarInt(val));
}


void charSetLong(CHAR_DATA *ch, const char *key, long val) {
  CHAR_VAR_AUX_DATA *data = charGetAuxiliaryData(ch, "char_var_aux_data");
  CHAR_VAR *old = (data->char_vars ? hashRemove(data->char_vars, key) : NULL);
  if(data->char_vars == NULL && val != 0)
    data->char_vars = newHashtable(CHAR_VAR_TABLE_SIZE);
  if(old != NULL) 
    deleteCharVar(old);
  if(val != 0)
    hashPut(data->char_vars, key, newCharVarLong(val));
}


void charSetDouble(CHAR_DATA *ch, const char *key, double val) {
  CHAR_VAR_AUX_DATA *data = charGetAuxiliaryData(ch, "char_var_aux_data");
  CHAR_VAR *old = (data->char_vars ? hashRemove(data->char_vars, key) : NULL);
  if(data->char_vars == NULL && val != 0)
    data->char_vars = newHashtable(CHAR_VAR_TABLE_SIZE);
  if(old != NULL) 
    deleteCharVar(old);
  if(val != 0)
    hashPut(data->char_vars, key, newCharVarDouble(val));
}


void charSetString(CHAR_DATA *ch, const char *key, const char *val) {
  CHAR_VAR_AUX_DATA *data = charGetAuxiliaryData(ch, "char_var_aux_data");
  CHAR_VAR *old = (data->char_vars ? hashRemove(data->char_vars, key) : NULL);
  if(data->char_vars == NULL && *val != '\0') 
    data->char_vars = newHashtable(CHAR_VAR_TABLE_SIZE);
  if(old != NULL) 
    deleteCharVar(old);
  if(*val != '\0')
    hashPut(data->char_vars, key, newCharVarString(val));
}
