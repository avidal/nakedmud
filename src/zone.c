//*****************************************************************************
//
// zone.c
//
// A zone is like a stand-alone module, or encounter, within the game (e.g.
// a city in the world), and contains all of the NPCs, objects, rooms, scripts,
// etc... needed for the zone to be complete.
//
//*****************************************************************************

#include <sys/stat.h>

#include "mud.h"
#include "storage.h"
#include "object.h"
#include "character.h"
#include "room.h"
#include "utils.h"
#include "auxiliary.h"
#include "dialog.h"
#include "world.h"
#include "zone.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/script.h"


struct zone_data {
  char   *name;
  char   *editors;
  BUFFER *desc;

  WORLD_DATA     *world;
  PROPERTY_TABLE *rooms;
  PROPERTY_TABLE *scripts;
  PROPERTY_TABLE *dialogs;
  PROPERTY_TABLE *mob_protos;
  PROPERTY_TABLE *obj_protos;
  LIST           *resettables;

  int vnum;
  int min;
  int max;

  int pulse_timer;  // the timer duration
  int pulse;        // how far down have we gone?

  HASHTABLE *auxiliary_data; // additional data installed on us
};


ZONE_DATA *newZone(int vnum, int min, int max) {
  ZONE_DATA *zone   = malloc(sizeof(ZONE_DATA));
  zone->resettables = newList();
  zone->name        = strdup("");
  zone->desc        = newBuffer(1);
  zone->editors     = strdup("");
  zone->vnum        = vnum;
  zone->min         = min;
  zone->max         = max;

  zone->pulse_timer = -1; // never resets
  zone->pulse       = -1;

  zone->world = NULL;
  // maximum of about 5 things/bucket
  zone->rooms      = newPropertyTable(roomGetVnum,   1 + (max-min)/5);
  zone->scripts    = newPropertyTable(scriptGetVnum, 1 + (max-min)/5);  
  zone->dialogs    = newPropertyTable(dialogGetVnum, 1 + (max-min)/5);
  zone->mob_protos = newPropertyTable(charGetVnum,   1 + (max-min)/5);
  zone->obj_protos = newPropertyTable(objGetVnum,    1 + (max-min)/5);

  zone->auxiliary_data = newAuxiliaryData(AUXILIARY_TYPE_ZONE);

  return zone;
}

ZONE_DATA *zoneCopy(ZONE_DATA *zone) {
  ZONE_DATA *newzone = newZone(zone->vnum, zone->min, zone->max);
  zoneCopyTo(zone, newzone);
  return newzone;
}

void zoneCopyTo(ZONE_DATA *from, ZONE_DATA *to) {
  zoneSetName(to, zoneGetName(from));
  zoneSetDesc(to, zoneGetDesc(from));
  zoneSetEditors(to, zoneGetEditors(from));
  to->vnum  = from->vnum;
  to->min   = from->min;
  to->max   = from->max;
  to->pulse_timer = from->pulse_timer;
  to->pulse = from->pulse;
  auxiliaryDataCopyTo(from->auxiliary_data, to->auxiliary_data);
}

void deleteZone(ZONE_DATA *zone){ 
  if(zone->name)        free(zone->name);
  if(zone->desc)        deleteBuffer(zone->desc);
  if(zone->editors)     free(zone->editors);
  if(zone->resettables) deleteListWith(zone->resettables, deleteInteger);

  deletePropertyTable(zone->rooms);
  deletePropertyTable(zone->scripts);
  deletePropertyTable(zone->dialogs);
  deletePropertyTable(zone->mob_protos);
  deletePropertyTable(zone->obj_protos);

  deleteAuxiliaryData(zone->auxiliary_data);

  //*******************************************************************
  // The only time we're deleting a zone is when we're editing the copy
  // in OLC. So, We don't want to delete any of the zone's content when
  // we delete it.
  //*******************************************************************
    
  free(zone);
}


//
// Pulse a zone. i.e. decrement it's reset timer. When the timer hits 0,
// set it back to the max, and reset everything in the zone
//
void zonePulse(ZONE_DATA *zone) { 
  zone->pulse--;
  if(zone->pulse == 0) {
    zone->pulse = zone->pulse_timer;

    // do a bunch of resetting, and stuff
    ROOM_DATA       *room = NULL;
    INTEGER      *integer = NULL;
    LIST_ITERATOR *room_i = newListIterator(zone->resettables);
    ITERATE_LIST(integer, room_i) {
      room = zoneGetRoom(zone, integerGetVal(integer));
      if(room != NULL)
	roomReset(room);
    } deleteListIterator(room_i);
  }
}

void zoneForceReset(ZONE_DATA *zone) {
  zone->pulse = 1;
  zonePulse(zone);
}


//
// generic function that loads something up from disk
void *zoneLoadThing(ZONE_DATA *zone, PROPERTY_TABLE *table, void *reader,
		   const char *datatype, int vnum) {
  void *(* read_func)(STORAGE_SET *set) = reader;
  void *thing = NULL;
  static char buf[MAX_BUFFER];
  sprintf(buf, "%s/%s/%d", worldGetZonePath(zone->world, zone->vnum), 
	  datatype, vnum);
  STORAGE_SET *set = storage_read(buf);
  if(set != NULL) {
    thing = read_func(set);
    propertyTablePut(table, thing);
    storage_close(set);
  }
  return thing;
}


//
// The generic function for reading all of the data for one type of thing in a 
// zone (e.g. room, object, mobile, etc...). Pulls out the list in the storage 
// set, and parses each element of the list, adding it to the zone. This
// function is obsolete as of v2.7, but we need to keep it around so we can
// convert formats.
void zoneReadDataOld(ZONE_DATA *zone, STORAGE_SET *set, void *putter,
		     void *reader) {
  void  *(* put_func)(ZONE_DATA *, void *) = putter;
  LIST *list = gen_read_list(read_list(set, "list"), reader);
  void  *elem = NULL;
  while( (elem = listPop(list)) != NULL)
    put_func(zone, elem);
  deleteList(list);
}


//
// In v2.7, we redid the way zone contents are stored so it is easier to 
// implement lazy loading. This is the function that does all of the old loading
// so we can do format conversions.
ZONE_DATA *zoneLoadOld(WORLD_DATA *world, int vnum) {
  ZONE_DATA *zone = newZone(vnum, 0, 1);
  char fname[SMALL_BUFFER];
  zone->world = world;

  // first, load all of the zone data
  sprintf(fname, "%s/zone", worldGetZonePath(world, zone->vnum));
  STORAGE_SET  *set = storage_read(fname);
  zone->vnum        = read_int   (set, "vnum");
  zone->min         = read_int   (set, "min");
  zone->max         = read_int   (set, "max");
  zone->pulse_timer = read_int   (set, "pulse_timer");
  zoneSetName(zone,   read_string(set, "name"));
  zoneSetDesc(zone,   read_string(set, "desc"));
  zoneSetEditors(zone,read_string(set, "editors"));

  deleteAuxiliaryData(zone->auxiliary_data);
  zone->auxiliary_data = auxiliaryDataRead(read_set(set, "auxiliary"), 
					   AUXILIARY_TYPE_ZONE);
  storage_close(set);

  // now, load in all of our old contents in the old way
  sprintf(fname, "%s/rooms", worldGetZonePath(zone->world, zone->vnum));
  set = storage_read(fname);
  zoneReadDataOld(zone, set, zoneAddRoom, roomRead);
  storage_close(set);

  sprintf(fname, "%s/mobs", worldGetZonePath(zone->world, zone->vnum));
  set = storage_read(fname);
  zoneReadDataOld(zone, set, zoneAddMob, charRead);
  storage_close(set);
    
  sprintf(fname, "%s/objs", worldGetZonePath(zone->world, zone->vnum));
  set = storage_read(fname);
  zoneReadDataOld(zone, set, zoneAddObj, objRead);
  storage_close(set);
    
  sprintf(fname, "%s/dialogs", worldGetZonePath(zone->world, zone->vnum));
  set = storage_read(fname);
  zoneReadDataOld(zone, set, zoneAddDialog, dialogRead);
  storage_close(set);
    
  sprintf(fname, "%s/scripts", worldGetZonePath(zone->world, zone->vnum));
  set = storage_read(fname);
  zoneReadDataOld(zone, set, zoneAddScript, scriptRead);
  storage_close(set);

  return zone;
}


bool zoneIsOldFormat(WORLD_DATA *world, int vnum) {
  // check to see if we have the new directory structure...
  char buf[MAX_BUFFER];
  sprintf(buf, "%s/room", worldGetZonePath(world, vnum));
  return (dir_exists(buf) == FALSE);
}


//
// generic function for saving something to disk
void zoneSaveThing(ZONE_DATA *zone, void *thing, void *storer, int vnum,
		   const char *type) {
  STORAGE_SET *(* store_func)(void *) = storer;
  STORAGE_SET *set = store_func(thing);
  if(set != NULL) {
    static char buf[MAX_BUFFER];
    sprintf(buf, "%s/%s/%d", worldGetZonePath(zone->world, zone->vnum), 
	    type, vnum);
    storage_write(set, buf);
    storage_close(set);
  }
}


//
// saves all of the contents of a table. This is used after we convert a zone
// from the old format to our new format.
void zoneSaveTable(ZONE_DATA *zone, PROPERTY_TABLE *table, void *storer,
		   const char *type) {
  int      i = zoneGetMinBound(zone);
  void *data = NULL;
  for(; i <= zoneGetMaxBound(zone); i++) {
    data = propertyTableGet(table, i);
    if(data != NULL)
      zoneSaveThing(zone, data, storer, i, type);
  }
}


void zoneConvertFormat(ZONE_DATA *zone) {
  char buf[MAX_BUFFER];
  // time to do the conversion. First, make all of our new directories
  sprintf(buf, "%s/room", worldGetZonePath(zone->world, zone->vnum));
  mkdir(buf, S_IRWXU | S_IRWXG);
  sprintf(buf, "%s/mob",  worldGetZonePath(zone->world, zone->vnum));
  mkdir(buf, S_IRWXU | S_IRWXG);
  sprintf(buf, "%s/obj",  worldGetZonePath(zone->world, zone->vnum));
  mkdir(buf, S_IRWXU | S_IRWXG);
  sprintf(buf, "%s/dialog", worldGetZonePath(zone->world, zone->vnum));
  mkdir(buf, S_IRWXU | S_IRWXG);
  sprintf(buf, "%s/script", worldGetZonePath(zone->world, zone->vnum));
  mkdir(buf, S_IRWXU | S_IRWXG);    

  // now, save all of our tables in the new format
  zoneSaveTable(zone, zone->rooms,      roomStore,   "room");
  zoneSaveTable(zone, zone->mob_protos, charStore,   "mob");
  zoneSaveTable(zone, zone->obj_protos, objStore,    "obj");
  zoneSaveTable(zone, zone->dialogs,    dialogStore, "dialog");
  zoneSaveTable(zone, zone->scripts,    scriptStore, "script");
    
  // go through all of our rooms and figure out which ones have resets, or
  // initialization/reset scripts
  int i = 0;
  for(i = zone->min; i <= zone->max; i++) {
    ROOM_DATA *room = propertyTableGet(zone->rooms, i);
    if(room != NULL && roomIsResettable(room))
      listPut(zone->resettables, newInteger(roomGetVnum(room)));
  }

  // delete all of our old files
  //***********
  // FINISH ME
  //***********
  
  // save all the changes we have made to the zone data file (i.e. resets)
  zoneSave(zone);
}


//
// reads a resettable vnum from a storage set
INTEGER *resettableRead(STORAGE_SET *set) {
  return newInteger(read_int(set, "vnum"));
}


ZONE_DATA *zoneLoad(WORLD_DATA *world, int vnum) {
  ZONE_DATA *zone = newZone(vnum, 0, 1);
  char fname[SMALL_BUFFER];
  zone->world = world;

  // first, load all of the zone data
  sprintf(fname, "%s/zone", worldGetZonePath(world, zone->vnum));
  STORAGE_SET  *set = storage_read(fname);
  zone->vnum        = read_int   (set, "vnum");
  zone->min         = read_int   (set, "min");
  zone->max         = read_int   (set, "max");
  zone->pulse_timer = read_int   (set, "pulse_timer");
  zoneSetName(zone,   read_string(set, "name"));
  zoneSetDesc(zone,   read_string(set, "desc"));
  zoneSetEditors(zone,read_string(set, "editors"));
  deleteListWith(zone->resettables, deleteInteger);
  zone->resettables = gen_read_list(read_list(set, "resettable"), 
				    resettableRead);

  deleteAuxiliaryData(zone->auxiliary_data);
  zone->auxiliary_data = auxiliaryDataRead(read_set(set, "auxiliary"), 
					   AUXILIARY_TYPE_ZONE);
  storage_close(set);

  return zone;
}


//
// stores an integer as a set
STORAGE_SET *resettableStore(INTEGER *integer) {
  STORAGE_SET *set = new_storage_set();
  store_int(set, "vnum", integerGetVal(integer));
  return set;
}


//
// the new zone saving function
bool zoneSave(ZONE_DATA *zone) {
  char fname[MAX_BUFFER];
  
  // first, for our zone data
  sprintf(fname, "%s/zone", worldGetZonePath(zone->world, zone->vnum));
  STORAGE_SET *set = new_storage_set();
  store_int   (set, "vnum",        zone->vnum);
  store_int   (set, "min",         zone->min);
  store_int   (set, "max",         zone->max);
  store_int   (set, "pulse_timer", zone->pulse_timer);
  store_string(set, "name",        zone->name);
  store_string(set, "desc",        bufferString(zone->desc));
  store_string(set, "editors",     zone->editors);
  store_set   (set, "auxiliary",   auxiliaryDataStore(zone->auxiliary_data));
  store_list  (set, "resettable",  
	       gen_store_list(zone->resettables, resettableStore));

  storage_write(set, fname);
  storage_close(set);
  return TRUE;
}



//*****************************************************************************
// add and remove functions
//*****************************************************************************

//
// The generic function for adding data into a zone.
//
bool zoneAdd(ZONE_DATA *zone, PROPERTY_TABLE *table, const char *datatype,
	     void *data, int vnum) {
  if(vnum < zone->min || vnum > zone->max)
    log_string("ERROR: tried to add %s %d to zone %d - "
	       "vnum out of bounds for zone (%d, %d)!",
	       datatype, vnum, zone->vnum, zone->min, zone->max);
  else if(propertyTableGet(table, vnum) != NULL)
    log_string("ERROR: tried to add %s %d to zone %d - "
	       "%s with vnum already exists in zone!",
	       datatype, vnum, zone->vnum, datatype);
  else {
    propertyTablePut(table, data);
    return TRUE;
  }
  return FALSE;
}

void zoneAddRoom(ZONE_DATA *zone, ROOM_DATA *room) { 
  zoneAdd(zone, zone->rooms, "room", room, roomGetVnum(room));
}

void zoneAddMob(ZONE_DATA *zone, CHAR_DATA *mob) {
  zoneAdd(zone, zone->mob_protos, "mobile", mob, charGetVnum(mob)); 
}

void zoneAddObj(ZONE_DATA *zone, OBJ_DATA *obj) {
  zoneAdd(zone, zone->obj_protos, "object", obj, objGetVnum(obj));
}

void zoneAddScript(ZONE_DATA *zone, SCRIPT_DATA *script) {
  zoneAdd(zone, zone->scripts, "script", script, scriptGetVnum(script));
}

void zoneAddDialog(ZONE_DATA *zone, DIALOG_DATA *dialog) {
  zoneAdd(zone, zone->dialogs, "dialog", dialog, dialogGetVnum(dialog));
}

//
// Generic zone remove function
void *zoneRemove(ZONE_DATA *zone, PROPERTY_TABLE *table, void *reader,
		 const char *datatype, int vnum) {
  if(vnum < zone->min || vnum > zone->max) {
    log_string("ERROR: tried to remove %s %d from zone %d - "
	       "vnum out of bounds for zone!",
	       datatype, vnum, zone->vnum);
    return NULL;
  }
  else {
    // if it's not in our property table, we might have to read it in...
    if(!propertyTableGet(table, vnum))
      zoneLoadThing(zone, table, reader, datatype, vnum);
    // unlink it from disk
    char buf[MAX_BUFFER];
    sprintf(buf, "%s/%s/%d", worldGetZonePath(zone->world, zone->vnum), 
	    datatype, vnum);
    unlink(buf);
    // return a pointer to whatever we just removed, if anything...
    return propertyTableRemove(table, vnum);
  }
}

ROOM_DATA *zoneRemoveRoom(ZONE_DATA *zone, int room) {
  zoneRemoveResettableRoom(zone, room);
  return zoneRemove(zone, zone->rooms, roomRead, "room", room);
}

CHAR_DATA *zoneRemoveMob(ZONE_DATA *zone, int mob){ 
  return zoneRemove(zone, zone->mob_protos, charRead, "mob", mob);
}

OBJ_DATA *zoneRemoveObj(ZONE_DATA *zone, int obj){ 
  return zoneRemove(zone, zone->obj_protos, objRead, "obj", obj);
}

SCRIPT_DATA *zoneRemoveScript(ZONE_DATA *zone, int script) { 
  return zoneRemove(zone, zone->scripts, scriptRead, "script", script);
}

DIALOG_DATA *zoneRemoveDialog(ZONE_DATA *zone, int dialog) { 
  return zoneRemove(zone, zone->dialogs, dialogRead, "dialog", dialog);
}

void zoneSaveRoom(ZONE_DATA *zone, ROOM_DATA *room) {
  zoneSaveThing(zone, room, roomStore, roomGetVnum(room), "room");
}

void zoneSaveMob(ZONE_DATA *zone, CHAR_DATA *ch) {
  zoneSaveThing(zone, ch, charStore, charGetVnum(ch), "mob");
}

void zoneSaveObj(ZONE_DATA *zone, OBJ_DATA *obj) {
  zoneSaveThing(zone, obj, objStore, objGetVnum(obj), "obj");
}

void zoneSaveScript(ZONE_DATA *zone, SCRIPT_DATA *script) {
  zoneSaveThing(zone, script, scriptStore, scriptGetVnum(script), "script");
}

void zoneSaveDialog(ZONE_DATA *zone, DIALOG_DATA *dialog) {
  zoneSaveThing(zone, dialog, dialogStore, dialogGetVnum(dialog), "dialog");
}

bool zoneIsRoomLoaded(ZONE_DATA *zone, int vnum) {
  return (propertyTableGet(zone->rooms, vnum) != NULL);
}

bool zoneIsMobLoaded(ZONE_DATA *zone, int vnum) {
  return (propertyTableGet(zone->mob_protos, vnum) != NULL);
}

bool zoneIsObjLoaded(ZONE_DATA *zone, int vnum) {
  return (propertyTableGet(zone->obj_protos, vnum) != NULL);
}

bool zoneIsScriptLoaded(ZONE_DATA *zone, int vnum) {
  return (propertyTableGet(zone->scripts, vnum) != NULL);
}

bool zoneIsDialogLoaded(ZONE_DATA *zone, int vnum) {
  return (propertyTableGet(zone->dialogs, vnum) != NULL);
}

//
// generic unload function for zones
void zoneUnloadThing(ZONE_DATA *zone, PROPERTY_TABLE *table,
		     void *saver, void *deleter, int vnum) {
  void *data = propertyTableRemove(table, vnum);
  if(data != NULL) {
    void   (* save_func)(ZONE_DATA *, void *) = saver;
    void (* delete_func)(void *)              = deleter;
    save_func(zone, data);
    delete_func(data);
  }
}

void zoneUnloadRoom(ZONE_DATA *zone, int vnum) {
  zoneUnloadThing(zone, zone->rooms, zoneSaveRoom, deleteRoom, vnum);
}

void zoneUnloadMob(ZONE_DATA *zone, int vnum) {
  zoneUnloadThing(zone, zone->mob_protos, zoneSaveMob, deleteChar, vnum);
}

void zoneUnloadObj(ZONE_DATA *zone, int vnum) {
  zoneUnloadThing(zone, zone->obj_protos, zoneSaveObj, deleteObj, vnum);
}

void zoneUnloadScript(ZONE_DATA *zone, int vnum) {
  zoneUnloadThing(zone, zone->scripts, zoneSaveScript, deleteScript, vnum);
}

void zoneUnloadDialog(ZONE_DATA *zone, int vnum) {
  zoneUnloadThing(zone, zone->dialogs, zoneSaveDialog, deleteDialog, vnum);
}



//*****************************************************************************
// get and set functions for zones
//*****************************************************************************
bool canEditZone(ZONE_DATA *zone, CHAR_DATA *ch) {
  return (!charIsNPC(ch) && 
	  (is_keyword(zone->editors, charGetName(ch), FALSE) ||
	   bitIsOneSet(charGetUserGroups(ch), "admin")));
}

int zoneGetVnum(ZONE_DATA *zone) { 
  return zone->vnum;
}

int zoneGetMinBound(ZONE_DATA *zone) { 
  return zone->min;
}

int zoneGetMaxBound(ZONE_DATA *zone) { 
  return zone->max;
}

void *zoneGetAuxiliaryData(const ZONE_DATA *zone, char *name) {
  return hashGet(zone->auxiliary_data, name);
}

int getFreeRoomVnum(ZONE_DATA *zone) {
  int i;
  for(i = zone->min; i <= zone->max; i++)
    if(!zoneGetRoom(zone, i))
      return i;
  return NOWHERE;
}

int zoneGetPulseTimer(ZONE_DATA *zone) { 
  return zone->pulse_timer;
}

int zoneGetPulse(ZONE_DATA *zone) { 
  return zone->pulse;
}

WORLD_DATA *zoneGetWorld(ZONE_DATA *zone) { 
  return zone->world;
}

const char *zoneGetName(ZONE_DATA *zone) { 
  return zone->name;
}

const char *zoneGetDesc(ZONE_DATA *zone) { 
  return bufferString(zone->desc);
}

const char *zoneGetEditors(ZONE_DATA *zone) {
  return zone->editors;
}

BUFFER *zoneGetDescBuffer(ZONE_DATA *zone) {
  return zone->desc;
}

//
// Generic get function for zones
//
void *zoneGet(ZONE_DATA *zone, PROPERTY_TABLE *table, void *reader,
	      const char *datatype, int vnum) {
  if(vnum < zone->min || vnum > zone->max) {
    log_string("ERROR: tried to get %s %d from zone %d - "
	       "vnum out of bounds for zone!",
	       datatype, vnum, zone->vnum);
    return NULL;
  }
  else {
    void *data = propertyTableGet(table, vnum);
    // if it's null, try loading it from disk...
    if(data == NULL)
      data = zoneLoadThing(zone, table, reader, datatype, vnum);
    return data;
  }
}

ROOM_DATA *zoneGetRoom(ZONE_DATA *zone, int room) {
  return zoneGet(zone, zone->rooms, roomRead, "room", room);
}

CHAR_DATA *zoneGetMob(ZONE_DATA *zone, int mob) {
  return zoneGet(zone, zone->mob_protos, charRead, "mob", mob);
}

OBJ_DATA *zoneGetObj(ZONE_DATA *zone, int obj) {
  return zoneGet(zone, zone->obj_protos, objRead, "obj", obj);
}

SCRIPT_DATA *zoneGetScript(ZONE_DATA *zone, int script) {
  return zoneGet(zone, zone->scripts, scriptRead, "script", script);
}

DIALOG_DATA *zoneGetDialog(ZONE_DATA *zone, int dialog) {
  return zoneGet(zone, zone->dialogs, dialogRead, "dialog", dialog);
};

void zoneSetVnum(ZONE_DATA *zone, int vnum) { 
  zone->vnum = vnum;
}

void zoneSetMinBound(ZONE_DATA *zone, int min) { 
  zone->min = min;
}

void zoneSetMaxBound(ZONE_DATA *zone, int max) { 
  zone->max = max;
}

void zoneSetPulseTimer(ZONE_DATA *zone, int timer) { 
  // if we normally do not reset, change that
  if(zone->pulse_timer < 0)
    zone->pulse = timer;
  zone->pulse_timer = timer;
}

void zoneSetPulse(ZONE_DATA *zone, int pulse_left) { 
  zone->pulse = pulse_left;
}

void zoneAddResettableRoom(ZONE_DATA *zone, int vnum) {
  ROOM_DATA *room = zoneGetRoom(zone, vnum);
  if(room != NULL) {
    // check to see if we already have an entry...
    INTEGER *integer = newInteger(vnum);
    INTEGER     *ret = listGetWith(zone->resettables, integer, integerCmp);
    if(ret != NULL)
      deleteInteger(integer);
    else {
      listPut(zone->resettables, integer);
      zoneSave(zone);
    }
  }
}

void zoneRemoveResettableRoom(ZONE_DATA *zone, int vnum) {
  ROOM_DATA *room = zoneGetRoom(zone, vnum);
  if(room != NULL) {
    // make something to compare our target against...
    INTEGER *integer = newInteger(vnum);
    INTEGER     *ret = listRemoveWith(zone->resettables, integer, integerCmp);
    deleteInteger(integer);
    if(ret != NULL) {
      deleteInteger(ret);
      zoneSave(zone);
    }
  }
}

void zoneSetWorld(ZONE_DATA *zone, WORLD_DATA *world) { 
  zone->world = world;
}

void zoneSetName(ZONE_DATA *zone, const char *name) { 
  if(zone->name) free(zone->name);
  zone->name = (name ? strdup(name) : strdup("\0"));
}

void zoneSetDesc(ZONE_DATA *zone, const char *desc) { 
  bufferClear(zone->desc);
  bufferCat(zone->desc, desc);
}

void zoneSetEditors(ZONE_DATA *zone, const char *names) {
  if(zone->editors) free(zone->editors);
  zone->editors = strdup( (names ? names : ""));
}
