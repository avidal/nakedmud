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
#include "exit.h"
#include "movement.h"
#include "character.h"
#include "object.h"
#include "handler.h"
#include "inform.h"
#include "utils.h"
#include "races.h"
#include "dialog.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/script.h"
#include "scripts/script_set.h"
#include "items/items.h"



//
// Try to dig a special exit in a specific direction. Unlike cmd_dig,
// specdig does not link the room we're digging to, back to us, since
// we can't really figure out what the opposite direction is
void try_specdig(CHAR_DATA *ch, const char *dir, int to) {
  if(roomGetExitSpecial(charGetRoom(ch), dir))
    send_to_char(ch, "You must fill in the %s exit before you dig a new exit.\r\n", dir);
  else if(!worldGetRoom(gameworld, to))
    send_to_char(ch, "The destination, %d, does not exist.\r\n", to);
  else {
    roomDigExitSpecial(charGetRoom(ch), dir, to);
    send_to_char(ch, "%s now leads to %s.\r\n", 
		 dir, roomGetName(worldGetRoom(gameworld, to)));
  }
}


//
// Try to fill in a special exit in a specific direction. Unlike cmd_dig,
// specdig does not fill in the exit on the other side, since we can't
// really figure out what the opposite direction is.
void try_specfill(CHAR_DATA *ch, const char *dir) {
  if(!roomGetExitSpecial(charGetRoom(ch), dir))
    send_to_char(ch, "There doesn't seem to be an exit in that direction.\r\n");
  else {
    // delete the exit
    roomSetExitSpecial(charGetRoom(ch), dir, NULL);
    send_to_char(ch, "You fill in %s.\r\n", dir);
  }
}


//
// creates an exit in a direction to a specific room
// usage: dig [dir] [room]
//
COMMAND(cmd_dig) {
  char buf[MAX_INPUT_LEN];
  int dir;
  int to;

  // make sure we have input
  if(!arg || !*arg) {
    send_to_char(ch, "dig [direction] [room]\r\n");
    return;
  }

  sscanf(arg, "%s %d", buf, &to);
  dir = dirGetNum(buf);

  if(dir == DIR_NONE)
    dir = dirGetAbbrevNum(buf);


  if(!canEditZone(worldZoneBounding(gameworld,
				    roomGetVnum(charGetRoom(ch))), ch))
    send_to_char(ch, "You are not authorized to edit this zone.\r\n");
  else if(dir == DIR_NONE)
    try_specdig(ch, buf, to);
  else if(roomGetExit(charGetRoom(ch), dir))
    send_to_char(ch, "You must fill in the exit %s first, before you dig a new exit.\r\n", dirGetName(dir));
  else if(!worldGetRoom(gameworld, to))
    send_to_char(ch, "The destination, %d, does not exist.\r\n", to);
  else {
    ROOM_DATA *to_room = worldGetRoom(gameworld, to);
    roomDigExit(charGetRoom(ch), dir, to);

    // link back to us if possible ... make sure we can edit the zone, too
    if(!roomGetExit(worldGetRoom(gameworld, to), dirGetOpposite(dir)) &&
       canEditZone(worldZoneBounding(gameworld, roomGetVnum(to_room)), ch))
      roomDigExit(to_room,
		  dirGetOpposite(dir), 
		  roomGetVnum(charGetRoom(ch)));

    send_to_char(ch, "You dig %s to %s.\r\n", 
		 dirGetName(dir), roomGetName(worldGetRoom(gameworld, to)));

    // save the changes... this will get costly as our world gets bigger.
    // But that should be alright once we make zone saving a bit smarter
    worldSave(gameworld, WORLD_PATH);
  }
}


//
// fills in an exit in a specific direction
// usage: fill [dir]
//
COMMAND(cmd_fill) {
  int dir;

  if(!arg || !*arg) {
    send_to_char(ch, "fill [direction]\r\n");
    return;
  }

  dir = dirGetNum(arg);

  if(dir == DIR_NONE)
    dir = dirGetAbbrevNum(arg);

  if(!canEditZone(worldZoneBounding(gameworld,roomGetVnum(charGetRoom(ch))),ch))
    send_to_char(ch, "You are not authorized to edit this zone.\r\n");
  else if(dir == DIR_NONE)
    try_specfill(ch, arg);
  else if(!roomGetExit(charGetRoom(ch), dir))
    send_to_char(ch, "There doesn't seem to be an exit in that direction.\r\n");
  else {
    ROOM_DATA *exit_to   = NULL;
    EXIT_DATA *exit_back = NULL;

    exit_to = worldGetRoom(gameworld,
			   exitGetTo(roomGetExit(charGetRoom(ch), dir)));
    if(exit_to)
      exit_back = roomGetExit(exit_to, dirGetOpposite(dir));

    // see if the room we're filling leads back to us... if so, fill it in
    if(exit_back &&
       exitGetTo(exit_back) == roomGetVnum(charGetRoom(ch)))
      roomSetExit(exit_to, dirGetOpposite(dir), NULL);

    // delete the exit
    roomSetExit(charGetRoom(ch), dir, NULL);
    send_to_char(ch, "You fill in the exit to the %s.\r\n", dirGetName(dir));

    // save the changes... this will get costly as our world gets bigger.
    // But that should be alright once we make zone saving a bit smarter
    worldSave(gameworld, WORLD_PATH);
  }
}


//
// Load a copy of a specific mob/object
// usage: load [mob | obj] [vnum]
//
COMMAND(cmd_load) {
  if(!arg || !*arg)
    send_to_char(ch, "What did you want to load?\r\n");
  else {
    char type[MAX_BUFFER];
    int vnum;

    sscanf(arg, "%s %d", type, &vnum);

    if(strlen(type) < 1)
      send_to_char(ch, "What did you want to load?\r\n");

    else if(!strncasecmp("mobile", type, strlen(type))) {
      CHAR_DATA *mob = worldGetMob(gameworld, vnum);
      if(mob == NULL)
	send_to_char(ch, "No mobile exists with that vnum.\r\n");
      // make a copy and place it in the world
      else {
	mob = charCopy(mob);
	send_to_char(ch, "You create %s.\r\n", charGetName(mob));
	char_to_game(mob);
	char_to_room(mob, charGetRoom(ch));

	// check for initialization scripts
	try_scripts(SCRIPT_TYPE_INIT,
		    mob, SCRIPTOR_CHAR,
		    NULL, NULL, charGetRoom(mob), NULL, NULL, 0);
      }
    }

    else if(!strncasecmp("object", type, strlen(type))) {
      OBJ_DATA *obj = worldGetObj(gameworld, vnum);
      if(obj == NULL)
	send_to_char(ch, "No object exists with that vnum.\r\n");
      else {
	obj = objCopy(obj);
	send_to_char(ch, "You create %s.\r\n", objGetName(obj));
	obj_to_game(obj);
	obj_to_char(obj, ch);

	// check for initialization scripts
	try_scripts(SCRIPT_TYPE_INIT,
		    obj, SCRIPTOR_OBJ,
		    ch, NULL, charGetRoom(ch), NULL, NULL, 0);
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
//
COMMAND(cmd_purge) {
  // purge everything
  if(!arg || !*arg) {
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

  // purge one specific thing
  else {

    int found_type = FOUND_NONE;
    void *found;
    
    found = generic_find(ch, arg,
			 FIND_TYPE_CHAR | FIND_TYPE_OBJ,
			 FIND_SCOPE_ROOM | FIND_SCOPE_VISIBLE,
			 FALSE, &found_type);

    // make sure we found something
    if(found == NULL)
      send_to_char(ch, "Purge what?\r\n");

    // purge characters
    else if(found_type == FOUND_CHAR) {
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
    else if(found_type == FOUND_OBJ) {
      send_to_char(ch, "You purge %s.\r\n", objGetName(found));
      message(ch, NULL, found, NULL, FALSE, TO_ROOM,
	      "$n raises $s arms, and white flames engulf $o.");
      obj_from_room(found);
      extract_obj(found);
    }
    else
      send_to_char(ch, "What did you want to purge?\r\n");
  }
}


//
// trigger all of a specified zone's reset scripts and such. If no vnum is
// supplied, the zone the user is currently in is reset.
//   usage: zreset <zone vnum>
//
COMMAND(cmd_zreset) {
  ZONE_DATA *zone = NULL;

  if(!arg || !*arg)
    zone = worldZoneBounding(gameworld, roomGetVnum(charGetRoom(ch)));
  else
    zone = worldGetZone(gameworld, atoi(arg));

  if(zone == NULL)
    send_to_char(ch, "Which zone did you want to reset?\r\n");
  else if(!canEditZone(zone, ch))
    send_to_char(ch, "You are not authorized to edit that zone.\r\n");
  else {
    send_to_char(ch, "Zone %d has been reset.\r\n", zoneGetVnum(zone));
    zoneForceReset(zone);
  }
}


//
// toggle buildwalking on and off
//
COMMAND(cmd_buildwalk) {
  bitToggle(charGetPrfs(ch), "buildwalk");
  send_to_char(ch, "Buildwalk %s.\r\n",
	       (bitIsOneSet(charGetPrfs(ch), "buildwalk") ? "on":"off"));
}


//
// attach a new script to the given instanced object/mobile/room
COMMAND(cmd_attach) {
  char vbuf[SMALL_BUFFER];
  arg = one_arg(arg, vbuf);
  int vnum = atoi(vbuf);

  // check to make sure our vnum is OK
  if(!isdigit(*vbuf) || vnum == 0)
    send_to_char(ch, "Invalid script vnum!\r\n");
  else if(!worldGetScript(gameworld, vnum))
    send_to_char(ch, "No script exists with that vnum.\r\n");
  // we're trying to edit the room, not a char or mobile
  if(!strcasecmp(arg, "room")) {
    scriptSetAdd(roomGetScripts(charGetRoom(ch)), vnum);
    send_to_char(ch, "Script number %d attached to the room.\r\n", vnum);
  }
  else {
    int tgt_type = FOUND_NONE;
    void    *tgt = generic_find(ch, arg, FIND_TYPE_CHAR | FIND_TYPE_OBJ,
				FIND_SCOPE_IMMEDIATE, FALSE, &tgt_type);

    // make sure the target exists
    if(tgt == NULL || tgt_type == FOUND_NONE)
      send_to_char(ch, "Target not found.\r\n");
    else if(tgt_type == FOUND_CHAR) {
      send_to_char(ch, "Script number %d attached to %s.\r\n", vnum,
		   charGetName(tgt));
      scriptSetAdd(charGetScripts(tgt), vnum);
    }
    else {
      send_to_char(ch, "Script number %d attached to %s.\r\n", vnum,
		   objGetName(tgt));
      scriptSetAdd(objGetScripts(tgt), vnum);
    }
  }  
}


//
// attach a new script to the given instanced object/mobile/room
COMMAND(cmd_detach) {
  char vbuf[SMALL_BUFFER];
  arg = one_arg(arg, vbuf);
  int vnum = atoi(vbuf);

  // check to make sure our vnum is OK
  if(!isdigit(*vbuf) || vnum == 0)
    send_to_char(ch, "Invalid script vnum!\r\n");
  // we're trying to edit the room, not a char or mobile
  if(!strcasecmp(arg, "room")) {
    scriptSetRemove(roomGetScripts(charGetRoom(ch)), vnum);
    send_to_char(ch, "Script number %d detached from the room.\r\n", vnum);
  }
  else {
    int tgt_type = FOUND_NONE;
    void    *tgt = generic_find(ch, arg, FIND_TYPE_CHAR | FIND_TYPE_OBJ,
				FIND_SCOPE_IMMEDIATE, FALSE, &tgt_type);

    // make sure the target exists
    if(tgt == NULL || tgt_type == FOUND_NONE)
      send_to_char(ch, "Target not found.\r\n");
    else if(tgt_type == FOUND_CHAR) {
      send_to_char(ch, "Script number %d detached to %s.\r\n", vnum,
		   charGetName(tgt));
      scriptSetRemove(charGetScripts(tgt), vnum);
    }
    else {
      send_to_char(ch, "Script number %d detached to %s.\r\n", vnum,
		   objGetName(tgt));
      scriptSetRemove(objGetScripts(tgt), vnum);
    }
  }  
}



//*****************************************************************************
// Functions for deleting different prototype
//*****************************************************************************


//
// Delete an object, room, mobile, etc... from the game. First remove it from
// the gameworld, and then delete it and its contents. The onus is on the 
// builder to make sure deleting a prototype won't screw anything up (e.g.
// people standing about in a room when it's deleted).
void do_delete(CHAR_DATA *ch, void *remover, void *deleter, 
	       const char *datatype, const char *arg) {
  void *(* remove_func)(WORLD_DATA *world, int vnum) = remover;
  void  (* delete_func)(void *data)                  = deleter;
  
  if(!arg || !*arg || !isdigit(*arg))
    send_to_char(ch, "Which %s did you want to delete?\r\n", datatype);
  else {
    int vnum = atoi(arg);
    ZONE_DATA *zone = worldZoneBounding(gameworld, vnum);
    if(zone == NULL || !canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit that zone.\r\n");
    else {
      void *data = remove_func(gameworld, vnum);
      if(data == NULL)
	send_to_char(ch, "%s %d does not exist.\r\n", datatype, vnum);
      else {
	send_to_char(ch, "%s %d deleted.\r\n", datatype, vnum);
	delete_func(data);
	worldSave(gameworld, WORLD_PATH);
      }
    }
  }
}

COMMAND(cmd_scdelete) {
  do_delete(ch, worldRemoveScriptVnum, deleteScript, "script", arg);
}

COMMAND(cmd_rdelete) {
  do_delete(ch, worldRemoveRoomVnum, deleteRoom, "room", arg);
}

COMMAND(cmd_mdelete) {
  do_delete(ch, worldRemoveMobVnum, deleteChar, "mob", arg);
}

COMMAND(cmd_odelete) {
  do_delete(ch, worldRemoveObjVnum, deleteObj, "obj", arg);
}

COMMAND(cmd_ddelete) {
  do_delete(ch, worldRemoveDialogVnum, deleteDialog, "dialog", arg);
}



//*****************************************************************************
// Functions for listing different types of data (zones, mobs, objs, etc...)
//*****************************************************************************
const char *charGetListType(CHAR_DATA *ch) {
  return charGetRace(ch);
}

const char *roomGetListType(ROOM_DATA *room) {
  return terrainGetName(roomGetTerrain(room));
}

const char *scriptGetListType(SCRIPT_DATA *script) {
  return scriptTypeName(scriptGetType(script));
}


//
// Generic xxxlist for builders. If the thing to list doesn't have any types
// (e.g. dialogs) then type_namer can be NULL.
void do_list(CHAR_DATA *ch, 
	     void *getter, void *namer, void *type_namer, 
	     const char *datatype, const char *arg) {
  const char *(* type_naming_func)(void *)  = type_namer;
  const char *(* naming_func)(void *)       = namer;
  void *(* get_func)(void *, int)           = getter;
  ZONE_DATA *zone                           = NULL;
  
  if(!arg || !*arg)
    zone = worldZoneBounding(gameworld, roomGetVnum(charGetRoom(ch)));
  else
    zone = worldGetZone(gameworld, atoi(arg));

  if(zone == NULL)
    send_to_char(ch, "Which zone did you want to list %s for?\r\n", datatype);
  else {
    int vnum;
    send_to_char(ch,
" {wVnum  Name                                                                %s\r\n"
"{b-------------------------------------------------------------------------------{n\r\n",
		 (type_naming_func == NULL ? "" : "Type"));
    for(vnum = zoneGetMinBound(zone); vnum <= zoneGetMaxBound(zone); vnum++) {
      void *data = get_func(zone, vnum);
      if(data != NULL)
	send_to_char(ch, "{y[{c%4d{y] {c%-50s{w%22s{n\r\n", 
		     vnum, naming_func(data), 
		     (type_naming_func == NULL ? "" : type_naming_func(data)));
    }
  }
}

COMMAND(cmd_sclist) {
  do_list(ch, zoneGetScript, scriptGetName, scriptGetListType, "scripts", arg);
}

COMMAND(cmd_rlist) {
  do_list(ch, zoneGetRoom, roomGetName, roomGetListType, "rooms", arg);
}

COMMAND(cmd_mlist) {
  do_list(ch, zoneGetMob, charGetName, charGetListType, "mobs", arg);
}

COMMAND(cmd_olist) {
  do_list(ch, zoneGetObj, objGetName, objGetTypes, "objs", arg);
}

COMMAND(cmd_dlist) {
  do_list(ch, zoneGetDialog, dialogGetName, NULL, "dialogs", arg);
}

int zone_comparator(ZONE_DATA *zone1, ZONE_DATA *zone2) {
  if(zoneGetVnum(zone1) == zoneGetVnum(zone2))
    return 0;
  else if(zoneGetVnum(zone1) < zoneGetVnum(zone2))
    return -1;
  else
    return 1;
}

COMMAND(cmd_zlist) {
  LIST *zones = worldGetZones(gameworld);
  // first, order all the zones
  listSortWith(zones, zone_comparator);
  // now, iterate across them all and show them
  LIST_ITERATOR *zone_i = newListIterator(zones);
  ZONE_DATA *zone = NULL;

  send_to_char(ch,
" {wVnum  Name                                          Editors  Timer   Min   Max\r\n"
"{b-------------------------------------------------------------------------------\r\n{n"
	       );

  ITERATE_LIST(zone, zone_i)
    send_to_char(ch, 
		 "{y[{c%4d{y] {c%-30s %22s  {w%5d %5d %5d\r\n",
		 zoneGetVnum(zone), zoneGetName(zone), zoneGetEditors(zone),
		 zoneGetPulseTimer(zone), zoneGetMinBound(zone), 
		 zoneGetMaxBound(zone));
  deleteListIterator(zone_i);
}
