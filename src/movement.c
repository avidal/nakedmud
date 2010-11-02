//*****************************************************************************
//
// movement.c
//
// all of the functions assocciated with moving (chars and objects)
//
//*****************************************************************************

#include "mud.h"
#include "character.h"
#include "world.h"
#include "room.h"
#include "exit.h"
#include "movement.h"
#include "handler.h"
#include "inform.h"
#include "builder.h"
#include "utils.h"
#include "items.h"
#include "object.h"

// optional modules
#ifdef MODULE_SCRIPTS
#include "modules/scripts/script.h"
#endif


bool try_exit(CHAR_DATA *ch, EXIT_DATA *exit, int dir) {
  ROOM_DATA *to   = NULL;

  if(exitIsClosed(exit))
    send_to_char(ch, "You must open %s first.\r\n", exitGetName(exit));
  else if(exitGetTo(exit) == NOWHERE || 
	  (to = worldGetRoom(gameworld, exitGetTo(exit))) == NULL)
    send_to_char(ch, "It doesn't look like %s leads anywhere!", 
		 exitGetName(exit));
  else {
    if(*exitGetSpecLeave(exit))
      message(ch, NULL, NULL, NULL, FALSE, TO_ROOM | TO_NOTCHAR,
	      exitGetSpecLeave(exit));
    else if(dir != DIR_NONE)
      message(ch, NULL, NULL, NULL, FALSE, TO_ROOM | TO_NOTCHAR,
	      "$n leaves %s.", dirGetName(dir));
    else
      message(ch, NULL, NULL, NULL, FALSE, TO_ROOM | TO_NOTCHAR,
	      "$n leaves.");

    char_from_room(ch);
    char_to_room(ch, to);
    look_at_room(ch, to);

    if(*exitGetSpecEnter(exit))
      message(ch, NULL, NULL, NULL, FALSE, TO_ROOM | TO_NOTCHAR,
	      exitGetSpecEnter(exit));
    else if(dir != DIR_NONE)
      message(ch, NULL, NULL, NULL, FALSE, TO_ROOM | TO_NOTCHAR,
	      "$n arrives from the %s.", dirGetName(dirGetOpposite(dir)));
    else
      message(ch, NULL, NULL, NULL, FALSE, TO_ROOM | TO_NOTCHAR,
	      "$n has arrived.");

    return TRUE;
  }
  return FALSE;
}


bool try_move(CHAR_DATA *ch, int dir, const char *specdir) {
  EXIT_DATA *exit = NULL;
  if(dir != DIR_NONE)      
    exit = roomGetExit(charGetRoom(ch), dir);
  else if(specdir != NULL) 
    exit = roomGetExitSpecial(charGetRoom(ch), specdir);

  if(exit == NULL || !can_see_exit(ch, exit)) {
    // see if we can buildwalk a new room
    if(charIsBitSet(ch, BITFIELD_PRFS, PRF_BUILDWALK))
      return try_buildwalk(ch, dir);
    else
      send_to_char(ch, "Alas, there is no exit in that direction.\r\n");
    return FALSE;
  }

  else {
#ifdef MODULE_SCRIPTS
    ROOM_DATA *old_room = charGetRoom(ch);
    bool success = try_exit(ch, exit, dir);
    if(success) {
      try_exit_script(ch, old_room, exit, 
		      (dir != DIR_NONE ? dirGetName(dir) : specdir));
      try_enterance_script(ch, charGetRoom(ch), exit, 
			   (dir != DIR_NONE ? dirGetName(dir) : specdir));
    }
    return success;
#else
    return try_exit(ch, exit, dir);
#endif
  }
}


//
// cmd_move is the basic entry into all of the movement utilities. See
// try_move() in movement.h
//
COMMAND(cmd_move) {
  if(subcmd == DIR_NONE) {
    send_to_char(ch, "In which direction did you wish to move?\r\n");
    return;
  };

  try_move(ch, subcmd, NULL);
};


//
// cmd_enter is used to go through portals
//   usage: enter [object]
//
//   examples:
//     enter portal         enter the thing called "portal" in your room
//
COMMAND(cmd_enter) {
  if(!arg || !*arg)
    send_to_char(ch, "What did you want to enter?\r\n");
  else {
    int found_type = FOUND_NONE;
    void *found = generic_find(ch, arg,
			       FIND_TYPE_OBJ | FIND_TYPE_EXIT,
			       FIND_SCOPE_IMMEDIATE,
			       FALSE, &found_type);

    // we're trying to enter a portal
    if(found && found_type == FOUND_OBJ) {
      if(objGetType(found) != ITEM_PORTAL)
	send_to_char(ch, "You can only enter portals.\r\n");
      else {
	ROOM_DATA *dest = worldGetRoom(gameworld, portalGetDestination(found));
	if(!dest)
	  send_to_char(ch, 
		       "You go to enter the portal, "
		       "but dark forces prevent you!\r\n");
	else {
	  send_to_char(ch, "You step through %s.\r\n", objGetName(found));
	  message(ch, NULL, found, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
		  "$n steps through $o.");
	  char_from_room(ch);
	  char_to_room(ch, dest);
	  look_at_room(ch, dest);
	  message(ch, NULL, found, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
		  "$n arrives after travelling through $o.");
	}
      }
    }

    // we're trying to enter an exit
    else if(found && found_type == FOUND_EXIT)
      try_exit(ch, found, DIR_NONE);
    else
      send_to_char(ch, "What were you trying to enter?\r\n");
  }
}



//*****************************************************************************
//
// Functions and commands for changing position (sleeping, standing, etc)
//
//*****************************************************************************
bool try_change_pos(CHAR_DATA *ch, int pos) {
  if(charGetPos(ch) == pos) {
    send_to_char(ch, "You are already %s.\r\n", posGetName(pos));
    return FALSE;
  }
  else {
    send_to_char(ch, "You %s.\r\n", posGetActionSelf(pos));
    charSetPos(ch, pos);
    message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	    "$n %s.", posGetActionOther(pos));
    return TRUE;
  }
}

bool try_use_furniture(CHAR_DATA *ch, char *arg, int pos) {
  // strip out "at" and "on". People might be 
  // trying "sit at table" or "sleep on bed"
  strip_word(arg, "at");
  strip_word(arg, "on");

  int furniture_type  = FOUND_NONE;
  OBJ_DATA *furniture = generic_find(ch, arg,
				     FIND_TYPE_OBJ, 
				     FIND_SCOPE_ROOM | FIND_SCOPE_VISIBLE,
				     FALSE, &furniture_type);
  
  if(!furniture || furniture_type != FOUND_OBJ)
    send_to_char(ch, "Where did you want to %s?\r\n", posGetActionSelf(pos));
  else if(objGetType(furniture) != ITEM_FURNITURE)
    send_to_char(ch, "But that's not furniture!\r\n");
  // make sure we found something we might be able to sit on
  else if(charGetFurniture(ch) == furniture)
    send_to_char(ch, "You're already %s %s.\r\n",
		 (objGetSubtype(furniture) == FURNITURE_ON ? "on" : "at"),
		 objGetName(furniture));

  else if(furnitureGetCapacity(furniture) <= listSize(objGetUsers(furniture)))
    send_to_char(ch, "There isn't any room left.\r\n");
  
  else {
    // if we're already sitting on something, get up first
    if(charGetFurniture(ch)) {
      send_to_char(ch, "You stand up from %s.\r\n", 
		   objGetName(charGetFurniture(ch)));
      message(ch, NULL, charGetFurniture(ch), NULL,TRUE, TO_ROOM | TO_NOTCHAR,
	      "$n stands up from $o.");
      char_from_furniture(ch);
    }
    
    // now sit down on the new thing
    send_to_char(ch, "You %s %s %s.\r\n",
		 posGetActionSelf(pos),
		 (objGetSubtype(furniture) == FURNITURE_ON ? "on" : "at"),
		 objGetName(furniture));
    message(ch, NULL, furniture, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	    "$n %s %s $o.",
	    posGetActionOther(pos),
	    (objGetSubtype(furniture) == FURNITURE_ON ? "on" : "at"));
    char_to_furniture(ch, furniture);
    charSetPos(ch, pos);
    return TRUE;
  }

  return FALSE;
}

COMMAND(cmd_sit) {
  if(!arg || !*arg)
    try_change_pos(ch, POS_SITTING);
  else
    try_use_furniture(ch, arg, POS_SITTING);
}

COMMAND(cmd_sleep) {
  if(!arg || !*arg)
    try_change_pos(ch, POS_SLEEPING);
  else
    try_use_furniture(ch, arg, POS_SLEEPING);
}

COMMAND(cmd_stand) {
  if(charGetPos(ch) == POS_FLYING) {
    send_to_char(ch, "You stop flying.\r\n");
    message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	    "$n stops flying.");
    charSetPos(ch, POS_STANDING);
  }
  else {
    if(try_change_pos(ch, POS_STANDING) && charGetFurniture(ch))
      char_from_furniture(ch);
  }
}

COMMAND(cmd_wake) {
  send_to_char(ch, "You stop sleeping and sit up.\r\n");
  message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	  "$n stops sleeping and sits up.");
  charSetPos(ch, POS_SITTING);
}
