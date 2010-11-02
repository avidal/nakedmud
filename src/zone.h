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
ZONE_DATA *newZone(zone_vnum vnum, room_vnum min, room_vnum max);


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


ROOM_DATA *zoneRemoveRoom(ZONE_DATA *zone, room_vnum room);
CHAR_DATA  *zoneRemoveMob(ZONE_DATA *zone, mob_vnum mob);
OBJ_DATA  *zoneRemoveObj(ZONE_DATA *zone, obj_vnum obj);
SCRIPT_DATA *zoneRemoveScript(ZONE_DATA *zone, script_vnum script);
DIALOG_DATA *zoneRemoveDialog(ZONE_DATA *zone, dialog_vnum script);



//*****************************************************************************
//
// get and set functions for zones
//
//*****************************************************************************
zone_vnum zoneGetVnum(ZONE_DATA *zone);
room_vnum zoneGetMinBound(ZONE_DATA *zone);
room_vnum zoneGetMaxBound(ZONE_DATA *zone);
room_vnum getFreeRoomVnum(ZONE_DATA *zone);
int zoneGetPulseTimer(ZONE_DATA *zone);
int zoneGetPulse(ZONE_DATA *zone);
WORLD_DATA *zoneGetWorld(ZONE_DATA *zone);
const char *zoneGetName(ZONE_DATA *zone);
const char *zoneGetDesc(ZONE_DATA *zone);
const char *zoneGetEditors(ZONE_DATA *zone);
char      **zoneGetDescPtr(ZONE_DATA *zone);
bool        canEditZone(ZONE_DATA *zone, CHAR_DATA *ch);
ROOM_DATA  *zoneGetRoom(ZONE_DATA *zone, room_vnum room);
CHAR_DATA   *zoneGetMob(ZONE_DATA *zone, mob_vnum room);
OBJ_DATA   *zoneGetObj(ZONE_DATA *zone, obj_vnum obj);
SCRIPT_DATA *zoneGetScript(ZONE_DATA *zone, script_vnum script);
DIALOG_DATA *zoneGetDialog(ZONE_DATA *zone, dialog_vnum script);


void zoneSetVnum(ZONE_DATA *zone, zone_vnum vnum);
void zoneSetMinBound(ZONE_DATA *zone, room_vnum min);
void zoneSetMaxBound(ZONE_DATA *zone, room_vnum max);
void zoneSetPulseTimer(ZONE_DATA *zone, int timer);
void zoneSetPulse(ZONE_DATA *zone, int pulse_left);
void zoneSetWorld(ZONE_DATA *zone, WORLD_DATA *world);
void zoneSetName(ZONE_DATA *zone, const char *name);
void zoneSetDescription(ZONE_DATA *zone, const char *description);
void zoneSetEditors(ZONE_DATA *zone, const char *names);
//*****************************************************************************
//
// misc stuff
//
//*****************************************************************************

#endif // __ZONE_H
