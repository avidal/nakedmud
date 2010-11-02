#ifndef __ROOM_RESET_H
#define __ROOM_RESET_H
//*****************************************************************************
//
// room_reset.h
//
// Personally, I prefer to do all of my zone resetting through scripts because
// of the flexibility it allows for. However, I know some people are not
// too inclined towards scripting, so this is an effort to provide an easy
// way for those people to handle room resets.
//
//*****************************************************************************

#define INITIATOR_ROOM        0 // the room initiated the reset call
#define INITIATOR_IN_OBJ      1 // an "in obj" call initiated it
#define INITIATOR_ON_OBJ      2 // an "on obj" call initiated it
#define INITIATOR_THEN_OBJ    3 // an object load/find initiated it
#define INITIATOR_IN_MOB      4 // an "in char" initiated it
#define INITIATOR_ON_MOB      5 // an "on char" initiated it
#define INITIATOR_THEN_MOB    6 // a mobile load/find initiated it

#define RESET_LOAD_OBJECT     0 // load an object
#define RESET_LOAD_MOBILE     1 // load a mobile
#define RESET_FIND_OBJECT     2 // find an object in the room
#define RESET_FIND_MOBILE     3 // find a mobile in the room
#define RESET_PURGE_OBJECT    4 // purge an object
#define RESET_PURGE_MOBILE    5 // purge a mobile
#define RESET_OPEN            6 // open an exit/object
#define RESET_CLOSE           7 // close an exit/object
#define RESET_LOCK            8 // lock an exit/object
#define RESET_POSITION        9 // change the position of a mobile
#define NUM_RESETS           10

const char    *resetTypeGetName (int type);

RESET_DATA    *newReset         ();
void           deleteReset      (RESET_DATA *reset);
RESET_DATA    *resetCopy        (RESET_DATA *reset);
void           resetCopyTo      (RESET_DATA *from, RESET_DATA *to);
STORAGE_SET   *resetStore       (RESET_DATA *reset);
RESET_DATA    *resetRead        (STORAGE_SET *set);

int            resetGetType     (const RESET_DATA *reset);
int            resetGetTimes    (const RESET_DATA *reset);
int            resetGetChance   (const RESET_DATA *reset);
int            resetGetMax      (const RESET_DATA *reset);
int            resetGetRoomMax  (const RESET_DATA *reset);
const char    *resetGetArg      (const RESET_DATA *reset);
LIST          *resetGetOn       (const RESET_DATA *reset);
LIST          *resetGetIn       (const RESET_DATA *reset);
LIST          *resetGetThen     (const RESET_DATA *reset);

void           resetSetType     (RESET_DATA *reset, int type);
void           resetSetTimes    (RESET_DATA *reset, int times);
void           resetSetChance   (RESET_DATA *reset, int chance);
void           resetSetMax      (RESET_DATA *reset, int max);
void           resetSetRoomMax  (RESET_DATA *reset, int room_max);
void           resetSetArg      (RESET_DATA *reset, const char *arg);

void           resetAddOn       (RESET_DATA *reset, RESET_DATA *on);
void           resetAddIn       (RESET_DATA *reset, RESET_DATA *in);
void           resetAddThen     (RESET_DATA *reset, RESET_DATA *then);


//
// try a reset command
//
bool resetRun(RESET_DATA *reset, void *initiator, int initiator_type);


//
// Perform resetRun on all the reset commands in the list, using
// initiator and initiator_type
//
void resetRunOn(LIST *list, void *initiator, int initiator_type);

#endif // __ROOM_RESET_H
