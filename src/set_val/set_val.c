//*****************************************************************************
//
// set_val.c
//
// There are often times in-game when we will want to change the values of a
// field in a character or object; most DIKU muds do this via a "set" command
// that simply has a list of all of the fields, and how to handle performing
// the set. However, because of our modular nature, it would be fairly 
// cumbersome to have all of the possible things to set in one big list. 
// Instead, we provide a way that modules can install new fields that can be
// set for rooms, objects, and mobiles.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../world.h"
#include "../zone.h"
#include "../socket.h"
#include "../character.h"
#include "../object.h"
#include "../room.h"
#include "../races.h"
#include "../handler.h"
#include "../save.h"

#include "set_val.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../editor/editor.h" // for charGetNotepad



//*****************************************************************************
// setter functions
//*****************************************************************************

//
// most everything already has a setter function done up, but because user
// groups go through a bitvector, we need an extra function to get in and be
// able to set a character's groups...
void charSetUserGroups(CHAR_DATA *ch, const char *groups) {
  bitClear(charGetUserGroups(ch));
  bitSet(charGetUserGroups(ch), groups);
}



//*****************************************************************************
// local defines, datastructures, functions, and commands
//*****************************************************************************
HASHTABLE *char_set_table = NULL;
HASHTABLE *obj_set_table  = NULL;
HASHTABLE *room_set_table = NULL;

typedef struct set_val_data {
  int   type;    // the type of data (int, double, long, string, etc...)
  void *setter;  // the function to do the setting
  void *checker; // make sure the new value is an acceptable value
} SET_VAL_DATA;


SET_VAL_DATA *newSetValData(int type, void *setter, void *checker) {
  SET_VAL_DATA *data = malloc(sizeof(SET_VAL_DATA));
  data->type    = type;
  data->setter  = setter;
  data->checker = checker;
  return data;
}

void deleteSetValData(SET_VAL_DATA *data) {
  free(data);
}


void try_set(CHAR_DATA *ch, void *tgt, HASHTABLE *table, 
	     const char *field, const char *val) {
  SET_VAL_DATA *data = hashGet(table, field);
  if(data == NULL)
    send_to_char(ch, "You cannot set that field!\r\n");
  else {
    // make sure this is an acceptable value
    bool set_ok = TRUE;
    if(data->checker != NULL) {
      if(data->type == SET_TYPE_INT)
	set_ok = ((bool (*)(int)) data->checker)(atoi(val));
      else if(data->type == SET_TYPE_LONG)
	set_ok = ((bool (*)(long)) data->checker)(atol(val));
      else if(data->type == SET_TYPE_DOUBLE)
	set_ok = ((bool (*)(double)) data->checker)(atof(val));
      else if(data->type == SET_TYPE_STRING)
	set_ok = ((bool (*)(const char *)) data->checker)(val);
    }

    // make sure the set is ok
    if(set_ok == FALSE) {
      send_to_char(ch, "'%s' is not an acceptable value!\r\n", val);
      return;
    }
    
    // perform the change
    if(data->type == SET_TYPE_INT)
      ((void (*)(void *, int)) data->setter)(tgt, atoi(val));
    else if(data->type == SET_TYPE_LONG)
      ((void (*)(void *, long)) data->setter)(tgt, atol(val));
    else if(data->type == SET_TYPE_DOUBLE)
      ((void (*)(void *, double)) data->setter)(tgt, atof(val));
    else if(data->type == SET_TYPE_STRING)
      ((void (*)(void *, const char *)) data->setter)(tgt, val);
    send_to_char(ch, "Ok.\r\n");
  }
}


// are we trying to set a value from an argument, or from our notepad?
#define SET_SUBCMD_SET        0
#define SET_SUBCMD_SETPAD     1

//
// The entrypoint to the set utility for players in the MUD
//   usage: set [thing] [field] [value]
//
// thing is expected to be a character's name, an object's name, a room's
// vnum, or simply "room" for the current room. Optionally, the function can
// take a value from the player's notepad instead of from the command line (this
// is useful for if you need to write long lengths of formatted text).
COMMAND(cmd_set) {
  char name [SMALL_BUFFER];
  char field[SMALL_BUFFER];
  bool  file = FALSE;

  // are we trying to modify a character on disk?
  if(!strncasecmp(arg, "file ", 5)) {
    file = TRUE;
    arg += 5;
  }

  const char *val = two_args(arg, name, field);

  // check to see if we're trying to set from our notepad. Also, make sure
  // we have a socket and CAN access our notepad.
  if(subcmd == SET_SUBCMD_SETPAD) {
    if(charGetSocket(ch) && bufferLength(socketGetNotepad(charGetSocket(ch))))
      val = bufferString(socketGetNotepad(charGetSocket(ch)));
    else {
      send_to_char(ch, "Your notepad currently has no contents.\r\n");
      return;
    }
  }

  if(!*val || !*name || !*field)
    send_to_char(ch, "Set which value on what?\r\n");

  // we're trying to set something on someone's pfile
  else if(file == TRUE) {
    // make sure the player isn't online, currently
    CHAR_DATA *tgt = generic_find(ch, name, FIND_TYPE_CHAR, FIND_SCOPE_ALL, 
				  FALSE, NULL);
    if(tgt != NULL)
      send_to_char(ch, "%s is currently logged on. No need to touch the pfile.\r\n", charGetName(tgt));
    else {
      tgt = load_player(name);
      if(tgt == NULL)
	send_to_char(ch, "No pfile for %s exists!\r\n", name);
      else if(!charHasMoreUserGroups(ch, tgt))
	send_to_char(ch, "Sorry, %s has just as many priviledges as you.\r\n", 
		     HESHE(tgt));
      else {
	try_set(ch, tgt, char_set_table, field, val);
	save_player(tgt);
	deleteChar(tgt);
      }
    }
  }

  // are we trying to set a field on a room?
  else if(!strcasecmp("room", name)) {
    if(!canEditZone(worldZoneBounding(gameworld, roomGetVnum(charGetRoom(ch))),
		    ch))
      send_to_char(ch, "You are not authorized to edit this zone.\r\n");
    else
      try_set(ch, charGetRoom(ch), room_set_table, field, val);
  }

  // are we trying to set a field on a room, by vnum>
  else if(isdigit(*name) && worldGetRoom(gameworld, atoi(name))) {
    if(!canEditZone(worldZoneBounding(gameworld, atoi(name)), ch))
      send_to_char(ch, "You are not authorized to edit this zone.\r\n");
    else
      try_set(ch, worldGetRoom(gameworld, atoi(name)),room_set_table,field,val);
  }

  // are we setting a field on an object or character in game?
  else {
    int found = FOUND_NONE;
    void *tgt = NULL;
    tgt = generic_find(ch, name, FIND_TYPE_CHAR | FIND_TYPE_OBJ,
		       FIND_SCOPE_ALL | FIND_SCOPE_VISIBLE, FALSE, &found);

    if(found == FOUND_CHAR) {
      if(ch != tgt && !charHasMoreUserGroups(ch, tgt))
	send_to_char(ch, "Sorry, %s has just as many priviledges as you.\r\n", 
		     HESHE(tgt));
      else
	try_set(ch, tgt, char_set_table, field, val);
    }
    else if(found == FOUND_OBJ)
      try_set(ch, tgt, obj_set_table, field, val);
    else
      send_to_char(ch, "What was the target you were trying to modify?\r\n");
  }
}



//*****************************************************************************
//
// implementation of set_val.h
//
//*****************************************************************************
void init_set() {
  // build all of our tables
  char_set_table = newHashtable();
  obj_set_table  = newHashtable();
  room_set_table = newHashtable();

  // add in the default sets for the core of the MUD
  /************************************************************/
  /*                         WARNING                          */
  /*                                                          */
  /* If you are wanting to add new "set" fields to the set    */
  /* module, you are best off adding them with whatever module*/
  /* the new data is included in. If the term "module" is not */
  /* making any sense to you, READ THE DOCUMENTATION!         */
  /************************************************************/

  // PLAYER SETS
  add_set("desc",     SET_CHAR, SET_TYPE_STRING, charSetDesc,         NULL);
  add_set("rdesc",    SET_CHAR, SET_TYPE_STRING, charSetRdesc,        NULL);
  add_set("name",     SET_CHAR, SET_TYPE_STRING, charSetName,         NULL);
  add_set("mrdesc",   SET_CHAR, SET_TYPE_STRING, charSetMultiRdesc,   NULL);
  add_set("mname",    SET_CHAR, SET_TYPE_STRING, charSetMultiName,    NULL);
  add_set("keywords", SET_CHAR, SET_TYPE_STRING, charSetKeywords,     NULL);
  add_set("dialog",   SET_CHAR, SET_TYPE_INT,    charSetDialog,       NULL);
  add_set("race",     SET_CHAR, SET_TYPE_STRING, charSetRace,       isRace);
  add_set("groups",   SET_CHAR, SET_TYPE_STRING, charSetUserGroups,   NULL);

  // ROOM SETS
  add_set("name",     SET_ROOM, SET_TYPE_STRING, roomSetName,         NULL);
  add_set("desc",     SET_ROOM, SET_TYPE_STRING, roomSetDesc,         NULL);

  // OBJECT SETS
  add_set("name",     SET_OBJECT,SET_TYPE_STRING,objSetName,          NULL);
  add_set("desc",     SET_OBJECT,SET_TYPE_STRING,objSetDesc,          NULL);
  add_set("rdesc",    SET_OBJECT,SET_TYPE_STRING,objSetRdesc,         NULL);
  add_set("name",     SET_OBJECT,SET_TYPE_STRING,objSetName,          NULL);
  add_set("mrdesc",   SET_OBJECT,SET_TYPE_STRING,objSetMultiRdesc,    NULL);
  add_set("mname",    SET_OBJECT,SET_TYPE_STRING,objSetMultiName,     NULL);
  add_set("keywords", SET_OBJECT,SET_TYPE_STRING,objSetKeywords,      NULL);

  // now, add the admin commands for working with set
  add_cmd("set", NULL, cmd_set, SET_SUBCMD_SET, POS_UNCONCIOUS, POS_FLYING,
	  "admin", FALSE, FALSE);
  add_cmd("setpad", NULL, cmd_set, SET_SUBCMD_SETPAD, POS_UNCONCIOUS,POS_FLYING,
	  "admin", FALSE, FALSE);
}


void add_set(const char *name, int set_for, int type, void *setter, void *checker) {
  HASHTABLE *table = NULL;
  // first, find our table
  if     (set_for == SET_CHAR)   table = char_set_table;
  else if(set_for == SET_OBJECT) table = obj_set_table;
  else if(set_for == SET_ROOM)   table = room_set_table;

  // if there was no table, we can't do anything
  if(table == NULL) return;

  // If old data exists, remove it
  SET_VAL_DATA *old_set = hashRemove(table, name);
  if(old_set) deleteSetValData(old_set);

  // now, build our new data and add it to the table
  SET_VAL_DATA *new_set = newSetValData(type, setter, checker);
  hashPut(table, name, new_set);
}
