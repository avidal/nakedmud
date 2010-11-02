#ifndef __SCRIPT_H
#define __SCRIPT_H
//*****************************************************************************
//
// script.h
//
// All of the information regarding scripts, from the functions to run them
// and all of the different script types.
//
//*****************************************************************************

//
// This must be put at the top of mud.h so the rest of the MUD knows that
// we've got the scripts module installed
// #define MODULE_SCRIPTS
//

//*****************************************************************************
//                            SCRIPT DOCUMENTATION
//*****************************************************************************
// SCRIPT_TYPE_INIT
//   Description:
//     runs when the scriptor is loaded for the first time, or reset in the
//     case of rooms.
//   String Arguments:
//     None
//   Numeric Argument:
//     None
//   Works with:
//     rooms, objects, mobiles
//   Parameters:
//     me   = ourself
//     ch   = the character loaded to (if applicable)
//     room = the room loaded to (if applicable)
//     obj  = the container loaded to (if applicable)

//*****************************************************************************
// SCRIPT_TYPE_SPEECH
//   Description:
//     Runs when the scriptor hears a person in the room say something. This
//     can be through say or ask.
//   Arguments:
//     a comma-separated list of keywords that the script triggers off of
//   Numeric Argument:
//     None
//   Works with:
//     mobiles
//   Parameters:
//     me   = ourself
//     ch   = the person talking
//     room = the room the speech trigger occured in
//     arg  = the speech that was said

//*****************************************************************************
// SCRIPT_TYPE_DROP
//   Description:
//     Runs when the scriptor is dropped (obj) or has something drop to 
//     it (room)
//   Arguments:
//     None
//   Numeric Argument:
//     None
//   Works with:
//     rooms, objects
//   Parameters:
//     me   = ourself
//     ch   = the character doing the dropping
//     room = the room dropped to (if applicable) or ourself
//     obj  = the object that was dropped to us (if applicable) or ourself

//*****************************************************************************
// SCRIPT_TYPE_GIVE
//   Description:
//     Runs when an object is given to the scriptor, or the scriptor is
//     given (in the case that the scriptor is an object)
//   Arguments:
//     None
//   Numeric Arguments:
//     1 if the receiver needs to see the character. 0 otherwise.
//   Works with:
//     objects, mobiles
//   Parameters:
//     me   = ourself
//     ch   = the character doing the giving
//     room = the room the giving is being done in
//     obj  = the object being given (if applicable) or ourself

//*****************************************************************************
// SCRIPT_TYPE_ENTER
//   Description:
//     Runs when a character enters the room we are in, or enters us
//     in the case that we are a room
//   Arguments:
//     None
//   Numeric Arguments:
//     1 if me (a mob watching) needs to see the character. 0 otherwise
//   Works with:
//     rooms, mobiles
//   Parameters:
//     me   = ourself
//     ch   = the character entering the room
//     room = the room we are in (if applicable) or ourself

//*****************************************************************************
// SCRIPT_TYPE_EXIT
//   Description:
//     Runs when a character exits a room.
//   Arguments:
//     None
//   Numeric Arguments:
//     1 if me (a mob watching) needs to see the character. 0 otherwise
//   Works with:
//     rooms, mobiles.
//   Parameters:
//     me   = ourself
//     ch   = the character exiting
//     room = the room being left (if applicable) or ourself
//     cmd  = the direction that ch left through

//*****************************************************************************
// SCRIPT_TYPE_COMMAND
//   Description:
//     Runs when a character enters a command in the argument list
//   Arguments:
//     A comma-separated list of commands that trigger this script
//   Numeric Arguments:
//     0 if the normal command should be followed through with afterwards
//     1 otherwise
//   Works with:
//     rooms, mobiles
//   Parameters:
//     me   = ourself
//     ch   = the person using a command
//     room = the room the command was issued in
//     cmd  = the command
//     arg  = the argument supplied to the command

//*****************************************************************************
#define SCRIPT_TYPE_NONE          (-1)
#define SCRIPT_TYPE_INIT            0 // when the room/mob/obj resets or loads
#define SCRIPT_TYPE_SPEECH          1 // when someone says a keyword
#define SCRIPT_TYPE_DROP            2 // when obj is dropped
#define SCRIPT_TYPE_GIVE            3 // when obj is given
#define SCRIPT_TYPE_ENTER           4 // when a char enters the room
#define SCRIPT_TYPE_EXIT            5 // when the character exits the room 
#define SCRIPT_TYPE_COMMAND         6 // when a command is issued
#define NUM_SCRIPTS                 7


const char *scriptTypeName(int num);

SCRIPT_DATA *newScript   ();
void         deleteScript(SCRIPT_DATA *script);

STORAGE_SET *scriptStore(SCRIPT_DATA *script);
SCRIPT_DATA *scriptRead (STORAGE_SET *set);

SCRIPT_DATA *scriptCopy(SCRIPT_DATA *script);
void         scriptCopyTo(SCRIPT_DATA *from, SCRIPT_DATA *to);

script_vnum scriptGetVnum(SCRIPT_DATA *script);
int         scriptGetType(SCRIPT_DATA *script);
int         scriptGetNumArg(SCRIPT_DATA *script);
const char *scriptGetArgs(SCRIPT_DATA *script);
const char *scriptGetName(SCRIPT_DATA *script);
const char *scriptGetCode(SCRIPT_DATA *script);
char      **scriptGetCodePtr(SCRIPT_DATA *script); // for text editing in olc

void scriptSetVnum(SCRIPT_DATA *script, script_vnum vnum);
void scriptSetType(SCRIPT_DATA *script, int type);
void scriptSetNumArg(SCRIPT_DATA *script, int num_arg);
void scriptSetArgs(SCRIPT_DATA *script, const char *args);
void scriptSetName(SCRIPT_DATA *script, const char *name);
void scriptSetCode(SCRIPT_DATA *script, const char *code);


//
// Functions for accessing auxiliary script data in 
// rooms, characters, and objects.
//
SCRIPT_SET *roomGetScripts(const ROOM_DATA *room);
void        roomSetScripts(ROOM_DATA *room, SCRIPT_SET *scripts);
SCRIPT_SET *objGetScripts(const OBJ_DATA *obj);
void        objSetScripts(OBJ_DATA *obj, SCRIPT_SET *scripts);
SCRIPT_SET *charGetScripts(const CHAR_DATA *mob);
void        charSetScripts(CHAR_DATA *mob, SCRIPT_SET *scripts);




//*****************************************************************************
//
// The interface for running scripts
//
//*****************************************************************************

// different types for things that can trigger/have a script
#define SCRIPTOR_NONE         (-1)
#define SCRIPTOR_CHAR           0
#define SCRIPTOR_ROOM           1
#define SCRIPTOR_OBJ            2

//
// initialize the scripting system
//
void init_scripts();

//
// Shut scripts down
//
void finalize_scripts();


//
// start up a script
//
void run_script(const char *script, void *me, int me_type,
		CHAR_DATA *ch, OBJ_DATA *obj, ROOM_DATA *room,
		const char *cmd, const char *arg, int narg);

//
// format a string so that it is a viable script
//
void format_script(char **script, int max_len);


//
// Show a script to the socket
//
void script_display(SOCKET_DATA *sock, const char *script, bool show_line_nums);


//
// See if a speech script needs to be triggered. If listener == NULL,
// everyone in the room is checked. If a listener is provided, then 
// only the listener is checked
//
void try_speech_script(CHAR_DATA *ch, CHAR_DATA *listener, char *speech);


//
// Try enterance scripts in the given room (the room itself, and mobs)
//
void try_enterance_script(CHAR_DATA *ch, ROOM_DATA *room, const char *dirname);

//
// Try exit scripts in the given room (the room itself, and mobs)
//
void try_exit_script(CHAR_DATA *ch, ROOM_DATA *room, const char *dirname);

//
// Searches for command scripts in the room. If the actual command should
// be followed through with, returns 0. If the actual command should be
// prevented, returns 1.
//
int  try_command_script(CHAR_DATA *ch, const char *cmd, const char *arg);

//
// Check for and run scripts of the given type found on the scriptor.
// If the return value is non-zero, then whatever function that is checking
// the script needs to be halted (e.g. if it is a command script, the command
// parser should not continue checking for a normal command in the command list,
// if the character is giving an object, the give should not complete, etc...)
//
int try_scripts(int script_type,
		void *me, int me_type,
		CHAR_DATA *ch, OBJ_DATA *obj, ROOM_DATA *room,
		const char *cmd, const char *arg, int narg);

#endif //__SCRIPT_H
