#ifndef __SCRIPT_SET_H
#define __SCRIPT_SET_H
//*****************************************************************************
//
// script_set.h
//
// A container of script numbers an object, mob, room, etc... may have. Provides
// functions to make it easy to extract lists of scripts in the set that conform
// to certain properties (e.g. all speech scripts, exit scripts, etc).
//
//*****************************************************************************


#define SCRIPT_TYPE_ANY           (-1)


SCRIPT_SET *newScriptSet();
void deleteScriptSet(SCRIPT_SET *set);

void scriptSetAdd   (SCRIPT_SET *set, script_vnum vnum);
void scriptSetRemove(SCRIPT_SET *set, script_vnum vnum);

void copyScriptSetTo(SCRIPT_SET *from, SCRIPT_SET *to);
SCRIPT_SET *copyScriptSet(SCRIPT_SET *set);

//
// Return a list of all scripts of a certain type the set contains.
// use SCRIPT_TYPE_ANY for a list of all scripts. LIST must be 
// deleted after being used. 
//
LIST *scriptSetList (SCRIPT_SET *set, int type);

#endif // __SCRIPT_SET_H
