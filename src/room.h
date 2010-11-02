#ifndef __ROOM_H
#define __ROOM_H
//*****************************************************************************
//
// room.h
//
// the interface for working with the room structure.
//
//*****************************************************************************



#define DIR_NONE       (-1)
#define DIR_NORTH         0
#define DIR_EAST          1
#define DIR_SOUTH         2
#define DIR_WEST          3
#define DIR_UP            4
#define DIR_DOWN          5
#define DIR_NORTHEAST     6
#define DIR_SOUTHEAST     7
#define DIR_SOUTHWEST     8
#define DIR_NORTHWEST     9
#define NUM_DIRS         10


#define TERRAIN_NONE         (-1)
#define TERRAIN_INDOORS         0	
#define TERRAIN_CITY            1	
#define TERRAIN_ROAD            2
#define TERRAIN_BRIDGE          3 
#define TERRAIN_SHALLOW_WATER   4	
#define TERRAIN_DEEP_WATER      5	
#define TERRAIN_OCEAN           6
#define TERRAIN_UNDERWATER	7	
#define TERRAIN_FIELD           8	
#define TERRAIN_PLAINS          9 
#define TERRAIN_MEADOW          10
#define TERRAIN_FOREST          11
#define TERRAIN_DEEP_FOREST     12 
#define TERRAIN_HILLS           13
#define TERRAIN_HIGH_HILLS      14
#define TERRAIN_MOUNTAIN        15
#define TERRAIN_SWAMP           16 
#define TERRAIN_DEEP_SWAMP      17
#define TERRAIN_SAND            18 
#define TERRAIN_DESERT          19
#define TERRAIN_ICE             20
#define TERRAIN_GLACIER         21
#define TERRAIN_CAVERN          22
#define NUM_TERRAINS            23


//
// Create a clean, new room
//
ROOM_DATA *newRoom();


//
// Delete a room, and detach it from all of its dependant structures
//
void deleteRoom(ROOM_DATA *room);


//
// run the room's reset scripts
//
void roomReset(ROOM_DATA *room);


//
// make a copy of the room
//
ROOM_DATA *roomCopy(ROOM_DATA *room);


//
// Copy one room to another
//
void roomCopyTo(ROOM_DATA *from, ROOM_DATA *to);


//
// parse and store room data
//
ROOM_DATA    *roomRead(STORAGE_SET *set);
STORAGE_SET *roomStore(ROOM_DATA *room);


//*****************************************************************************
//
// add and remove functions
//
//*****************************************************************************

void       roomRemoveChar     (ROOM_DATA *room, const CHAR_DATA *ch);
void       roomRemoveObj      (ROOM_DATA *room, const OBJ_DATA *obj);

void       roomAddChar        (ROOM_DATA *room, CHAR_DATA *ch);
void       roomAddObj         (ROOM_DATA *room, OBJ_DATA *obj);

void       roomDigExit        (ROOM_DATA *room, int dir, room_vnum to);
void       roomDigExitSpecial (ROOM_DATA *room, const char *dir, room_vnum to);

//*****************************************************************************
//
// get and set functions for rooms
//
//*****************************************************************************

room_vnum   roomGetVnum        (const ROOM_DATA *room);
const char *roomGetName        (const ROOM_DATA *room);
const char *roomGetDesc        (const ROOM_DATA *room);
int         roomGetTerrain     (const ROOM_DATA *room);
 // used for editing room descs with the text editor
char      **roomGetDescPtr     (ROOM_DATA *room);

EXIT_DATA  *roomGetExit         (const ROOM_DATA *room, int dir);
EXIT_DATA  *roomGetExitSpecial  (const ROOM_DATA *room, const char *dir);
char      **roomGetExitNames    (const ROOM_DATA *room, int *num);
EDESC_SET  *roomGetEdescs       (const ROOM_DATA *room);
const char *roomGetEdesc        (const ROOM_DATA *room, const char *keyword);
void       *roomGetAuxiliaryData(const ROOM_DATA *room, const char *name);

LIST       *roomGetCharacters  (const ROOM_DATA *room);
LIST       *roomGetContents    (const ROOM_DATA *room);

LIST       *roomGetResets      (const ROOM_DATA *room);
void        roomRemoveReset    (ROOM_DATA *room, RESET_DATA *reset);
void        roomAddReset       (ROOM_DATA *room, RESET_DATA *reset);

void        roomSetEdescs      (ROOM_DATA *room, EDESC_SET *edescs);
void        roomSetVnum        (ROOM_DATA *room, room_vnum vnum);
void        roomSetName        (ROOM_DATA *room, const char *name);
void        roomSetDesc        (ROOM_DATA *room, const char *desc);
void        roomSetTerrain     (ROOM_DATA *room, int terrain_type);

void        roomSetExit        (ROOM_DATA *room,int dir, EXIT_DATA *exit);
void        roomSetExitSpecial (ROOM_DATA *room,const char *dir, EXIT_DATA *exit);




//*****************************************************************************
//
// direction stuff
//
//*****************************************************************************
const char *dirGetName(int dir);
const char *dirGetAbbrev(int dir);
int dirGetOpposite(int dir);

int dirGetNum(const char *dir);
int dirGetAbbrevNum(const char *dir);


//*****************************************************************************
//
// terrain stuff
//
//*****************************************************************************
const char *terrainGetName(int terrain);
const char *terrainGetDefaultRname(int terrain);
const char *terrainGetDefaultRdesc(int terrain);
const char *terrainGetMapSymbol(int terrain);
double      terrainGetVisibility(int terrain); // [0, 1]
double      terrainGetExhaustion(int terrain); // [0, 1]

int terrainGetNum(const char *terrain);


#endif // __ROOM_H
