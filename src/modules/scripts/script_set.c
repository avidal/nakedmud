//*****************************************************************************
//
// script_set.c
//
// A container of script numbers an object, mob, room, etc... may have. Provides
// functions to make it easy to extract lists of scripts in the set that conform
// to certain properties (e.g. all speech scripts, exit scripts, etc).
//
//*****************************************************************************

#include <mud.h>
#include <utils.h>
#include <world.h>

#include "script_set.h"
#include "script.h"


struct script_set_data {
  LIST *script_vnums; // LIST is typically used for pointers, but because
                      // they are the same number of bits as an int, we can
                      // use them to store script vnum values as well
};

SCRIPT_SET *newScriptSet() {
  SCRIPT_SET *set = malloc(sizeof(SCRIPT_SET));
  set->script_vnums = newList();
  return set;
}

void deleteScriptSet(SCRIPT_SET *set) {
  // we don't have to delete any of the elements in here, since
  // we are not actually using the list to point to anything
  deleteList(set->script_vnums);
  free(set);
}

void scriptSetAdd   (SCRIPT_SET *set, script_vnum vnum) {
  listQueue(set->script_vnums, (void *)vnum);
}

void scriptSetRemove(SCRIPT_SET *set, script_vnum vnum) {
  listRemove(set->script_vnums, (void *)vnum);
}

LIST *scriptSetList (SCRIPT_SET *set, int type) {
  LIST *scripts = newList();
  int i = 0, script_vnum = NOTHING;

  //*************************************
  // USE A LIST ITERATOR TO OPTIMIZE THIS
  //*************************************
  for(i = 0; i < listSize(set->script_vnums); i++) {
    script_vnum = (int)listGet(set->script_vnums, i);
    SCRIPT_DATA *script = worldGetScript(gameworld, script_vnum);
    if(script != NULL &&
       (type == SCRIPT_TYPE_NONE || scriptGetType(script) == type))
      listPut(scripts, script);
  }
  return scripts;
}

void copyScriptSetTo(SCRIPT_SET *from, SCRIPT_SET *to) {
  // clear the "to" list
  while(listSize(to->script_vnums) > 0)
    listPop(to->script_vnums);

  // copy everything over
  int i;
  for(i = 0; i < listSize(from->script_vnums); i++)
    listQueue(to->script_vnums, listGet(from->script_vnums, i));
}

SCRIPT_SET *copyScriptSet(SCRIPT_SET *set) {
  SCRIPT_SET *newset = newScriptSet();
  copyScriptSetTo(set, newset);
  return newset;
}
