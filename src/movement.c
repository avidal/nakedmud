//*****************************************************************************
//
// movement.c
//
// all of the functions assocciated with moving (chars and objects)
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "character.h"
#include "world.h"
#include "zone.h"
#include "room.h"
#include "exit.h"
#include "handler.h"
#include "inform.h"
#include "object.h"
#include "movement.h"
#include "hooks.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "items/items.h"
#include "items/furniture.h"



//*****************************************************************************
// implementation of movement.h
//*****************************************************************************
EXIT_DATA *try_move_mssg(CHAR_DATA *ch, const char *dir) {
  ROOM_DATA *old_room = charGetRoom(ch);
  EXIT_DATA     *exit = try_move(ch, dir);
  
  // did we successfully move?
  if(exit != NULL) {
    ROOM_DATA *new_room = charGetRoom(ch);
    int          dirnum = dirGetNum(dir);
    // are we using an abbreviated direction name?
    if(dirnum == DIR_NONE && !roomGetExit(old_room, dir))
      dirnum = dirGetAbbrevNum(dir);

    // now, we have to temporarily go back to the old room so we can do
    // leave messages. Then we come back to the new room and do enter messages
    char_from_room(ch);
    char_to_room(ch, old_room);
    if(*exitGetSpecLeave(exit))
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM, exitGetSpecLeave(exit));
    else if(dirnum == DIR_NONE)
      send_around_char(ch, TRUE, "%s leaves.\r\n", charGetName(ch));
    else
      send_around_char(ch, TRUE, "%s leaves %s.\r\n", charGetName(ch), 
		       dirGetName(dirnum));
    char_from_room(ch);
    char_to_room(ch, new_room);
    
    // do we have a special enter message? If so, use them
    if(*exitGetSpecEnter(exit))
      message(ch, NULL, NULL, NULL, FALSE, TO_ROOM, exitGetSpecEnter(exit));
    else if(dirnum == DIR_NONE)
      send_around_char(ch, TRUE, "%s has arrived.\r\n", charGetName(ch));
    else
      send_around_char(ch, TRUE, "%s arrives from the %s.\r\n",
		       charGetName(ch), dirGetName(dirGetOpposite(dirnum)));
  }
  return exit;
}

EXIT_DATA *try_move(CHAR_DATA *ch, const char *dir) {
  EXIT_DATA *exit = roomGetExit(charGetRoom(ch), dir);
  ROOM_DATA   *to = NULL;

  // are we using an abbreviated direction name?
  if(exit == NULL && dirGetAbbrevNum(dir) != DIR_NONE) {
    dir  = dirGetName(dirGetAbbrevNum(dir));
    exit = roomGetExit(charGetRoom(ch), dir);
  }

  // did we find an exit?
  if(exit == NULL || !can_see_exit(ch, exit))
    send_to_char(ch, "{gAlas, there is no exit in that direction.\r\n");
  else if(exitIsClosed(exit))
    send_to_char(ch, "You will have to open %s first.\r\n",
		 (*exitGetName(exit) ? exitGetName(exit) : "it"));
  else if((to = worldGetRoom(gameworld, exitGetTo(exit))) == NULL)
    send_to_char(ch, "It doesn't look like %s leads anywhere!", 
		 (*exitGetName(exit) ? exitGetName(exit) : "it"));
  else {
    ROOM_DATA  *old_room = charGetRoom(ch);

    // try all of our exit hooks
    hookRun("exit", ch, old_room, exit);

    char_from_room(ch);
    char_to_room(ch, to);
    look_at_room(ch, charGetRoom(ch));

    // now try all of our entrance hooks
    hookRun("enter", ch, to, NULL);

    return exit;
  }

  // our exit failed
  return NULL;
}

//
// cmd_move is the basic entry into all of the movement utilities. See
// try_move() in movement.h
COMMAND(cmd_move) {
  try_move_mssg(ch, cmd);
}



//*****************************************************************************
// Functions and commands for changing position (sleeping, standing, etc)
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
