#ifndef __ZONE_H
#define __ZONE_H
//*****************************************************************************
//
// zone.h
//
// A zone is like a stand-alone module, or encounter, within the game (e.g.
// a city in the world), and contains all of the NPCs, objects, rooms, scripts,
// etc... needed for the zone to be complete.
//
//*****************************************************************************


//
// Create a new zone, with vnums bounded between (inclusive) min and max
ZONE_DATA *newZone(int vnum, int min, int max);

//
// Delete a zone, plus all of the prototypes contained within.
// CONSIDERATION: Should we delete rooms as well, and deposit
//                players into some sort of limbo, or should
//                we leave rooms as-is? I think we should probably
//                delete the rooms, and deposit the players in a
//                room that is known to always exist (limbo?)
void deleteZone(ZONE_DATA *zone);

//
// Load a zone from disk. 
ZONE_DATA *zoneLoad(WORLD_DATA *world, int vnum);

//
// load a zone from disk. Uses the old storage scheme
ZONE_DATA *zoneLoadOld(WORLD_DATA *world, int vnum);
bool   zoneIsOldFormat(WORLD_DATA *world, int vnum);
void zoneConvertFormat(ZONE_DATA  *zone);

//
// Save a zone to the specified directory path
bool zoneSave(ZONE_DATA *zone);

//
// Pulse a zone. i.e. decrement it's reset timer. When the timer hits 0,
// set it back to the max, and reset everything in the zone
void      zonePulse(ZONE_DATA *zone);
void zoneForceReset(ZONE_DATA *zone);

//
// Copy zone-specific data, but not contents in the zone (no rooms, mobs
// objs, scripts, etc)
ZONE_DATA *zoneCopy(ZONE_DATA *zone);
void     zoneCopyTo(ZONE_DATA *from, ZONE_DATA *to);



//*****************************************************************************
// add, remove, and save functions
//*****************************************************************************

//
// add the thing to the zone, but do not save it to disk
void   zoneAddRoom(ZONE_DATA *zone, ROOM_DATA *room);
void    zoneAddMob(ZONE_DATA *zone, CHAR_DATA *mob);
void    zoneAddObj(ZONE_DATA *zone, OBJ_DATA *obj);
void zoneAddScript(ZONE_DATA *zone, SCRIPT_DATA *script);
void zoneAddDialog(ZONE_DATA *zone, DIALOG_DATA *dialog);

//
// remove the thing from the zone (including removing it from disk)
ROOM_DATA     *zoneRemoveRoom(ZONE_DATA *zone, int room);
CHAR_DATA      *zoneRemoveMob(ZONE_DATA *zone, int mob);
OBJ_DATA       *zoneRemoveObj(ZONE_DATA *zone, int obj);
SCRIPT_DATA *zoneRemoveScript(ZONE_DATA *zone, int script);
DIALOG_DATA *zoneRemoveDialog(ZONE_DATA *zone, int script);

//
// save the thing to disk
void   zoneSaveRoom(ZONE_DATA *zone, ROOM_DATA   *room);
void    zoneSaveMob(ZONE_DATA *zone, CHAR_DATA   *ch);
void    zoneSaveObj(ZONE_DATA *zone, OBJ_DATA    *obj);
void zoneSaveScript(ZONE_DATA *zone, SCRIPT_DATA *script);
void zoneSaveDialog(ZONE_DATA *zone, DIALOG_DATA *dialog);

//
// return whether or not the thing is loaded in RAM
bool   zoneIsRoomLoaded(ZONE_DATA *zone, int vnum);
bool    zoneIsMobLoaded(ZONE_DATA *zone, int vnum);
bool    zoneIsObjLoaded(ZONE_DATA *zone, int vnum);
bool zoneIsScriptLoaded(ZONE_DATA *zone, int vnum);
bool zoneIsDialogLoaded(ZONE_DATA *zone, int vnum);

//
// unloads something from memory. Assumes it is not being used by anything
// at the moment (e.g. rooms don't have chars/objects in them)
void   zoneUnloadRoom(ZONE_DATA *zone, int vnum);
void    zoneUnloadMob(ZONE_DATA *zone, int vnum);
void    zoneUnloadObj(ZONE_DATA *zone, int vnum);
void zoneUnloadScript(ZONE_DATA *zone, int vnum);
void zoneUnloadDialog(ZONE_DATA *zone, int vnum);



//*****************************************************************************
// get and set functions for zones
//*****************************************************************************

//
// various get functions for zones
int            zoneGetVnum(ZONE_DATA *zone);
int        zoneGetMinBound(ZONE_DATA *zone);
int        zoneGetMaxBound(ZONE_DATA *zone);
int        getFreeRoomVnum(ZONE_DATA *zone);
int      zoneGetPulseTimer(ZONE_DATA *zone);
int           zoneGetPulse(ZONE_DATA *zone);
WORLD_DATA   *zoneGetWorld(ZONE_DATA *zone);
const char    *zoneGetName(ZONE_DATA *zone);
const char    *zoneGetDesc(ZONE_DATA *zone);
const char *zoneGetEditors(ZONE_DATA *zone);
BUFFER  *zoneGetDescBuffer(ZONE_DATA *zone);
ROOM_DATA     *zoneGetRoom(ZONE_DATA *zone, int room);
CHAR_DATA      *zoneGetMob(ZONE_DATA *zone, int room);
OBJ_DATA       *zoneGetObj(ZONE_DATA *zone, int obj);
SCRIPT_DATA *zoneGetScript(ZONE_DATA *zone, int script);
DIALOG_DATA *zoneGetDialog(ZONE_DATA *zone, int dialog);
void *zoneGetAuxiliaryData(const ZONE_DATA *zone, char *name);

//
// various set functions for zones
void       zoneSetVnum(ZONE_DATA *zone, int vnum);
void   zoneSetMinBound(ZONE_DATA *zone, int min);
void   zoneSetMaxBound(ZONE_DATA *zone, int max);
void zoneSetPulseTimer(ZONE_DATA *zone, int timer);
void      zoneSetPulse(ZONE_DATA *zone, int pulse_left);
void      zoneSetWorld(ZONE_DATA *zone, WORLD_DATA *world);
void       zoneSetName(ZONE_DATA *zone, const char *name);
void       zoneSetDesc(ZONE_DATA *zone, const char *description);
void    zoneSetEditors(ZONE_DATA *zone, const char *names);

//
// set a room as needing to be reset when our zone is pulsed
void    zoneAddResettableRoom(ZONE_DATA *zone, int vnum);
void zoneRemoveResettableRoom(ZONE_DATA *zone, int vnum);



//*****************************************************************************
// misc stuff
//*****************************************************************************

//
// returns true if the char has edit priviledges for the zone
bool canEditZone(ZONE_DATA *zone, CHAR_DATA *ch);

#endif // __ZONE_H
