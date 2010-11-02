#ifndef __CHAR_VARS_H
#define __CHAR_VARS_H
//*****************************************************************************
//
// char_vars.h
//
// This module allows key/value pairs to be stored on a character. The values
// can be strings, integers, or doubles. If a value is called for in the wrong
// type (e.g. you're trying to get a string as an integer) the module will
// automagically handle the conversion. Variable types default to ints.
//
//*****************************************************************************


//
// This must be put at the top of mud.h so the rest of the MUD knows that
// we've got the alias module installed
// #define MODULE_CHAR_VARS
//


// the different types of data we can store
#define CHAR_VAR_STRING        0
#define CHAR_VAR_INT           1
#define CHAR_VAR_LONG          2
#define CHAR_VAR_DOUBLE        3



//
// prepare char_vars for use
//
void         init_char_vars   ();

int          charGetVarType   (CHAR_DATA *ch, const char *key);

int          charGetInt       (CHAR_DATA *ch, const char *key);
long         charGetLong      (CHAR_DATA *ch, const char *key);
double       charGetDouble    (CHAR_DATA *ch, const char *key);
const char  *charGetString    (CHAR_DATA *ch, const char *key);

void         charSetInt       (CHAR_DATA *ch, const char *key, int val);
void         charSetLong      (CHAR_DATA *ch, const char *key, long val);
void         charSetDouble    (CHAR_DATA *ch, const char *key, double val);
void         charSetString    (CHAR_DATA *ch, const char *key, const char *val);

bool         charHasVar       (CHAR_DATA *ch, const char *key);
void         charDeleteVar    (CHAR_DATA *ch, const char *key);

#endif // __CHAR_VARS_H
