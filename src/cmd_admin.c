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
// Locks the game for anyone not a member of one of the user groups we specify.
COMMAND(cmd_lockdown) {
  // check the current lockdown status
  if(!*arg) {
    if(!*mudsettingGetString("lockdown"))
      send_to_char(ch, "Lockdown is currently turned off.\r\n");
    else {
      send_to_char(ch, "Current lockdown is to members not of: %s\r\n",
		   mudsettingGetString("lockdown"));
      send_to_char(ch, "To turn off lockdown, use {clockdown off{n\r\n");
    }
  }

  // turn lockdown off
  else if(!strcasecmp(arg, "off")) {
    send_to_char(ch, "Lockdown disabled.\r\n");
    mudsettingSetString("lockdown", "");
  }

  // make sure we're not locking ourself out
  else if(!bitIsSet(charGetUserGroups(ch), arg))
    send_to_char(ch, "You cannot lock yourself out!\r\n");

  // lock out anyone not in the groups we specify
  else {
    send_to_char(ch, "MUD locked down to everyone not in groups: %s\r\n", arg);
    mudsettingSetString("lockdown", arg);

    // kick out everyone who we've just locked out
    LIST_ITERATOR *ch_i = newListIterator(mobile_list);
    ITERATE_LIST(ch, ch_i) {
      if(!charIsNPC(ch) && !bitIsSet(charGetUserGroups(ch), arg)) {
	send_to_char(ch, "The mud has just been locked down to you.\r\n");
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
// tries to force the person to do something
void try_force(CHAR_DATA *ch, CHAR_DATA *vict, char *cmd) {
  if(ch == vict)
    send_to_char(ch, "Why don't you just try doing it?\r\n");
  else if(!charHasMoreUserGroups(ch, vict))
    send_to_char(ch, "But %s has just as many priviledges as you!\r\n",
		 charGetName(vict));
  else {
    send_to_char(ch,   "You force %s to '%s'\r\n", charGetName(vict), cmd);
    send_to_char(vict, "%s forces you to '%s'\r\n",
		 see_char_as(vict, ch), cmd);
    do_cmd(vict, cmd, TRUE, TRUE);
  }
}


//
// force someone to execute a command
COMMAND(cmd_force) {
  char name[SMALL_BUFFER];
  arg = one_arg(arg, name);
  
  if(!*name || !*arg)
    send_to_char(ch, "Force who to do what?\r\n");
  else {
    int type    = FOUND_NONE;
    void *found = generic_find(ch, name, FIND_TYPE_CHAR, FIND_SCOPE_ALL,
			       TRUE, &type);

    // did we find a list of characters?
    if(found == NULL)
      send_to_char(ch, "No targets found!\r\n");
    else if(type == FOUND_LIST) {
      if(listSize(found) == 0)
	send_to_char(ch, "No targets found.\r\n");
      else {
	LIST_ITERATOR *ch_i = newListIterator(found);
	CHAR_DATA   *one_ch = NULL;
	ITERATE_LIST(one_ch, ch_i) {
	  if(ch != one_ch)
	    try_force(ch, one_ch, arg);
	} deleteListIterator(ch_i);
	deleteList(found);
      }
    }
    // a single character...
    else
      try_force(ch, found, arg);
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
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM,
	      "$n disappears in a puff of smoke.");
      char_from_room(ch);
      char_to_room(ch, room);
      look_at_room(ch, room);
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM,
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
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM,
	      "$n disappears in a puff of smoke.");
      char_from_room(ch);
      char_to_room(ch, charGetRoom(tgt));
      look_at_room(ch, charGetRoom(ch));
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM,
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

    if(tgt == NULL)
      send_to_char(ch, "Who are you looking for?\r\n");
    else if(ch == tgt)
      send_to_char(ch, "You're already here, boss.\r\n");
    else if(charGetRoom(ch) == charGetRoom(tgt))
      send_to_char(ch, "They're already here.\r\n");
    else {
      message(ch, tgt, NULL, NULL, TRUE, TO_VICT,
	      "$n has transferred you!");
      message(tgt, NULL, NULL, NULL, TRUE, TO_ROOM,
	      "$n disappears in a puff of smoke.");
      char_from_room(tgt);
      char_to_room(tgt, charGetRoom(ch));
      look_at_room(tgt, charGetRoom(tgt));
      message(tgt, NULL, NULL, NULL, TRUE, TO_ROOM,
	      "$n arrives in a puff of smoke.");
    }
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
