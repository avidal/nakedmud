//*****************************************************************************
//
// inform.c
//
// These are all of the functions that are used for listing off processed
// information about things (e.g. room descriptions + contents, shopping
// lists, etc...)
//
//*****************************************************************************

#include "mud.h"
#include "character.h"
#include "object.h"
#include "world.h"
#include "room.h"
#include "exit.h"
#include "extra_descs.h"
#include "utils.h"
#include "body.h"
#include "races.h"
#include "handler.h"
#include "socket.h"
#include "inform.h"
#include "log.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "items/items.h"
#include "items/portal.h"
#include "items/container.h"
#include "items/furniture.h"



//*****************************************************************************
// optional module headers
//*****************************************************************************
#ifdef MODULE_TIME
#include "time/mudtime.h"
#endif



//*****************************************************************************
//
// local functions
//
//*****************************************************************************

//
// Show a character who is all sitting at one piece of furniture
//
void list_one_furniture(CHAR_DATA *ch, OBJ_DATA *furniture) {
  LIST *can_see = find_all_chars(ch,objGetUsers(furniture), "", NOBODY, TRUE);
  listRemove(can_see, ch);

  char *chars = print_list(can_see, charGetName, charGetMultiName);
  if(*chars) send_to_char(ch, "{g%s %s %s %s%s.\r\n",
			  chars, (listSize(can_see) == 1 ? "is" : "are"),
			  (furnitureGetType(furniture)==FURNITURE_AT?"at":"on"),
			  objGetName(furniture),
			  (charGetFurniture(ch) == furniture ?" with you": ""));
  // everyone was invisible to us... we should still show the furniture though
  else
    send_to_char(ch, "{g%s\r\n", objGetRdesc(furniture));
  deleteList(can_see);
  free(chars);
}


//
// Lists furniture that is assumed to be used. 
//
void list_used_furniture(CHAR_DATA *ch, LIST *furniture) {
  OBJ_DATA *obj;
  LIST_ITERATOR *obj_i = newListIterator(furniture);

  ITERATE_LIST(obj, obj_i)
    // hmmm... how should we handle invisible furniture?
    list_one_furniture(ch, obj);
  deleteListIterator(obj_i);
}


//
// Get a sublist of characters not using furniture. The returned list must
// be deleted after use.
//
LIST *get_nofurniture_chars(CHAR_DATA *ch, LIST *list, 
			    bool invis_ok, bool include_self) {
  LIST *newlist = newList();
  LIST_ITERATOR *char_i = newListIterator(list);
  CHAR_DATA *i = NULL;

  ITERATE_LIST(i, char_i) {
    // don't show ourself
    if(i == ch && !include_self) continue;
    // check for invis and hidden ...
    if(!(invis_ok || can_see_char(ch, i)))
      continue;
    // make sure they're not on furniture
    if(charGetFurniture(i))
      continue;
    listPut(newlist, i);
  };
  deleteListIterator(char_i);
  return newlist;
}


//
// uhhh... "contents" isn't really the right word, since we list both
// objects AND characters. Alas, my lexicon is not as verbose as it
// could be.
//
void list_room_contents(CHAR_DATA *ch, ROOM_DATA *room) {
  LIST *list = NULL;

  list = get_nofurniture_chars(ch, roomGetCharacters(room), FALSE, FALSE);
  show_list(ch, list, charGetRdesc, charGetMultiRdesc, charGetVnum);
  deleteList(list);

  // show all of the objects that have people using them
  list = get_used_items(ch, roomGetContents(room), FALSE);
  list_used_furniture(ch, list);
  deleteList(list);

  // show all of the objects that don't have people using them that we can see
  list = get_unused_items(ch, roomGetContents(room), FALSE);
  show_list(ch, list, objGetRdesc, objGetMultiRdesc, objGetVnum);
  deleteList(list);
}



//*****************************************************************************
//
// implementaiton of inform.h
// look_at_xxx and show_xxx functions.
//
//*****************************************************************************
void look_at_obj(CHAR_DATA *ch, OBJ_DATA *obj) {
  char *new_desc = tagEdescs(objGetEdescs(obj),
			     objGetDesc(obj),
			     "{c", "{g");
  send_to_char(ch, "{g%s", new_desc);
  free(new_desc);

  // list container-related stuff
  if(objIsType(obj, "container")) {
    send_to_char(ch, "{g%s is %s%s.\r\n",
		 objGetName(obj), 
		 (containerIsClosed(obj) ? "closed" : "opened"),
		 (containerIsLocked(obj) ? " and locked"  : ""));

    // print all of our contents
    if(listSize(objGetContents(obj)) > 0 && !containerIsClosed(obj)) {
      LIST *vis_contents = find_all_objs(ch, objGetContents(obj), "", 
					 NOTHING, TRUE);
      // make sure we can still see things
      if(listSize(vis_contents) > 0) {
	send_to_char(ch, "It contains:\r\n");
	show_list(ch, vis_contents, objGetName, objGetMultiName, objGetVnum);
      }
      deleteList(vis_contents);
    }
  }

  // list furniture-related stuff
  else if(objIsType(obj, "furniture")) {
    int num_sitters = listSize(objGetUsers(obj));

    send_to_char(ch, "\r\n");

    // print character names
    if(num_sitters > 0) {
      LIST *can_see = find_all_chars(ch, objGetUsers(obj), "", NOBODY, TRUE);
      listRemove(can_see, ch);

      char *chars = print_list(can_see, charGetName, charGetMultiName);
      if(*chars) send_to_char(ch, "{g%s %s %s %s%s.\r\n",
			      chars, (listSize(can_see) == 1 ? "is" : "are"),
			      (furnitureGetType(obj)==FURNITURE_AT ? "at":"on"),
			      objGetName(obj),
			      (charGetFurniture(ch) == obj ? " with you" : ""));
      deleteList(can_see);
      free(chars);
    }

    // print out how much room there is left on the furniture
    int seats_left = (furnitureGetCapacity(obj) - num_sitters);
    if(seats_left > 0)
      send_to_char(ch, "{g%s looks like it could fit %d more %s.\r\n",
		   objGetName(obj), seats_left, 
		   (seats_left == 1 ? "person" : "people"));
  }
}


void look_at_exit(CHAR_DATA *ch, EXIT_DATA *exit) {
  send_to_char(ch, "{g%s", (*exitGetDesc(exit) ? exitGetDesc(exit) :
			    NOTHING_SPECIAL"\r\n"));
  // we have a door ... gotta print it's status
  if(exitIsClosable(exit)) {
    if(!*exitGetName(exit))
      send_to_char(ch, "It is %s.\r\n", (exitIsClosed(exit) ? "closed":"open"));
    else {
      send_to_char(ch, "You see %s. (%s)\r\n", 
		   exitGetName(exit), (exitIsClosed(exit) ? "closed":"open"));
    }
  }

  // the door is not closed, list off the people we can see as well
  if(!exitIsClosed(exit)) {
    ROOM_DATA *room = worldGetRoom(gameworld, exitGetTo(exit));
    if(room != NULL)
      list_room_contents(ch, room);
  }
}


void list_room_exits(CHAR_DATA *ch, ROOM_DATA *room) {
  char buf[20]; // for room vnums
  EXIT_DATA *exit;
  ROOM_DATA *to;
  int i;

  // list normal exits
  for(i = 0; i < NUM_DIRS; i++) {
    if((exit = roomGetExit(room, i)) == NULL)
      continue;
    if(!can_see_exit(ch, exit))
      continue;
    else if((to = worldGetRoom(gameworld, exitGetTo(exit))) == NULL) {
      log_string("ERROR: room %d heads %s to room %d, which does not exist.",
		 roomGetVnum(room), dirGetName(i), exitGetTo(exit));
      continue;
    }
    
    if(bitIsOneSet(charGetUserGroups(ch), "builder"))
      sprintf(buf, "[%d] ", roomGetVnum(to));
    else
      buf[0] = '\0';
    
    send_to_char(ch, "{g  %-10s :: %s%s\r\n", 
		 dirGetName(i),
		 buf,
		 (exitIsClosed(exit) ? 
		  // if it's closed, print the exit name
		  (*exitGetName(exit) ? exitGetName(exit) : "closed" ) :
		  // if it's open, print where it leads to
		  roomGetName(to)));
  }


  // list special exits
  int num_spec_exits = 0;
  const char **names = roomGetExitNames(room, &num_spec_exits);
  for(i = 0; i < num_spec_exits; i++) {
    if((exit = roomGetExitSpecial(room, names[i])) == NULL)
      continue;
    else if(!can_see_exit(ch, exit))
      continue;
    else if((to = worldGetRoom(gameworld, exitGetTo(exit))) == NULL) {
       log_string("ERROR: room %d exits (%s) to room %d, which does not exist.",
		 roomGetVnum(room), names[i], exitGetTo(exit));
       continue;
    }

    if(bitIsOneSet(charGetUserGroups(ch), "builder"))
      sprintf(buf, "[%d] ", roomGetVnum(to));
    else
      buf[0] = '\0';
    
    send_to_char(ch, "{g  %-10s :: %s%s\r\n", 
		 names[i],
		 buf,
		 (exitIsClosed(exit) ? 
		  // if it's closed, print the exit name
		  (exitGetName(exit) ? exitGetName(exit) : "closed" ) :
		  // if it's open, print where it leads to
		  roomGetName(to)));
  }


  // clean up our mess
  free(names);
}


void show_body(CHAR_DATA *ch, BODY_DATA *body) {
  int i, num_bodyparts;
  const char **bodyparts = bodyGetParts(body, TRUE, &num_bodyparts);
  OBJ_DATA *equipment = NULL;
  char posbuf[MAX_BUFFER];
  for(i = 0; i < num_bodyparts; i++) {
    equipment = bodyGetEquipment(body, bodyparts[i]);
    if(!equipment || !can_see_obj(ch, equipment))
      continue;

    sprintf(posbuf, "{c<{C%s{c>{n", bodyparts[i]);
    send_to_char(ch, "%-30s %s\r\n", 
		 posbuf, objGetName(equipment));
  }
  if(bodyparts) 
    free(bodyparts);
}


void look_at_char(CHAR_DATA *ch, CHAR_DATA *vict) {
  send_to_char(ch, "%s", (*charGetDesc(vict) ? 
			  charGetDesc(vict) : NOTHING_SPECIAL"\r\n"));
  show_body(ch, charGetBody(vict));
}


void look_at_room(CHAR_DATA *ch, ROOM_DATA *room) {
  if(bitIsOneSet(charGetUserGroups(ch), "builder"))
    send_to_char(ch, "{c[%d] ", roomGetVnum(room));

  send_to_char(ch, "{c%s\r\n", roomGetName(room));

  const char *desc = roomGetDesc(room);
#ifdef MODULE_TIME
  if((is_evening() || is_night()) && *roomGetNightDesc(room))
    desc = roomGetNightDesc(room);
#endif
  char *colorize_desc = tagEdescs(roomGetEdescs(room), desc, "{c", "{g");
  send_to_char(ch, "{g%s", colorize_desc);
  free(colorize_desc);
    
  list_room_exits(ch, room);
  list_room_contents(ch, room);

  send_to_char(ch, "{n");
};



//*****************************************************************************
//
// implementaiton of inform.h
// send_to_xxx functions
//
//*****************************************************************************
void send_outdoors(const char *format, ...) {
  if(format && *format) {
    // form the message
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);

    // send it out to everyone
    LIST_ITERATOR *list_i = newListIterator(mobile_list);
    CHAR_DATA *ch = NULL;
    ITERATE_LIST(ch, list_i)
      if(roomGetTerrain(charGetRoom(ch)) != TERRAIN_INDOORS &&
	 roomGetTerrain(charGetRoom(ch)) != TERRAIN_CAVERN)
	text_to_char(ch, buf);
    deleteListIterator(list_i);
  }
}


void text_to_char(CHAR_DATA *ch, const char *txt) {
  //  if (txt && *txt && charGetSocket(ch) && 
  //      socketGetState(charGetSocket(ch)) == STATE_PLAYING) {
  if(txt && *txt && charGetSocket(ch)) {
    text_to_buffer(charGetSocket(ch), txt);
    socketBustPrompt(charGetSocket(ch));
  }

  // if it's a PC or we are not in game, then
  // don't send the mesage to us
  if(!charIsNPC(ch))
    try_log(charGetName(ch), txt);
}


void send_to_char(CHAR_DATA *ch, const char *format, ...) {
  if(charGetSocket(ch) && format && *format) {
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);
    text_to_char(ch, buf);
    return;
  }
}


void send_around_char(CHAR_DATA *ch, bool hide_nosee, const char *format, ...) {
  static char buf[MAX_BUFFER];
  va_list args;
  va_start(args, format);
  vsprintf(buf, format, args);
  va_end(args);

  LIST_ITERATOR *room_i = newListIterator(roomGetCharacters(charGetRoom(ch)));
  CHAR_DATA       *vict = NULL;

  ITERATE_LIST(vict, room_i) {
    if(ch == vict)
      continue;
    if(hide_nosee && !can_see_char(vict, ch))
      continue;
    text_to_char(vict, buf);
  }
  deleteListIterator(room_i);
  return;
}


void send_to_groups(const char *groups, const char *format, ...) {
  static char buf[MAX_BUFFER];
  va_list args;
  va_start(args, format);
  vsprintf(buf, format, args);
  va_end(args);

  LIST_ITERATOR *ch_i = newListIterator(mobile_list);
  CHAR_DATA       *ch = NULL;

  ITERATE_LIST(ch, ch_i) {
    if(!charGetSocket(ch) || !bitIsSet(charGetUserGroups(ch), groups))
      continue;
    text_to_char(ch, buf);
  }
  deleteListIterator(ch_i);
}


void send_to_list(LIST *list, const char *format, ...) {
  if(format && *format) {
    // form the message
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);

    // send it out to everyone
    LIST_ITERATOR *list_i = newListIterator(list);
    CHAR_DATA *ch = NULL;
    ITERATE_LIST(ch, list_i)
      text_to_char(ch, buf);
    deleteListIterator(list_i);
  }
};



//*****************************************************************************
//
// implementation of inform.h
// game commands
//
//*****************************************************************************

//
// cmd_more skips onto the next page in the socket's read buffer
//
COMMAND(cmd_more) {
  if(charGetSocket(ch))
    page_continue(charGetSocket(ch));
}


//
// cmd_back goes back to the previous page in the socket's read buffer
//
COMMAND(cmd_back) {
  if(charGetSocket(ch))
    page_back(charGetSocket(ch));
}


//
// lists all of the commands available to a character via some group. If the
// character is not part of the group, s/he cannot see the commands
COMMAND(cmd_groupcmds) {
  if(!*arg)
    send_to_char(ch, "Which group did you want to commands for: %s\r\n",
		 bitvectorGetBits(charGetUserGroups(ch)));
  else if(!bitIsOneSet(charGetUserGroups(ch), arg))
    send_to_char(ch, "You are not a member of user group, %s.\r\n", arg);
  else
    show_commands(ch, arg);
}


//
// cmd_look lets a character get information about a specific thing. Objects,
// characters, extra descriptions, and just about anything else that can exist
// in the MUD can be looked at.
//   usage: look <at> [target] [on thing] [in thing]
//
//   examples:
//     look bob                      look at bob
//     look at 2.woman               look at the 2nd woman
//     look at sword in sheath       look at the sword in the sheath
//     look at earrings on sue       look at the earrings sue is wearing
//
COMMAND(cmd_look) {
  if(!arg || !*arg)
    look_at_room(ch, charGetRoom(ch));
  else {
    int found_type = FOUND_NONE;
    void *found;

    found = generic_find(ch, arg,
			 FIND_TYPE_ALL,
			 FIND_SCOPE_IMMEDIATE,
			 FALSE, &found_type);
    // nothing!
    if(!found)
      send_to_char(ch, "What did you want to look at?\r\n");

    // is it an extra description?
    else if(found_type == FOUND_EDESC) {
      // get the set it belongs to
      EDESC_SET *set = edescGetSet(found);
      // if it belongs to a set, highlight keywords
      if(set) {
	char *new_edesc = tagEdescs(set,
				    edescSetGetDesc(found),
				    "{c", "{g");
	send_to_char(ch, "{g%s", new_edesc);
	free(new_edesc);
      }
      // otherwise, just show the plain desc
      else
	send_to_char(ch, "{g%s", edescSetGetDesc(found));
    }

    // is it an item?
    else if(found_type == FOUND_OBJ)
      look_at_obj(ch, found);

    // is it something inside of an object?
    else if(found_type == FOUND_IN_OBJ) {
      // show the destination we're peering at
      if(objIsType(found, "portal")) {
	ROOM_DATA *dest = worldGetRoom(gameworld, portalGetDest(found));
	if(dest) {
	  send_to_char(ch, "You peer inside %s.\r\n", see_obj_as(ch, found));
	  look_at_room(ch, dest);
	}
	else
	  send_to_char(ch, 
		       "%s is murky, and you cannot "
		       "make out anything on the other side.\r\n",
		       see_obj_as(ch, found));
      }
      else if(!objIsType(found, "container"))
	send_to_char(ch, "%s is not a container or portal.\r\n",
		     objGetName(found));
      else if(containerIsClosed(found))
	send_to_char(ch, "%s is closed.\r\n", objGetName(found));
      else if(listSize(objGetContents(found)) == 0)
	send_to_char(ch, "There is nothing inside of %s.\r\n", 
		     objGetName(found));
      else {
	send_to_char(ch, "You peer inside of %s:\r\n", objGetName(found));
	LIST *vis_objs = find_all_objs(ch, objGetContents(found), "",
				       NOTHING, TRUE);
	show_list(ch, vis_objs, objGetName, objGetMultiName, objGetVnum);
	deleteList(vis_objs);
      }
    }

    // is it another character?
    else if(found_type == FOUND_CHAR)
      look_at_char(ch, found);

    // is it an exit?
    else if(found_type == FOUND_EXIT)
      look_at_exit(ch, found);

    // couldn't find anything. too bad!
    else
      send_to_char(ch, "What did you want to at?\r\n");
  }
}


//
// list all of the equipment a character is wearing to him or herself
//
COMMAND(cmd_equipment) {
  send_to_char(ch, "You are wearing:\r\n");
  show_body(ch, charGetBody(ch));
}


//
// list a character's inventory to him or herself
//
COMMAND(cmd_inventory) {
  if(listSize(charGetInventory(ch)) == 0)
    send_to_char(ch, "You aren't carrying anything.\r\n");
  else {
    send_to_char(ch, "{gYou are carrying:\r\n");
    LIST *vis_objs = find_all_objs(ch, charGetInventory(ch), "", NOTHING, TRUE);
    show_list(ch, vis_objs, objGetName, objGetMultiName, objGetVnum);
    deleteList(vis_objs);
  }
}


//
// show a list of all commands available to the character
//
COMMAND(cmd_commands) {
  show_commands(ch, bitvectorGetBits(charGetUserGroups(ch)));
}


//
// show the player all of the people who are currently playing
COMMAND(cmd_who)
{
  CHAR_DATA *plr;
  SOCKET_DATA *dsock;
  BUFFER *buf = newBuffer(MAX_BUFFER);
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  int socket_count = 0, playing_count = 0;

  bprintf(buf, 
	  "{cPlayers Online:\r\n"
	  "{gStatus   Race )\r\n"
	  );

  // build our list of people online
  ITERATE_LIST(dsock, sock_i) {
    socket_count++;
    if ((plr = socketGetChar(dsock)) == NULL) continue;
    playing_count++;
    bprintf(buf, "{y%-8s %-3s  {g)  {c%-12s {b%26s\r\n",
	    (bitIsSet(charGetUserGroups(plr), "admin") ? "admin" :
	     (bitIsSet(charGetUserGroups(plr), "scripter") ? "scripter" :
	      (bitIsSet(charGetUserGroups(plr), "builder") ? "builder"  :
	       (bitIsSet(charGetUserGroups(plr), "player") ? "player" : 
		"noone!")))),
	    raceGetAbbrev(charGetRace(plr)),
	    charGetName(plr), socketGetHostname(dsock));
  } deleteListIterator(sock_i);

  // send out info about the number of sockets and players logged on
  bprintf(buf, "\r\n{g%d socket%s connected. %d playing.\r\n",
	  socket_count, (socket_count == 1 ? "" : "s"), playing_count);
  page_string(charGetSocket(ch), bufferString(buf));
  deleteBuffer(buf);
}



//*****************************************************************************
// below this line are all of the subfunctions related to the message() 
// function
//*****************************************************************************

//
// Send a message out
//
// Converts the following symbols:
//  $n = ch name
//  $N = vict name
//  $m = him/her of char
//  $M = him/her of vict
//  $s = his/hers of char
//  $S = his/hers of vict
//  $e = he/she of char
//  $E = he/she of vict
//
//  $o = obj name
//  $O = vobj name
//  $a = a/an of obj
//  $A = a/an of vobj
void send_message(CHAR_DATA *to, 
		  const char *str,
		  CHAR_DATA *ch, CHAR_DATA *vict,
		  OBJ_DATA *obj, OBJ_DATA *vobj) {
  char buf[MAX_BUFFER];
  int i, j;

  // if there's nothing to send the message to, don't go through all
  // the work it takes to parse the string
  if(charGetSocket(to) == NULL)
    return;

  for(i = 0, j = 0; str[i] != '\0'; i++) {
    if(str[i] != '$') {
      buf[j] = str[i];
      j++;
    }
    else {
      i++;

      switch(str[i]) {
      case 'n':
	if(!ch) break;
	sprintf(buf+j, see_char_as(to, ch));
	while(buf[j] != '\0') j++;
	break;
      case 'N':
	if(!vict) break;
	sprintf(buf+j, see_char_as(to, vict));
	while(buf[j] != '\0') j++;
	break;
      case 'm':
	if(!ch) break;
	sprintf(buf+j, (can_see_char(to, ch) ? HIMHER(ch) : SOMEONE));
	while(buf[j] != '\0') j++;
	break;
      case 'M':
	if(!vict) break;
	sprintf(buf+j, (can_see_char(to, vict) ? HIMHER(vict) : SOMEONE));
	while(buf[j] != '\0') j++;
	break;
      case 's':
	if(!ch) break;
	sprintf(buf+j, (can_see_char(to, ch) ? HISHERS(ch) :SOMEONE"'s"));
	while(buf[j] != '\0') j++;
	break;
      case 'S':
	if(!vict) break;
	sprintf(buf+j, (can_see_char(to, vict) ? HISHERS(vict) :SOMEONE"'s"));
	while(buf[j] != '\0') j++;
	break;
      case 'e':
	if(!ch) break;
	sprintf(buf+j, (can_see_char(to, ch) ? HESHE(ch) : SOMEONE));
	while(buf[j] != '\0') j++;
	break;
      case 'E':
	if(!vict) break;
	sprintf(buf+j, (can_see_char(to, vict) ? HESHE(vict) : SOMEONE));
	while(buf[j] != '\0') j++;
	break;
      case 'o':
	if(!obj) break;
	sprintf(buf+j, see_obj_as(to, obj));
	while(buf[j] != '\0') j++;
	break;
      case 'O':
	if(!vobj) break;
	sprintf(buf+j, see_obj_as(to, vobj));
	while(buf[j] != '\0') j++;
	break;
      case 'a':
	if(!obj) break;
	sprintf(buf+j, AN(see_obj_as(to, obj)));
	while(buf[j] != '\0') j++;
	break;
      case 'A':
	if(!vobj) break;
	sprintf(buf+j, AN(see_obj_as(to, vobj)));
	while(buf[j] != '\0') j++;
	break;
      case '$':
	buf[j] = '$';
	j++;
	break;
      default:
	// do nothing
	break;
      }
    }
  }

  //  buf[0] = toupper(buf[0]);
  sprintf(buf+j, "\r\n");
  text_to_char(to, buf);
}


void message(CHAR_DATA *ch,  CHAR_DATA *vict,
	     OBJ_DATA  *obj, OBJ_DATA  *vobj,
	     int hide_nosee, bitvector_t range, 
	     const char *mssg) {
  if(!mssg || !*mssg)
    return;

  // what's our scope?
  if(IS_SET(range, TO_VICT) &&
     (!hide_nosee ||
      // make sure the vict can the character, or the
      // object if there is no character
      ((!ch || can_see_char(vict, ch)) &&
       (ch  || (!obj || can_see_obj(vict, obj))))))
    send_message(vict, mssg, ch, vict, obj, vobj);

  // characters can always see themselves. No need to do checks here
  if(IS_SET(range, TO_CHAR))
    send_message(ch, mssg, ch, vict, obj, vobj);

  LIST *recipients = NULL;
  // check if the scope of this message is everyone in the world
  if(IS_SET(range, TO_WORLD))
    recipients = mobile_list;
  else if(IS_SET(range, TO_ROOM))
    recipients = roomGetCharacters(charGetRoom(ch));

  // if we have a list to send the message to, do it
  if(recipients != NULL) {
    LIST_ITERATOR *rec_i = newListIterator(recipients);
    CHAR_DATA *rec = NULL;

    // go through everyone in the list
    ITERATE_LIST(rec, rec_i) {
      // if we wanted to send to ch or vict, we would have already...
      if(rec == vict || rec == ch)
	continue;
      if(rec == ch ||
	 (!hide_nosee ||
	  // make sure the vict can see the character, or the
	  // object if there is no character
	  ((!ch || can_see_char(rec, ch)) &&
	   (ch  || (!obj || can_see_obj(rec, obj))))))
      send_message(rec, mssg, ch, vict, obj, vobj);
    } deleteListIterator(rec_i);
  }
}

void mssgprintf(CHAR_DATA *ch, CHAR_DATA *vict, 
		OBJ_DATA *obj, OBJ_DATA  *vobj,
		int hide_nosee, bitvector_t range, const char *fmt, ...) {
  if(fmt && *fmt) {
    // form the message
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    message(ch, vict, obj, vobj, hide_nosee, range, buf);
  }
}
