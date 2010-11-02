//*****************************************************************************
//
// cmd_builder.c
//
// various utilities (non-OLC) for builders, such as digging/filling exits, 
// listing zone scripts/rooms/etc, and utilities for loading/purging mobs and
// objects.
//
//*****************************************************************************

#include "mud.h"
#include "world.h"
#include "zone.h"
#include "room.h"
#include "room_reset.h"
#include "character.h"
#include "object.h"
#include "handler.h"
#include "inform.h"
#include "utils.h"
#include "prototype.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "items/items.h"



//
// Load a copy of a specific mob/object
// usage: load <mob | obj> <vnum>
COMMAND(cmd_load) {
  if(!arg || !*arg)
    send_to_char(ch, "What did you want to load?\r\n");
  else {
    char   type[SMALL_BUFFER];
    char   name[SMALL_BUFFER];
    char locale[SMALL_BUFFER];
    char    key[SMALL_BUFFER];
    arg = one_arg(arg, type);
    if(!parse_worldkey_relative(ch, arg, name, locale)) {
      send_to_char(ch, "What did you want to load?\r\n");
      return;
    }

    sprintf(key, "%s@%s", name, locale);
    if(!strncasecmp("mobile", type, strlen(type))) {
      PROTO_DATA *proto = worldGetType(gameworld, "mproto", key);
      if(proto == NULL)
	send_to_char(ch, "No mobile prototype exists with that key.\r\n");
      else if(protoIsAbstract(proto))
	send_to_char(ch, "That prototype is abstract.\r\n");
      else {
	CHAR_DATA *mob = protoMobRun(proto);
	if(mob == NULL)
	  send_to_char(ch, "There was an error running the prototype.\r\n");
	else {
	  send_to_char(ch, "You create %s.\r\n", charGetName(mob));
	  char_to_room(mob, charGetRoom(ch));
	}
      }
    }

    else if(!strncasecmp("object", type, strlen(type))) {
      PROTO_DATA *proto = worldGetType(gameworld, "oproto", key);
      if(proto == NULL)
	send_to_char(ch, "No object prototype exists with that key.\r\n");
      else if(protoIsAbstract(proto))
	send_to_char(ch, "That prototype is abstract.\r\n");
      else {
	OBJ_DATA *obj = protoObjRun(proto);
	if(obj == NULL)
	  send_to_char(ch, "There was an error running the prototype.\r\n");
	else {
	  send_to_char(ch, "You create %s.\r\n", objGetName(obj));
	  obj_to_char(obj, ch);
	}
      }
    }

    // type not found
    else 
      send_to_char(ch, "What type of thing did you want to load?\r\n");
  }
}


//
// remove an object or player from the game. If no argument is supplied, all
// objects and non-player characters are removed from the current room.
//   usage: purge <target>
COMMAND(cmd_purge) {
  void    *found = NULL;
  int found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, "| { ch.room.noself obj.room }",
		 &found, &found_type))
    return;

  // purge everything in the current room
  if(found == NULL) {
    LIST_ITERATOR *list_i = newListIterator(roomGetContents(charGetRoom(ch)));
    OBJ_DATA *obj;
    CHAR_DATA *vict;

    send_to_char(ch, "You purge the room.\r\n");
    message(ch, NULL, NULL, NULL, FALSE, TO_ROOM,
	    "$n raises $s arms, and white flames engulf the entire room.");

    // purge all the objects. 
    ITERATE_LIST(obj, list_i)
      extract_obj(obj);
    deleteListIterator(list_i);

    // and now all of the non-characters
    list_i = newListIterator(roomGetCharacters(charGetRoom(ch)));
    ITERATE_LIST(vict, list_i) {
      if(vict == ch || !charIsNPC(vict)) 
	continue;
      extract_mobile(vict);
    }
    deleteListIterator(list_i);
  }

  // purge characters
  else if(found_type == PARSE_CHAR) {
    // we can only purge him if we have all the same groups as him, and more
    if(!charHasMoreUserGroups(ch, found))
      send_to_char(ch, "Erm, you better not try that on %s. %s has "
		   "just as much priviledges as you.\r\n", 
		   HIMHER(found), HESHE(found));
    else {
      send_to_char(ch, "You purge %s.\r\n", charGetName(found));
      message(ch, found, NULL, NULL, FALSE, TO_ROOM,
	      "$n raises $s arms, and white flames engulf $N.");
      extract_mobile(found);
    }
  }

  // purge objects
  else if(found_type == PARSE_OBJ) {
    send_to_char(ch, "You purge %s.\r\n", objGetName(found));
    message(ch, NULL, found, NULL, FALSE, TO_ROOM,
	    "$n raises $s arms, and white flames engulf $o.");
    obj_from_room(found);
    extract_obj(found);
  }
}


//
// reruns the room's load script, and replaces the old version of the room with
// the new one.
COMMAND(cmd_rreload) {
  PROTO_DATA   *proto = NULL;
  ROOM_DATA *old_room = NULL;
  ROOM_DATA *new_room = NULL;
  ZONE_DATA     *zone = NULL;
  const char     *key = roomGetClass(charGetRoom(ch));

  // unless an arg is supplied, we're working on the current room.
  if(arg && *arg)
    key = get_fullkey_relative(arg, get_key_locale(key));

  // make sure all of our requirements are met
  if( (zone = worldGetZone(gameworld, get_key_locale(key))) == NULL)
    send_to_char(ch, "That zone does not exist!\r\n");
  else if(!canEditZone(zone, ch))
    send_to_char(ch, "You are not authorized to edit that zone.\r\n");
  else if( (proto = worldGetType(gameworld, "rproto", key)) == NULL)
    send_to_char(ch, "No prototype for that room exists.\r\n");
  else if(!worldRoomLoaded(gameworld, key))
    send_to_char(ch, "No room with that key is currently loaded.\r\n");
  else {
    // try running the proto to get our new room...
    old_room = worldGetRoom(gameworld, key);
    new_room = protoRoomRun(proto);
    if(new_room == NULL)
      send_to_char(ch, "There was an error reloading the room.\r\n");
    else {
      do_mass_transfer(old_room, new_room, TRUE, TRUE, TRUE);
      extract_room(old_room);
      worldPutRoom(gameworld, key, new_room);
      send_to_char(ch, "Room reloaded.\r\n");
    }
  }
}


//
// trigger all of a specified zone's reset scripts and such. If no vnum is
// supplied, the zone the user is currently in is reset.
//   usage: zreset <zone vnum>
COMMAND(cmd_zreset) {
  ZONE_DATA *zone = NULL;

  if(!arg || !*arg)
    zone= worldGetZone(gameworld,get_key_locale(roomGetClass(charGetRoom(ch))));
  else
    zone= worldGetZone(gameworld, arg);

  if(zone == NULL)
    send_to_char(ch, "Which zone did you want to reset?\r\n");
  else if(!canEditZone(zone, ch))
    send_to_char(ch, "You are not authorized to edit that zone.\r\n");
  else {
    send_to_char(ch, "%s has been reset.\r\n", zoneGetName(zone));
    zoneForceReset(zone);
  }
}



//*****************************************************************************
// Functions for deleting different prototype
//*****************************************************************************
COMMAND(cmd_rdelete) {
  if(do_delete(ch, "rproto", deleteProto, arg)) {
    do_delete(ch, "reset", deleteResetList, arg);
    send_to_char(ch, "If the room has already been used, do not forget to "
		 "also purge the current instance of it.\r\n");
  }
}

COMMAND(cmd_mdelete) {
  do_delete(ch, "mproto", deleteProto, arg);
}

COMMAND(cmd_odelete) {
  do_delete(ch, "oproto", deleteProto, arg);
}



//*****************************************************************************
// Functions for listing different types of data (zones, mobs, objs, etc...)
//*****************************************************************************
//
// returns yes/no if the prototype is abstract or not
const char *prototype_list_info(PROTO_DATA *data) {
  static char buf[SMALL_BUFFER];
  sprintf(buf, "%-40s %13s", 
	  (*protoGetParents(data) ? protoGetParents(data) : "-------"),
	  (protoIsAbstract(data)  ? "yes" : "no"));
  return buf;
}

// this is used for the header when printing out zone proto info
#define PROTO_LIST_HEADER \
"Parents                                       Abstract"


COMMAND(cmd_rlist) {
  do_list(ch, (arg&&*arg?arg:get_key_locale(roomGetClass(charGetRoom(ch)))),
	  "rproto", PROTO_LIST_HEADER, prototype_list_info);
}

COMMAND(cmd_mlist) {
  do_list(ch, (arg&&*arg?arg:get_key_locale(roomGetClass(charGetRoom(ch)))),
	  "mproto", PROTO_LIST_HEADER, prototype_list_info);
}

COMMAND(cmd_olist) {
  do_list(ch, (arg&&*arg?arg:get_key_locale(roomGetClass(charGetRoom(ch)))),
	  "oproto", PROTO_LIST_HEADER, prototype_list_info);
}

COMMAND(cmd_mrename) {
  char from[SMALL_BUFFER];
  arg = one_arg(arg, from);
  do_rename(ch, "mproto", from, arg);
}

COMMAND(cmd_rrename) {
  char from[SMALL_BUFFER];
  arg = one_arg(arg, from);
  if(do_rename(ch, "rproto", from, arg)) {
    do_rename(ch, "reset", from, arg);
    send_to_char(ch, "No not forget to purge any instances of %s already "
		 "loaded.\r\n", from); 
  }
}

COMMAND(cmd_orename) {
  char from[SMALL_BUFFER];
  arg = one_arg(arg, from);
  do_rename(ch, "oproto", from, arg);
}

COMMAND(cmd_zlist) {
  LIST *keys = worldGetZoneKeys(gameworld);

  // first, order all the zones
  listSortWith(keys, strcasecmp);

  // now, iterate across them all and show them
  LIST_ITERATOR *zone_i = newListIterator(keys);
  ZONE_DATA       *zone = NULL;
  char             *key = NULL;

  send_to_char(ch,
" {wKey            Name                                             Editors  Timer\r\n"
"{b--------------------------------------------------------------------------------\r\n{n");

  ITERATE_LIST(key, zone_i) {
    if( (zone = worldGetZone(gameworld, key)) != NULL) {
      send_to_char(ch, " {c%-14s %-30s %25s  {w%5d\r\n", key, zoneGetName(zone),
		   zoneGetEditors(zone), zoneGetPulseTimer(zone));
    }
  } deleteListIterator(zone_i);
  deleteListWith(keys, free);
  send_to_char(ch, "{g");
}
