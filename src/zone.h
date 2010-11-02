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
//
ZONE_DATA *newZone(int vnum, int min, int max);


//
// Delete a zone, plus all of the prototypes contained within.
// CONSIDERATION: Should we delete rooms as well, and deposit
//                players into some sort of limbo, or should
//                we leave rooms as-is? I think we should probably
//                delete the rooms, and deposit the players in a
//                room that is known to always exist (limbo?)
//
void deleteZone(ZONE_DATA *zone);


//
// Load a zone from disk. The zone directory path must be
// supplied.
//
ZONE_DATA *zoneLoad(const char *dirpath);


//
// Save a zone to the specified directory path
//
bool zoneSave(ZONE_DATA *zone, const char *dirpath);


//
// Pulse a zone. i.e. decrement it's reset timer. When the timer hits 0,
// set it back to the max, and reset everything in the zone
//
void zonePulse(ZONE_DATA *zone);
void zoneForceReset(ZONE_DATA *zone);


//
// Copy zone-specific data, but not contents in the zone (no rooms, mobs
// objs, scripts, etc)
//
ZONE_DATA *zoneCopy(ZONE_DATA *zone);
void zoneCopyTo(ZONE_DATA *from, ZONE_DATA *to);

//*****************************************************************************
//
// add and remove functions
//
//*****************************************************************************
void zoneAddRoom(ZONE_DATA *zone, ROOM_DATA *room);
void zoneAddMob(ZONE_DATA *zone, CHAR_DATA *mob);
void zoneAddObj(ZONE_DATA *zone, OBJ_DATA *obj);
void zoneAddScript(ZONE_DATA *zone, SCRIPT_DATA *script);
void zoneAddDialog(ZONE_DATA *zone, DIALOG_DATA *dialog);


ROOM_DATA *zoneRemoveRoom(ZONE_DATA *zone, int room);
CHAR_DATA  *zoneRemoveMob(ZONE_DATA *zone, int mob);
OBJ_DATA  *zoneRemoveObj(ZONE_DATA *zone, int obj);
SCRIPT_DATA *zoneRemoveScript(ZONE_DATA *zone, int script);
DIALOG_DATA *zoneRemoveDialog(ZONE_DATA *zone, int script);



//*****************************************************************************
//
// get and set functions for zones
//
//*****************************************************************************
int zoneGetVnum(ZONE_DATA *zone);
int zoneGetMinBound(ZONE_DATA *zone);
int zoneGetMaxBound(ZONE_DATA *zone);
int getFreeRoomVnum(ZONE_DATA *zone);
int zoneGetPulseTimer(ZONE_DATA *zone);
int zoneGetPulse(ZONE_DATA *zone);
WORLD_DATA *zoneGetWorld(ZONE_DATA *zone);
const char *zoneGetName(ZONE_DATA *zone);
const char *zoneGetDesc(ZONE_DATA *zone);
const char *zoneGetEditors(ZONE_DATA *zone);
BUFFER     *zoneGetDescBuffer(ZONE_DATA *zone);
bool        canEditZone(ZONE_DATA *zone, CHAR_DATA *ch);
ROOM_DATA  *zoneGetRoom(ZONE_DATA *zone, int room);
CHAR_DATA   *zoneGetMob(ZONE_DATA *zone, int room);
OBJ_DATA   *zoneGetObj(ZONE_DATA *zone, int obj);
SCRIPT_DATA *zoneGetScript(ZONE_DATA *zone, int script);
DIALOG_DATA *zoneGetDialog(ZONE_DATA *zone, int dialog);
void        *zoneGetAuxiliaryData(const ZONE_DATA *zone, char *name);

void zoneSetVnum(ZONE_DATA *zone, int vnum);
void zoneSetMinBound(ZONE_DATA *zone, int min);
void zoneSetMaxBound(ZONE_DATA *zone, int max);
void zoneSetPulseTimer(ZONE_DATA *zone, int timer);
void zoneSetPulse(ZONE_DATA *zone, int pulse_left);
void zoneSetWorld(ZONE_DATA *zone, WORLD_DATA *world);
void zoneSetName(ZONE_DATA *zone, const char *name);
void zoneSetDesc(ZONE_DATA *zone, const char *description);
void zoneSetEditors(ZONE_DATA *zone, const char *names);
//*****************************************************************************
//
// misc stuff
//
//*****************************************************************************

#endif // __ZONE_H
