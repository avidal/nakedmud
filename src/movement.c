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
#include "zone.h"
#include "room.h"
#include "exit.h"
#include "movement.h"
#include "handler.h"
#include "inform.h"
#include "utils.h"
#include "object.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/script.h"
#include "items/items.h"
#include "items/furniture.h"



ROOM_DATA *try_exit(CHAR_DATA *ch, EXIT_DATA *exit, int dir) {
  ROOM_DATA *to   = NULL;

  if(exitIsClosed(exit))
    send_to_char(ch, "You must open %s first.\r\n", exitGetName(exit));
  else if(exitGetTo(exit) == NOWHERE || 
	  (to = worldGetRoom(gameworld, exitGetTo(exit))) == NULL)
    send_to_char(ch, "It doesn't look like %s leads anywhere!", 
		 exitGetName(exit));
  else {
    if(*exitGetSpecLeave(exit))
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM, exitGetSpecLeave(exit));
    else if(dir != DIR_NONE)
      send_around_char(ch, TRUE, "%s leaves %s.\r\n",
		       charGetName(ch), dirGetName(dir));
    else
      send_around_char(ch, TRUE, "%s leaves.\r\n", charGetName(ch));

    char_from_room(ch);
    char_to_room(ch, to);

    if(*exitGetSpecEnter(exit))
      message(ch, NULL, NULL, NULL, FALSE, TO_ROOM, exitGetSpecEnter(exit));
    else if(dir != DIR_NONE)
      send_around_char(ch, TRUE, "%s arrives from the %s.\r\n",
		       charGetName(ch), dirGetName(dirGetOpposite(dir)));
    else
      send_around_char(ch, TRUE, "%s has arrived.\r\n", charGetName(ch));

    return to;
  }
  return NULL;
}


bool try_buildwalk(CHAR_DATA *ch, int dir) {
  ZONE_DATA *zone = worldZoneBounding(gameworld, roomGetVnum(charGetRoom(ch)));
  
  if(!canEditZone(zone, ch))
    send_to_char(ch, "You are not authorized to edit this zone.\r\n");
  else if(roomGetExit(charGetRoom(ch), dir))
    send_to_char(ch, "You try to buildwalk %s, but a room already exists in that direction!\r\n", dirGetName(dir));
  else if(!zone) {
    send_to_char(ch, "The room you are in is not attached to a zone!\r\n");
    log_string("ERROR: %s tried to buildwalk %s, but room %d was not in a zone!", charGetName(ch), dirGetName(dir), roomGetVnum(charGetRoom(ch)));
  }
  else {
    int vnum = getFreeRoomVnum(zone);
    if(vnum == NOWHERE)
      send_to_char(ch, 
		   "Zone #%d has no free rooms left. "
		   "Buildwalk could not be performed.\r\n", zoneGetVnum(zone));
    else {
      char desc[MAX_BUFFER];
      ROOM_DATA *new_room = newRoom();
      roomSetVnum(new_room, vnum);
      roomSetTerrain(new_room, roomGetTerrain(charGetRoom(ch)));

      roomSetName(new_room, "A New Buildwalk Room");
      sprintf(desc, "This room was created by %s.\r\n", charGetName(ch));
      roomSetDesc(new_room, desc);

      zoneAddRoom(zone, new_room);
      roomDigExit(charGetRoom(ch), dir, vnum);
      roomDigExit(new_room, dirGetOpposite(dir), 
		  roomGetVnum(charGetRoom(ch)));
      try_move(ch, dir, NULL);
      return TRUE;

      // save the changes... this will get costly as our world gets bigger.
      // But that should be alright once we make zone saving a bit smarter
      worldSave(gameworld, WORLD_PATH);
    }
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
    if(bitIsOneSet(charGetPrfs(ch), "buildwalk"))
      return try_buildwalk(ch, dir);
    else
      send_to_char(ch, "{gAlas, there is no exit in that direction.\r\n");
    return FALSE;
  }

  else {
    ROOM_DATA *old_room = charGetRoom(ch);
    ROOM_DATA *new_room = try_exit(ch, exit, dir);
    if(new_room) {
      // move ourself back for a second so we can do the exit script...
      char_to_room(ch, old_room);
      try_exit_script(ch, old_room, 
		      (dir != DIR_NONE ? dirGetName(dir) : specdir));
      char_to_room(ch, new_room);

      try_enterance_script(ch, charGetRoom(ch),
			   (dir != DIR_NONE ? dirGetName(dir) : specdir));
      look_at_room(ch, new_room);
    }
    return (new_room != NULL);
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
    send_around_char(ch, TRUE, "%s %s.\r\n", 
		     charGetName(ch), posGetActionOther(pos));
    charSetPos(ch, pos);
    return TRUE;
  }
}

bool try_use_furniture(CHAR_DATA *ch, char *arg, int pos) {
  // strip out "at" and "on". People might be 
  // trying "sit at table" or "sleep on bed"
  strip_word(arg, "at");
  strip_word(arg, "on");

  OBJ_DATA *furniture = generic_find(ch, arg,
				     FIND_TYPE_OBJ, 
				     FIND_SCOPE_ROOM | FIND_SCOPE_VISIBLE,
				     FALSE, NULL);
  
  if(furniture == NULL)
    send_to_char(ch, "Where did you want to %s?\r\n", posGetActionSelf(pos));
  else if(!objIsType(furniture, "furniture"))
    send_to_char(ch, "But that's not furniture!\r\n");
  // make sure we found something we might be able to sit on
  else if(charGetFurniture(ch) == furniture)
    send_to_char(ch, "You're already %s %s.\r\n",
		 (furnitureGetType(furniture) == FURNITURE_ON ? "on" : "at"),
		 objGetName(furniture));

  else if(furnitureGetCapacity(furniture) <= listSize(objGetUsers(furniture)))
    send_to_char(ch, "There isn't any room left.\r\n");
  
  else {
    // if we're already sitting on something, get up first
    if(charGetFurniture(ch)) {
      send_to_char(ch, "You stand up from %s.\r\n", 
		   objGetName(charGetFurniture(ch)));
      message(ch, NULL, charGetFurniture(ch), NULL,TRUE, TO_ROOM,
	      "$n stands up from $o.");
      char_from_furniture(ch);
    }

    // send out messages
    char other_buf[SMALL_BUFFER];
    sprintf(other_buf, "$n %s %s $o.",	
	    posGetActionOther(pos),
	    (furnitureGetType(furniture) == FURNITURE_ON ? "on" : "at"));
    message(ch, NULL, furniture, NULL, TRUE, TO_ROOM, other_buf);

    send_to_char(ch, "You %s %s %s.\r\n",
		 posGetActionSelf(pos),
		 (furnitureGetType(furniture) == FURNITURE_ON ? "on" : "at"),
		 objGetName(furniture));

    // now sit down on the new thing
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
    send_around_char(ch, TRUE, "%s stops flying.\r\n", charGetName(ch));
    charSetPos(ch, POS_STANDING);
  }
  else {
    if(try_change_pos(ch, POS_STANDING) && charGetFurniture(ch))
      char_from_furniture(ch);
  }
}

COMMAND(cmd_wake) {
  send_to_char(ch, "You stop sleeping and sit up.\r\n");
  send_around_char(ch, TRUE, "%s stops sleeping and sits up.\r\n",
		   charGetName(ch));
  charSetPos(ch, POS_SITTING);
}
