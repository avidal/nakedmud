#ifndef __WORLD_H
#define __WORLD_H
//*****************************************************************************
//
// world.h
//
// This is the interface for working with the world structure.
//
//*****************************************************************************


//
// Create a new world from the path. The path is the directory the world
// data is stored in.
//
WORLD_DATA *newWorld();


//
// Delete the world from memory.
//
void deleteWorld(WORLD_DATA *world);


//
// Removes the room with the ID from the world, and returns it.
// returns NULL if the room does not exist.
//
ROOM_DATA *worldRemoveRoomVnum(WORLD_DATA *world, int vnum);
CHAR_DATA  *worldRemoveMobVnum (WORLD_DATA *world, int  vnum);
OBJ_DATA  *worldRemoveObjVnum (WORLD_DATA *world, int  vnum);
SCRIPT_DATA *worldRemoveScriptVnum(WORLD_DATA *world, int vnum);
DIALOG_DATA *worldRemoveDialogVnum(WORLD_DATA *world, int vnum);
ZONE_DATA *worldRemoveZoneVnum(WORLD_DATA *world, int vnum);


//
// Removes (no delete) the thing from the world. Returns true if successful,
// and false if it does not exist in the world.
//
bool worldRemoveRoom(WORLD_DATA *world, ROOM_DATA *room);
bool worldRemoveMob (WORLD_DATA *world, CHAR_DATA  *mob);
bool worldRemoveObj (WORLD_DATA *world, OBJ_DATA  *obj);
bool worldRemoveScript(WORLD_DATA *world, SCRIPT_DATA *script);
bool worldRemoveDialog(WORLD_DATA *world, DIALOG_DATA *dialog);
bool worldRemoveZone(WORLD_DATA *world, ZONE_DATA *zone);


//
// Saves the world to disk at the specified directory path
//
bool worldSave(WORLD_DATA *world, const char *dirpath);


//
// Loads a world from disk
//
WORLD_DATA *worldLoad(const char *dirpath);

//
// Pulse all of the zones in the world
//
void worldPulse(WORLD_DATA *world);
void worldForceReset(WORLD_DATA *world);



//*****************************************************************************
//
// set and get functions
//
//*****************************************************************************
ZONE_DATA   *worldZoneBounding(WORLD_DATA *world, int vnum);
ZONE_DATA   *worldGetZone(WORLD_DATA *world, int vnum);
ROOM_DATA   *worldGetRoom(WORLD_DATA *world, int vnum);
CHAR_DATA    *worldGetMob (WORLD_DATA *world, int  vnum);
OBJ_DATA    *worldGetObj (WORLD_DATA *world, int  vnum);
SCRIPT_DATA *worldGetScript(WORLD_DATA *world, int vnum);
DIALOG_DATA *worldGetDialog(WORLD_DATA *world, int vnum);
LIST        *worldGetZones(WORLD_DATA *world);

void        worldPutZone(WORLD_DATA *world, ZONE_DATA *zone);
void        worldPutRoom(WORLD_DATA *world, ROOM_DATA *room);
void        worldPutMob (WORLD_DATA *world, CHAR_DATA  *mob);
void        worldPutObj (WORLD_DATA *world, OBJ_DATA  *obj);
void        worldPutScript(WORLD_DATA *world, SCRIPT_DATA *script);
void        worldPutDialog(WORLD_DATA *world, DIALOG_DATA *dialog);

#endif // __WORLD_H
