//*****************************************************************************
//
// cmd_admin.c
//
// commands and procedures available only to admins.
//
//*****************************************************************************

#include "mud.h"
#include "world.h"
#include "inform.h"
#include "character.h"
#include "handler.h"
#include "utils.h"
#include "log.h"
#include "socket.h"
#include "save.h"
#include "event.h"
#include "action.h"
#include "storage.h"



//*****************************************************************************
// commands in cmd_admin.c
//*****************************************************************************

//
// Locks the game for anyone at or below the lockdown level. No argument tells
// us the current lockdown level. Suggested by Rhaelar
COMMAND(cmd_lockdown) {
  int lockdown = 0;

  // we're trying to view the current lockdown level
  if(!*arg) {
    lockdown = mudsettingGetInt("lockdown");
    // no lockdown in place
    if(lockdown == 0)
      send_to_char(ch, "The mud currently is not locked to anyone.\r\n");
    else if(lockdown == 1)
      send_to_char(ch, "The mud is locked down to new players.\r\n");
    else
      send_to_char(ch, "The mud is locked to anyone below level %d.\r\n",
		   lockdown);
  }

  // make sure we've got a level number
  else if(!isdigit(*arg))
    send_to_char(ch, "Which level would you like to set the lockdown to?\r\n");

  // make sure we don't lock ourself out
  else if( (lockdown = atoi(arg)) > charGetLevel(ch))
    send_to_char(ch, "You cannot lock out people below that level!\r\n");

  // do the lockdown
  else {
    mudsettingSetInt("lockdown", lockdown);
    if(lockdown == 0)
      send_to_char(ch, "Lockdown removed.\r\n");
    else if(lockdown == 1)
      send_to_char(ch, "New players are locked down.\r\n");
    else
      send_to_char(ch, "Everyone below level %d is locked down.\r\n", lockdown);

    // kick out anyone that is at or below our lockdown threshold.
    LIST_ITERATOR *ch_i = newListIterator(mobile_list);
    ITERATE_LIST(ch, ch_i) {
      if(!charIsNPC(ch) && charGetLevel(ch) < lockdown) {
	send_to_char(ch, "The mud has just been locked down to anyone at or "
		     "below your level.\r\n");
	save_player(ch);

	// and close the socket if we have one
	if(charGetSocket(ch)) {
	  SOCKET_DATA *sock = charGetSocket(ch);
	  charSetSocket(ch, NULL);
	  socketSetChar(sock, NULL);
	  close_socket(sock, FALSE);
	}

	// do the extraction
	extract_mobile(ch);
      }
    } deleteListIterator(ch_i);
  }
}


//
// BOOM! Shut down the MUD
COMMAND(cmd_shutdown) {
  shut_down = TRUE;
}


//
// Perform a command multiple times
COMMAND(cmd_repeat) {
  if(!arg || !*arg) {
    send_to_char(ch, "What did you want to repeat, and how many times?\r\n");
    return;
  }

  // how many times should we repeat?
  char rep_buf[SMALL_BUFFER];
  arg = one_arg(arg, rep_buf);
  int  repeats = atoi(rep_buf);

  // no command to delay
  if(!*arg)
    send_to_char(ch, "What command did you want to repeat?\r\n");
  else if(repeats < 1)
    send_to_char(ch, "You can only repeat commands a positive amounts of time.\r\n");
  else {
    int i;
    // now, do the repeating
    for(i = 0; i < repeats; i++)
      do_cmd(ch, arg, TRUE, TRUE);
  }
}


//
// Perform a command at another room or person
COMMAND(cmd_at) {
  if(!arg || !*arg) {
    send_to_char(ch, "Do what where?\r\n");
    return;
  }

  // how many times should we repeat?
  char where[SMALL_BUFFER];
  arg = one_arg(arg, where);

  // no command to delay
  if(!*arg)
    send_to_char(ch, "What were you trying to do, and where?\r\n");
  else {
    // first, are we trying to do this at a room vnum?
    ROOM_DATA *room = NULL;

    // are we looking for a vnum?
    if(isdigit(*where))
      room = worldGetRoom(gameworld, atoi(where));

    // no room? Maybe its the name of someone
    if(room == NULL) {
      CHAR_DATA *tgt = generic_find(ch, where, FIND_TYPE_CHAR,
				    FIND_SCOPE_ALL | FIND_SCOPE_VISIBLE,
				    FALSE, NULL);
      if(tgt != NULL)
	room = charGetRoom(tgt);
    }

    if(room == NULL)
      send_to_char(ch, "Where were you trying to do that?\r\n");
    else {
      ROOM_DATA *old_room = charGetRoom(ch);
      char_from_room(ch);
      char_to_room(ch, room);
      do_cmd(ch, arg, TRUE, TRUE);
      char_from_room(ch);
      char_to_room(ch, old_room);
    }
  }
}


//
// Go to a specific room, object, or character in the game. Rooms are referenced
// by vnum. Everything else is referenced by name.
//   usage: goto [thing]
//
//   examples:
//     goto 100             go to room number 100
//     goto jim             go to an object/person named jim
COMMAND(cmd_goto) {
  if(!arg || !*arg)
    send_to_char(ch, "Where would you like to go to?\r\n");
  // we're trying to go to a specific room number
  else if(isdigit(*arg)) {
    ROOM_DATA *room = worldGetRoom(gameworld, atoi(arg));

    if(!room)
      send_to_char(ch, "No such room exists.\r\n");
    else if(room == charGetRoom(ch))
      send_to_char(ch, "You're already here, boss.\r\n");
    else {
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	      "$n disappears in a puff of smoke.");
      char_from_room(ch);
      char_to_room(ch, room);
      look_at_room(ch, room);
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	      "$n arrives in a puff of smoke.");
    }
  }

  // find the character we're trying to go to
  else {
    void *tgt = generic_find(ch, arg, 
			     FIND_TYPE_CHAR,
			     FIND_SCOPE_WORLD | FIND_SCOPE_VISIBLE,
			     FALSE, NULL);

    if(ch == tgt)
      send_to_char(ch, "You're already here, boss.\r\n");
    else if(tgt != NULL) {
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	      "$n disappears in a puff of smoke.");
      char_from_room(ch);
      char_to_room(ch, charGetRoom(tgt));
      look_at_room(ch, charGetRoom(ch));
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	      "$n arrives in a puff of smoke.");
    }
    else
      send_to_char(ch, "Who were you trying to go to?\r\n");
  }
}


//
// The opposite of goto. Instead of moving to a specified location, it
// takes the target to the user.
//   usage: transfer [player]
COMMAND(cmd_transfer) {
  if(!arg || !*arg)
    send_to_char(ch, "Who would you like to transfer?\r\n");
  else {
    void *tgt = generic_find(ch, arg, 
			     FIND_TYPE_CHAR,
			     FIND_SCOPE_WORLD | FIND_SCOPE_VISIBLE,
			     FALSE, NULL);

    if(ch == tgt)
      send_to_char(ch, "You're already here, boss.\r\n");
    else if(charGetRoom(ch) == charGetRoom(tgt))
      send_to_char(ch, "They're already here.\r\n");
    else if(tgt != NULL) {
      message(ch, tgt, NULL, NULL, TRUE, TO_VICT,
	      "$n has transferred you!");
      message(tgt, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	      "$n disappears in a puff of smoke.");
      char_from_room(tgt);
      char_to_room(tgt, charGetRoom(ch));
      look_at_room(tgt, charGetRoom(tgt));
      message(tgt, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	      "$n arrives in a puff of smoke.");
    }
    else
      send_to_char(ch, "Who were you trying to transfer?\r\n");
  }
}


//
// Perform a copyover
COMMAND(cmd_copyover) { 
  do_copyover(ch);
}


//
// show a list of all the PCs who are linkdead
COMMAND(cmd_linkdead) {
  CHAR_DATA *xMob;
  char buf[MAX_BUFFER];
  bool found = FALSE;
  LIST_ITERATOR *mob_i = newListIterator(mobile_list);

  ITERATE_LIST(xMob, mob_i) {
    if (!(charIsNPC(xMob) || charGetSocket(xMob))) {
      sprintf(buf, "%s is linkdead.\n\r", charGetName(xMob));
      text_to_char(ch, buf);
      found = TRUE;
    }
  }
  deleteListIterator(mob_i);

  if (!found)
    text_to_char(ch, "Noone is currently linkdead.\n\r");
}


//
// List all of the non-player commands the character has access to
COMMAND(cmd_wizhelp) {
  show_commands(ch, LEVEL_BUILDER, charGetLevel(ch));
}


//
// Turn on/off immortal invisibility
COMMAND(cmd_invis) {
  int level = charGetLevel(ch); // default to our level
  if(arg && *arg && isdigit(*arg))
    level = atoi(arg);

  // make sure we're not trying to go invisible at a level higher than us
  if(level > charGetLevel(ch)) {
    send_to_char(ch, "You cannot go invisibile to a level higher than yours.\r\n");
    return;
  }

  // see if we're trying to go visible
  if(level == 0 && charGetImmInvis(ch) > 0) {
    send_to_char(ch, "Invisibility turned off.\r\n");
    charSetImmInvis(ch, 0);
    message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	    "$n slowly fades into existence.");    
  }
  // or invisible
  else if(level > 0) {
    message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	    "$n slowly fades out of existence.");
    charSetImmInvis(ch, level);
    send_to_char(ch, "You are now invisible to anyone below level %d.\r\n",
		 level);
  }
  // we tried to go visible, but we already are visible
  else
    send_to_char(ch, "But you're already visible!\r\n");
}


//
// turn off immortal invisibility
COMMAND(cmd_visible) {
  if(charGetImmInvis(ch) == 0)
    send_to_char(ch, "But you're already visible!\r\n");
  else {
    charSetImmInvis(ch, 0);
    send_to_char(ch, "You fade into existence.\r\n");
    message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	    "$n slowly fades into existence.");
  }
}
