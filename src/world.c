//*****************************************************************************
//
// world.c
//
// This is the implementation of the WORLD structure.
//
//*****************************************************************************

#include <sys/stat.h>

#include "mud.h"
#include "zone.h"
#include "room.h"
#include "character.h"
#include "object.h"
#include "dialog.h"
#include "utils.h"
#include "storage.h"
#include "world.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/script.h"



// the number of rooms we would expect, in different sized worlds
#define SMALL_WORLD       3000
#define MEDIUM_WORLD      7000
#define LARGE_WORLD      15000
#define HUGE_WORLD      250000


struct world_data {
  char            *path; // the path to our world directory
  PROPERTY_TABLE *rooms; // this table is a communal table for rooms.
                         // each room also has an entry in its corresponding
                         // zone, but we also put it into this table for
                         // quicker lookup
  LIST *zones;
};




//*****************************************************************************
//
// implementation of world.h
//
//*****************************************************************************
WORLD_DATA *newWorld(const char *path) {
  WORLD_DATA *world = malloc(sizeof(WORLD_DATA));

  world->path  = strdup(path);
  world->zones = newList();
  world->rooms = newPropertyTable(roomGetVnum, SMALL_WORLD);

  return world;
};


void deleteWorld(WORLD_DATA *world) {
  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA *zone = NULL;

  // detach ourself from all of our zones
  ITERATE_LIST(zone, zone_i)
    zoneSetWorld(zone, NULL);
  deleteListIterator(zone_i);

  deletePropertyTable(world->rooms);
  deleteList(world->zones);
  free(world->path);

  free(world);
};


//
// The generic "remove" function. "remover" is the function that
// removes the thing from a zone.
//
void *worldRemoveVnum(WORLD_DATA *world, void *remover, int vnum) {
  void *(* remove_func)(ZONE_DATA *, int) = remover;
  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA       *zone = NULL;
  void            *data = NULL;

  // find the zone that contains our vnum
  ITERATE_LIST(zone, zone_i) {
    // we've found it
    if(zoneGetMinBound(zone) <= vnum && zoneGetMaxBound(zone) >= vnum) {
      data = remove_func(zone, vnum);
      break;
    }
  }

  deleteListIterator(zone_i);
  return data;
}


ROOM_DATA *worldRemoveRoomVnum(WORLD_DATA *world, int vnum) {
  propertyTableRemove(world->rooms, vnum);
  return worldRemoveVnum(world, zoneRemoveRoom, vnum);
};


CHAR_DATA *worldRemoveMobVnum(WORLD_DATA *world, int vnum) {
  return worldRemoveVnum(world, zoneRemoveMob, vnum);
};


OBJ_DATA *worldRemoveObjVnum(WORLD_DATA *world, int vnum) {
  return worldRemoveVnum(world, zoneRemoveObj, vnum);
};

SCRIPT_DATA *worldRemoveScriptVnum(WORLD_DATA *world, int vnum) {
  return worldRemoveVnum(world, zoneRemoveScript, vnum);
};

DIALOG_DATA *worldRemoveDialogVnum(WORLD_DATA *world, int vnum) {
  return worldRemoveVnum(world, zoneRemoveDialog, vnum);
};

bool worldRemoveRoom(WORLD_DATA *world, ROOM_DATA *room) {
  return (worldRemoveRoomVnum(world, roomGetVnum(room)) != NULL);
};

bool worldRemoveMob(WORLD_DATA *world, CHAR_DATA *mob) {
  return (worldRemoveMobVnum(world, charGetVnum(mob)) != NULL);
};

bool worldRemoveObj(WORLD_DATA *world, OBJ_DATA *obj) {
  return (worldRemoveObjVnum(world, objGetVnum(obj)) != NULL);
};

bool worldRemoveScript(WORLD_DATA *world, SCRIPT_DATA *script) {
  return (worldRemoveScriptVnum(world, scriptGetVnum(script)) != NULL);
};

bool worldRemoveDialog(WORLD_DATA *world, DIALOG_DATA *dialog) {
  return (worldRemoveDialogVnum(world, dialogGetVnum(dialog)) != NULL);
};


ZONE_DATA *worldRemoveZoneVnum(WORLD_DATA *world, int vnum) {
  // go through and find our zone
  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA *zone = NULL;

  ITERATE_LIST(zone, zone_i) {
    // we've found it
    if(zoneGetVnum(zone) == vnum) {
      int i;
      // make sure we take out all rooms from this zone
      // that we've loaded into our zone table
      //
      // eventually, will we have to do this for obj and mob protos, too?
      for(i = zoneGetMinBound(zone); i <= zoneGetMaxBound(zone); i++)
	propertyTableRemove(world->rooms, i);
      break;
    }
  }

  deleteListIterator(zone_i);
  if(zone)
    zoneSetWorld(zone, NULL);
  return zone;
};


bool worldRemoveZone(WORLD_DATA *world, ZONE_DATA *zone) {
  return (worldRemoveZoneVnum(world, zoneGetVnum(zone)) != NULL);
};


bool worldSave(WORLD_DATA *world, const char *dirpath) {
  char buf[MAX_BUFFER];
  STORAGE_SET       *set = new_storage_set();
  STORAGE_SET_LIST *list = new_storage_list();
  store_list(set, "zones", list);

  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA       *zone = NULL;
  // save each zone to its own directory, and also put
  // its number in the save file for the zone list
  ITERATE_LIST(zone, zone_i) {
    if(zoneSave(zone)) {
      STORAGE_SET *zone_set = new_storage_set();
      store_int(zone_set, "vnum", zoneGetVnum(zone));
      storage_list_put(list, zone_set);
    }
  }
  deleteListIterator(zone_i);

  sprintf(buf, "%s/world", dirpath);
  storage_write(set, buf);
  storage_close(set);
  return TRUE;
}


WORLD_DATA *worldLoad(const char *dirpath) {
  WORLD_DATA      *world = newWorld(dirpath);
  char buf[MAX_BUFFER];
  sprintf(buf, "%s/world", dirpath);

  STORAGE_SET       *set = storage_read(buf);
  STORAGE_SET_LIST *list = read_list(set, "zones");
  STORAGE_SET  *zone_set = NULL;

  while( (zone_set = storage_list_next(list)) != NULL) {
    ZONE_DATA *zone = NULL;
    int        vnum = read_int(zone_set, "vnum");
    if(zoneIsOldFormat(world, vnum))
      zone = zoneLoadOld(world, vnum);
    else
      zone = zoneLoad(world, vnum);

    if(zone != NULL)
      listPut(world->zones, zone);
  }
  storage_close(set);

  return world;
}

void worldPulse(WORLD_DATA *world) {
  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA *zone;

  ITERATE_LIST(zone, zone_i)
    zonePulse(zone);
  deleteListIterator(zone_i);
}

void worldForceReset(WORLD_DATA *world) {
  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA *zone;

  ITERATE_LIST(zone, zone_i)
    zoneForceReset(zone);
  deleteListIterator(zone_i);
}



//*****************************************************************************
//
// set and get functions
//
//*****************************************************************************

//
// Search through all of the zones in this world, and return the one
// that has min/max vnums that bound this vnum
//
ZONE_DATA *worldZoneBounding(WORLD_DATA *world, int vnum) {
  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA *zone = NULL;

  ITERATE_LIST(zone, zone_i)
    if(zoneGetMinBound(zone) <= vnum && zoneGetMaxBound(zone) >= vnum)
      break;

  deleteListIterator(zone_i);
  return zone;
};

LIST *worldGetZones(WORLD_DATA *world) {
  return world->zones;
}

ZONE_DATA  *worldGetZone(WORLD_DATA *world, int vnum) {
  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA *zone = NULL;
  bool zone_found = FALSE;

  ITERATE_LIST(zone, zone_i) {
    if(zoneGetVnum(zone) == vnum) {
      zone_found = TRUE;
      break;
    }
  } deleteListIterator(zone_i);

  return (zone_found ? zone : NULL);
};

const char *worldGetZonePath(WORLD_DATA *world, int vnum) {
  static char buf[SMALL_BUFFER];
  sprintf(buf, "%s/%d", world->path, vnum);
  return buf;
}

//
// The generic world "get". getter must be the function that
// needs to be used to get the thing we want to get from a zone.
//
void *worldGet(WORLD_DATA *world, void *getter, int vnum) {
  void *(* get_func)(ZONE_DATA *, int) = getter;

  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA       *zone = NULL;
  void            *data = NULL;

  // find the zone that contains our vnum
  ITERATE_LIST(zone, zone_i) {
    // we've found it
    if(zoneGetMinBound(zone) <= vnum && zoneGetMaxBound(zone) >= vnum) {
      data = get_func(zone, vnum);
      break;
    }
  }
  deleteListIterator(zone_i);
  return data;
}

ROOM_DATA  *worldGetRoom(WORLD_DATA *world, int vnum) {
  ROOM_DATA *room = worldGet(world, zoneGetRoom, vnum);
  // if it exists, we might as well toss it
  // into the global table for future reference
  if(room != NULL)
    propertyTablePut(world->rooms, room);
  return room;
};

CHAR_DATA  *worldGetMob(WORLD_DATA *world, int vnum) {
  return worldGet(world, zoneGetMob, vnum);
};

OBJ_DATA  *worldGetObj(WORLD_DATA *world, int vnum) {
  return worldGet(world, zoneGetObj, vnum);
};

SCRIPT_DATA  *worldGetScript(WORLD_DATA *world, int vnum) {
  return worldGet(world, zoneGetScript, vnum);
};

DIALOG_DATA  *worldGetDialog(WORLD_DATA *world, int vnum) {
  return worldGet(world, zoneGetDialog, vnum);
};


//
// generic function for saving something to disk
bool worldSaveThing(WORLD_DATA *world, void *zone_save_func, void *thing, 
		    int vnum) {
  void (* saver)(ZONE_DATA *zone, void *thing) = zone_save_func;
  ZONE_DATA *zone = worldZoneBounding(world, vnum);
  if(zone == NULL)
    return FALSE;
  else {
    saver(zone, thing);
    return TRUE;
  }
}

bool worldSaveRoom(WORLD_DATA *world, ROOM_DATA *room) {
  return worldSaveThing(world, zoneSaveRoom, room, roomGetVnum(room));
}

bool worldSaveMob(WORLD_DATA *world, CHAR_DATA *ch) {
  return worldSaveThing(world, zoneSaveMob, ch, charGetVnum(ch));
}

bool worldSaveObj(WORLD_DATA *world, OBJ_DATA *obj) {
  return worldSaveThing(world, zoneSaveObj, obj, objGetVnum(obj));
}

bool worldSaveScript(WORLD_DATA *world, SCRIPT_DATA *script) {
    return worldSaveThing(world, zoneSaveScript, script, scriptGetVnum(script));
}

bool worldSaveDialog(WORLD_DATA *world, DIALOG_DATA *dialog) {
    return worldSaveThing(world, zoneSaveDialog, dialog, dialogGetVnum(dialog));
}


void worldPutZone(WORLD_DATA *world, ZONE_DATA *zone) {
  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA *tmpzone = NULL;

  // make sure there are no conflicts with other zones ...
  ITERATE_LIST(tmpzone, zone_i) {
    // do our ranges overlap at all?
    if( (zoneGetMinBound(tmpzone) >= zoneGetMinBound(zone) &&
	 zoneGetMinBound(tmpzone) <= zoneGetMaxBound(zone))   ||
	(zoneGetMaxBound(tmpzone) >= zoneGetMinBound(zone) &&
	 zoneGetMaxBound(tmpzone) <= zoneGetMaxBound(zone))) {
      log_string("ERROR: tried to add new zone %d, but its range overlapped "
		 "with zone %d!", zoneGetVnum(zone), zoneGetVnum(tmpzone));
      return;
    }
    // do we have the same vnum?
    else if(zoneGetVnum(zone) == zoneGetVnum(tmpzone)) {
      log_string("ERROR: tried to add new zone %d, but the world already has "
		 "a zone with that vnum!", zoneGetVnum(zone));
      return;
    }
  }
  deleteListIterator(zone_i);

  // make our directory and subdirectories
  char buf[MAX_BUFFER];
  mkdir(worldGetZonePath(world, zoneGetVnum(zone)), S_IRWXU | S_IRWXG);
  sprintf(buf, "%s/room", worldGetZonePath(world, zoneGetVnum(zone)));
  mkdir(buf, S_IRWXU | S_IRWXG);
  sprintf(buf, "%s/mob", worldGetZonePath(world, zoneGetVnum(zone)));
  mkdir(buf, S_IRWXU | S_IRWXG);
  sprintf(buf, "%s/obj", worldGetZonePath(world, zoneGetVnum(zone)));
  mkdir(buf, S_IRWXU | S_IRWXG);
  sprintf(buf, "%s/dialog", worldGetZonePath(world, zoneGetVnum(zone)));
  mkdir(buf, S_IRWXU | S_IRWXG);
  sprintf(buf, "%s/script", worldGetZonePath(world, zoneGetVnum(zone)));
  mkdir(buf, S_IRWXU | S_IRWXG);

  listPut(world->zones, zone);
  zoneSetWorld(zone, world);
};


//
// The generic world "put". putter must be the function that
// needs to be used to put the thing we want to put into a zone.
// return true if successful, and false otherwise
//
void worldPut(WORLD_DATA *world, void *putter, void *data, int vnum) {
  void *(* put_func)(ZONE_DATA *, void *) = putter;

  ZONE_DATA *zone = worldZoneBounding(world, vnum);
  // we have no zone for this thing ... don't add it
  if(zone != NULL)
    put_func(zone, data);
}

void worldPutRoom(WORLD_DATA *world, ROOM_DATA *room) {
  worldPut(world, zoneAddRoom, room, roomGetVnum(room));
};

void worldPutMob(WORLD_DATA *world, CHAR_DATA *mob) {
  worldPut(world, zoneAddMob, mob, charGetVnum(mob));
};

void worldPutObj(WORLD_DATA *world, OBJ_DATA *obj) {
  worldPut(world, zoneAddObj, obj, objGetVnum(obj));
};

void worldPutScript(WORLD_DATA *world, SCRIPT_DATA *script) {
  worldPut(world, zoneAddScript, script, scriptGetVnum(script));
};

void worldPutDialog(WORLD_DATA *world, DIALOG_DATA *dialog) {
  worldPut(world, zoneAddDialog, dialog, dialogGetVnum(dialog));
};

bool worldIsThingLoaded(WORLD_DATA *world, 
			bool (* checker)(ZONE_DATA *, int), int vnum) {
  ZONE_DATA *zone = worldZoneBounding(world, vnum);
  if(zone == NULL)
    return FALSE;
  else
    return checker(zone, vnum);
}

bool worldIsRoomLoaded(WORLD_DATA *world, int vnum) {
  return worldIsThingLoaded(world, zoneIsRoomLoaded, vnum);
}

bool worldIsMobLoaded(WORLD_DATA *world, int vnum) {
  return worldIsThingLoaded(world, zoneIsMobLoaded, vnum);
}

bool worldIsObjLoaded(WORLD_DATA *world, int vnum) {
  return worldIsThingLoaded(world, zoneIsObjLoaded, vnum);
}

bool worldIsScriptLoaded(WORLD_DATA *world, int vnum) {
  return worldIsThingLoaded(world, zoneIsScriptLoaded, vnum);
}

bool worldIsDialogLoaded(WORLD_DATA *world, int vnum) {
  return worldIsThingLoaded(world, zoneIsDialogLoaded, vnum);
}

void worldUnloadThing(WORLD_DATA *world,
		      void (* unloader)(ZONE_DATA *, int), int vnum) {
  ZONE_DATA *zone = worldZoneBounding(world, vnum);
  if(zone != NULL)
    unloader(zone, vnum);  
}

void worldUnloadRoom(WORLD_DATA *world, int vnum) {
  worldUnloadThing(world, zoneUnloadRoom, vnum);
}

void worldUnloadMob(WORLD_DATA *world, int vnum) {
  worldUnloadThing(world, zoneUnloadMob, vnum);
}

void worldUnloadObj(WORLD_DATA *world, int vnum) {
  worldUnloadThing(world, zoneUnloadObj, vnum);
}

void worldUnloadScript(WORLD_DATA *world, int vnum) {
  worldUnloadThing(world, zoneUnloadScript, vnum);
}

void worldUnloadDialog(WORLD_DATA *world, int vnum) {
  worldUnloadThing(world, zoneUnloadDialog, vnum);
}
