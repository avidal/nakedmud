//*****************************************************************************
//
// mud.c
//
// a bunch of variables and settings associated with the functioning, limits,
// and such of the MUD.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "storage.h"


//*****************************************************************************
// local defines, variables, and datastructures
//*****************************************************************************

//
// the file where we store all of our mud settings
#define MUD_DATA       "../lib/muddata"

//
// our storage set of mud settings
STORAGE_SET *settings = NULL;



//*****************************************************************************
// implementation of functions in mud.h
//*****************************************************************************
void init_mud_settings() {
  settings = storage_read(MUD_DATA);
}

void mudsettingSetString(const char *key, const char *val) {
  store_string(settings,  key, val);
  storage_write(settings, MUD_DATA);  
}

void mudsettingSetDouble(const char *key, double val) {
  store_double(settings, key, val);
  storage_write(settings, MUD_DATA);
}

void mudsettingSetInt(const char *key, int val) {
  store_int(settings, key, val);
  storage_write(settings, MUD_DATA);
}

void mudsettingSetLong(const char *key, long val) {
  store_long(settings, key, val);
  storage_write(settings, MUD_DATA);
}

void mudsettingSetBool(const char *key, bool val) {
  store_bool(settings, key, val);
  storage_write(settings, MUD_DATA);
}

const char *mudsettingGetString(const char *key) {
  return read_string(settings, key);
}

double mudsettingGetDouble(const char *key) {
  return read_double(settings, key);
}

int mudsettingGetInt(const char *key) {
  return read_int(settings, key);
}

long mudsettingGetLong(const char *key) {
  return read_long(settings, key);
}

bool mudsettingGetBool(const char *key) {
  return read_bool(settings, key);
}
