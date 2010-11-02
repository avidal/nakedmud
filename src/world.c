//*****************************************************************************
//
// world.c
//
// This is the implementation of the WORLD structure.
//
//*****************************************************************************

//
// NOTES FOR FUTURE DEVELOPMENT:
//   * when we remove a room from existence, make sure we clear it
//     of all the characters inhabiting it
//

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

#ifdef MODULE_SCRIPTS
#include "scripts/script.h"
#endif


// the number of rooms we would expect, in different sized worlds
#define SMALL_WORLD       3000
#define MEDIUM_WORLD      7000
#define LARGE_WORLD      15000
#define HUGE_WORLD      250000


struct world_data {
  PROPERTY_TABLE *rooms;  // this table is a communal table for rooms.
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
WORLD_DATA *newWorld() {
  WORLD_DATA *world = malloc(sizeof(WORLD_DATA));

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


ROOM_DATA *worldRemoveRoomVnum(WORLD_DATA *world, room_vnum vnum) {
  propertyTableRemove(world->rooms, vnum);
  return worldRemoveVnum(world, zoneRemoveRoom, vnum);
};


CHAR_DATA *worldRemoveMobVnum(WORLD_DATA *world, mob_vnum vnum) {
  return worldRemoveVnum(world, zoneRemoveMob, vnum);
};


OBJ_DATA *worldRemoveObjVnum(WORLD_DATA *world, obj_vnum vnum) {
  return worldRemoveVnum(world, zoneRemoveObj, vnum);
};

#ifdef MODULE_SCRIPTS
SCRIPT_DATA *worldRemoveScriptVnum(WORLD_DATA *world, script_vnum vnum) {
  return worldRemoveVnum(world, zoneRemoveScript, vnum);
};
#endif

DIALOG_DATA *worldRemoveDialogVnum(WORLD_DATA *world, dialog_vnum vnum) {
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

#ifdef MODULE_SCRIPTS
bool worldRemoveScript(WORLD_DATA *world, SCRIPT_DATA *script) {
  return (worldRemoveScriptVnum(world, scriptGetVnum(script)) != NULL);
};
#endif

bool worldRemoveDialog(WORLD_DATA *world, DIALOG_DATA *dialog) {
  return (worldRemoveDialogVnum(world, dialogGetVnum(dialog)) != NULL);
};


ZONE_DATA *worldRemoveZoneVnum(WORLD_DATA *world, zone_vnum vnum) {
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
  store_list(set, "zones", list, NULL);

  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA       *zone = NULL;
  // save each zone to its own directory, and also put
  // its number in the save file for the zone list
  ITERATE_LIST(zone, zone_i) {
    sprintf(buf, "%s/%d", dirpath, zoneGetVnum(zone));
    // make sure the directory is made
    mkdir(buf, S_IRWXU);

    if(zoneSave(zone, buf)) {
      STORAGE_SET *zone_set = new_storage_set();
      store_int(zone_set, "vnum", zoneGetVnum(zone), NULL);
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
  WORLD_DATA *world = newWorld();
  char buf[MAX_BUFFER];
  sprintf(buf, "%s/world", dirpath);

  STORAGE_SET       *set = storage_read(buf);
  STORAGE_SET_LIST *list = read_list(set, "zones");
  STORAGE_SET  *zone_set = NULL;

  while( (zone_set = storage_list_next(list)) != NULL) {
    sprintf(buf, "%s/%d", dirpath, read_int(zone_set, "vnum"));
    ZONE_DATA *zone = zoneLoad(buf);
    if(zone) worldPutZone(world, zone);
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
ZONE_DATA *worldZoneBounding(WORLD_DATA *world, room_vnum vnum) {
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

ZONE_DATA  *worldGetZone(WORLD_DATA *world, zone_vnum vnum) {
  LIST_ITERATOR *zone_i = newListIterator(world->zones);
  ZONE_DATA *zone = NULL;

  ITERATE_LIST(zone, zone_i)
    if(zoneGetVnum(zone) == vnum)
      break;
  deleteListIterator(zone_i);

  return zone;
};


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

ROOM_DATA  *worldGetRoom(WORLD_DATA *world, room_vnum vnum) {
  ROOM_DATA *room = worldGet(world, zoneGetRoom, vnum);
  // if it exists, we might as well toss it
  // into the global table for future reference
  if(room != NULL)
    propertyTablePut(world->rooms, room);
  return room;
};

CHAR_DATA  *worldGetMob(WORLD_DATA *world, mob_vnum vnum) {
  return worldGet(world, zoneGetMob, vnum);
};

OBJ_DATA  *worldGetObj(WORLD_DATA *world, obj_vnum vnum) {
  return worldGet(world, zoneGetObj, vnum);
};

#ifdef MODULE_SCRIPTS
SCRIPT_DATA  *worldGetScript(WORLD_DATA *world, script_vnum vnum) {
  return worldGet(world, zoneGetScript, vnum);
};
#endif

DIALOG_DATA  *worldGetDialog(WORLD_DATA *world, dialog_vnum vnum) {
  return worldGet(world, zoneGetDialog, vnum);
};


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

#ifdef MODULE_SCRIPTS
void worldPutScript(WORLD_DATA *world, SCRIPT_DATA *script) {
  worldPut(world, zoneAddScript, script, scriptGetVnum(script));
};
#endif

void worldPutDialog(WORLD_DATA *world, DIALOG_DATA *dialog) {
  worldPut(world, zoneAddDialog, dialog, dialogGetVnum(dialog));
};
