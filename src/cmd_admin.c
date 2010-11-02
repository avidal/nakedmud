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
#include "room.h"
#include "handler.h"
#include "utils.h"
#include "socket.h"
#include "save.h"
#include "storage.h"



//*****************************************************************************
// commands in cmd_admin.c
//*****************************************************************************

//
// Locks the game for anyone not a member of one of the user groups we specify.
COMMAND(cmd_lockdown) {
  // no argument - check the current lockdown status
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
// changes the number of pulses the mud experiences each second
COMMAND(cmd_pulserate) {
  if(!*arg)
    send_to_char(ch,"The mud currently has %d pulses per second.\r\n", 
		 PULSES_PER_SECOND);
  else {
    int pulserate = 0;
    if(!parse_args(ch, FALSE, cmd, arg, "int",  &pulserate) ||
       (1000 % pulserate != 0))
      send_to_char(ch, "The number of pulses per second must divide 1000.\r\n");
    else {
      mudsettingSetInt("pulses_per_second", pulserate);
      send_to_char(ch, "The mud's new pulse rate is %d pulses per second.\r\n",
		   PULSES_PER_SECOND);
    }
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
  int    repeats = 0;

  if(!parse_args(ch, TRUE, cmd, arg, "int string", &repeats, &arg))
    return;

  // make sure the integer is a valid number
  if(repeats < 1)
    send_to_char(ch, "Commands can only be repeated a positive number of time.\r\n");
  else {
    int i;
    // now, do the repeating
    for(i = 0; i < repeats; i++)
      do_cmd(ch, arg, TRUE);
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
    do_cmd(vict, cmd, TRUE);
  }
}


//
// force someone to execute a command
COMMAND(cmd_force) {
  void    *found = NULL;
  bool  multiple = FALSE; 

  if(!parse_args(ch, TRUE, cmd, arg, "ch.world.noself.multiple string",
		 &found, &multiple, &arg))
    return;

  // did we find a single character, or a list of characters?
  if(multiple == FALSE)
    try_force(ch, found, arg);
  else {
    LIST_ITERATOR *ch_i = newListIterator(found);
    CHAR_DATA   *one_ch = NULL;
    ITERATE_LIST(one_ch, ch_i) {
      try_force(ch, one_ch, arg);
    } deleteListIterator(ch_i);
    deleteList(found);
  }
}


//
// Perform a command at another room or person
COMMAND(cmd_at) {
  ROOM_DATA *room = NULL;
  void     *found = NULL;
  int  found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, "{ room ch.world.noself } string",
		 &found, &found_type, &arg))
    return;

  // figure out what room we're doing the command at
  if(found_type == PARSE_ROOM)
    room = found;
  else // found_type == PARSE_CHAR
    room = charGetRoom(found);

  // transfer us over to the new room, do the command, then transfer back
  ROOM_DATA *old_room = charGetRoom(ch);
  char_from_room(ch);
  char_to_room(ch, room);
  do_cmd(ch, arg, TRUE);
  char_from_room(ch);
  char_to_room(ch, old_room);
}


//
// Go to a specific room, object, or character in the game. Rooms are referenced
// by vnum. Everything else is referenced by name.
//   usage: goto <thing>
//
//   examples:
//     goto 100             go to room number 100
//     goto jim             go to an object/person named jim
COMMAND(cmd_goto) {
  ROOM_DATA *room = NULL;
  void     *found = NULL;
  int  found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, "{ room ch.world.noself }", 
		 &found, &found_type))
    return;

  // what did we find?
  if(found_type == PARSE_ROOM)
    room = found;
  else // found_type == PARSE_CHAR
    room = charGetRoom(found);

  message(ch, NULL, NULL, NULL, TRUE, TO_ROOM,
	  "$n disappears in a puff of smoke.");
  char_from_room(ch);
  char_to_room(ch, room);
  look_at_room(ch, room);
  message(ch, NULL, NULL, NULL, TRUE, TO_ROOM,
	  "$n arrives in a puff of smoke.");
}


//
// ch transfers tgt to dest
void do_transfer(CHAR_DATA *ch, CHAR_DATA *tgt, ROOM_DATA *dest) {
  if(dest == charGetRoom(tgt))
    send_to_char(ch, "%s is already %s.\r\n", charGetName(tgt),
		 (charGetRoom(ch) == dest ? "here" : "there"));
  else {
    send_to_char(tgt, "%s has transferred you to %s!\r\n",
		 see_char_as(tgt, ch), roomGetName(dest));
    message(tgt, NULL, NULL, NULL, TRUE, TO_ROOM,
	    "$n disappears in a puff of smoke.");
    char_from_room(tgt);
    char_to_room(tgt, dest);
    look_at_room(tgt, dest);
    message(tgt, NULL, NULL, NULL, TRUE, TO_ROOM,
	    "$n arrives in a puff of smoke.");
  }
}


//
// The opposite of goto. Instead of moving to a specified location, it
// takes the target to the user.
//   usage: transfer <player> [[to] room]
COMMAND(cmd_transfer) {
  void     *found = NULL;
  bool   multiple = FALSE;
  ROOM_DATA *dest = NULL;

  // if our arguments don't parse properly, 
  // parse_args will tell the person what is wrong
  if(parse_args(ch, TRUE, cmd, arg,
		"ch.world.multiple.noself | [to] room",
		&found, &multiple, &dest)) {
    // if we didn't supply a destination, use our current room
    if(dest == NULL)
      dest = charGetRoom(ch);

    // if we have multiple people, we'll have to transfer them one by one
    if(multiple == FALSE)
      do_transfer(ch, found, dest);
    else {
      LIST_ITERATOR *tgt_i = newListIterator(found);
      CHAR_DATA       *tgt = NULL;
      ITERATE_LIST(tgt, tgt_i) {
	do_transfer(ch, found, dest);
      } deleteListIterator(tgt_i);

      // we also have to delete the list that we were given
      deleteList(found);
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
  LIST_ITERATOR *ch_i = newListIterator(mobile_list);
  CHAR_DATA   *one_ch = NULL;
  bool          found = FALSE;

  ITERATE_LIST(one_ch, ch_i) {
    if (!(charIsNPC(one_ch) || charGetSocket(one_ch))) {
      send_to_char(ch, "%s is linkdead.\r\n", charGetName(one_ch));
      found = TRUE;
    }
  } deleteListIterator(ch_i);

  if (!found)
    send_to_char(ch, "Noone is currently linkdead.\r\n");
}
