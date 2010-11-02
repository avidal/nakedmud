//*****************************************************************************
//
// handler.c
//
// this file contains implementations for all of the "handling" functions;
// functions that move things from place to place. e.g. adding and removing
// characters from rooms, as well as objects and the like.
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "world.h"
#include "room.h"
#include "exit.h"
#include "extra_descs.h"
#include "character.h"
#include "object.h"
#include "body.h"
#include "inform.h"
#include "hooks.h"
#include "handler.h"
#include "commands.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "items/items.h"
#include "items/container.h"
#include "items/worn.h"



//*****************************************************************************
// obj/char from/to functions
//*****************************************************************************
void exit_to_game(EXIT_DATA *exit) {
  propertyTablePut(exit_table, exit);
}

void obj_to_game(OBJ_DATA *obj) {
  listPut(object_list, obj);
  propertyTablePut(obj_table, obj);

  // execute all of our to_game hooks
  hookRun("obj_to_game", obj, NULL, NULL);

  // also add all contents
  if(listSize(objGetContents(obj)) > 0) {
    LIST_ITERATOR *cont_i = newListIterator(objGetContents(obj));
    OBJ_DATA *cont = NULL;
    ITERATE_LIST(cont, cont_i)
      obj_to_game(cont);
    deleteListIterator(cont_i);
  }
}

void room_to_game(ROOM_DATA *room) {
  listPut(room_list, room);
  propertyTablePut(room_table, room);

  // execute all of our to_game hooks
  hookRun("room_to_game", room, NULL, NULL);

  // add contents
  if(listSize(roomGetContents(room)) > 0) {
    LIST_ITERATOR *cont_i = newListIterator(roomGetContents(room));
    OBJ_DATA        *cont = NULL;
    ITERATE_LIST(cont, cont_i)
      obj_to_game(cont);
    deleteListIterator(cont_i);
  }

  // add its people
  if(listSize(roomGetCharacters(room)) > 0) {
    LIST_ITERATOR *ch_i = newListIterator(roomGetCharacters(room));
    CHAR_DATA       *ch = NULL;
    ITERATE_LIST(ch, ch_i)
      char_to_game(ch);
    deleteListIterator(ch_i);
  }

  // add its exits, and their room table commands as neccessary
  LIST       *ex_list = roomGetExitNames(room);
  LIST_ITERATOR *ex_i = newListIterator(ex_list);
  char           *dir = NULL;
  ITERATE_LIST(dir, ex_i) {
    exit_to_game(roomGetExit(room, dir));
    if(dirGetNum(dir) == DIR_NONE)
      nearMapPut(roomGetCmdTable(room), dir, NULL,
		 newCmd(dir, cmd_move, POS_STANDING, POS_FLYING,
			"player", TRUE, TRUE));
  } deleteListIterator(ex_i);
  deleteListWith(ex_list, free);
}

void char_to_game(CHAR_DATA *ch) {
  listPut(mobile_list, ch);
  propertyTablePut(mob_table, ch);

  // execute all of our to_game hooks
  hookRun("char_to_game", ch, NULL, NULL);

  // also add inventory
  if(listSize(charGetInventory(ch)) > 0) {
    LIST_ITERATOR *inv_i = newListIterator(charGetInventory(ch));
    OBJ_DATA *obj = NULL;
    ITERATE_LIST(obj, inv_i)
      obj_to_game(obj);
    deleteListIterator(inv_i);
  }

  // and equipped items
  LIST *eq = bodyGetAllEq(charGetBody(ch));
  if(listSize(eq) > 0) {
    LIST_ITERATOR *eq_i = newListIterator(eq);
    OBJ_DATA *obj = NULL;
    ITERATE_LIST(obj, eq_i)
      obj_to_game(obj);
    deleteListIterator(eq_i);
  }
  deleteList(eq);
}

void exit_from_game(EXIT_DATA *exit) {
  propertyTableRemove(exit_table, exitGetUID(exit));
}

void obj_from_game(OBJ_DATA *obj) {
  listRemove(object_list, obj);
  propertyTableRemove(obj_table, objGetUID(obj));

  // go through all of our fromgame hooks
  hookRun("obj_from_game", obj, NULL, NULL);

  // also remove everything that is contained within the object
  if(listSize(objGetContents(obj)) > 0) {
    LIST_ITERATOR *cont_i = newListIterator(objGetContents(obj));
    OBJ_DATA *cont = NULL;
    ITERATE_LIST(cont, cont_i)
      obj_from_game(cont);
    deleteListIterator(cont_i);
  }
}

void room_from_game(ROOM_DATA *room) {
  listRemove(room_list, room);
  propertyTableRemove(room_table, roomGetUID(room));

  // go through all of our fromgame hooks
  hookRun("room_from_game", room, NULL, NULL);

  // also remove all the objects contained within the room
  if(listSize(roomGetContents(room)) > 0) {
    LIST_ITERATOR *cont_i = newListIterator(roomGetContents(room));
    OBJ_DATA        *cont = NULL;
    ITERATE_LIST(cont, cont_i)
      obj_from_game(cont);
    deleteListIterator(cont_i);
  }

  // and now all of the characters
  if(listSize(roomGetCharacters(room)) > 0) {
    LIST_ITERATOR *ch_i = newListIterator(roomGetCharacters(room));
    CHAR_DATA       *ch = NULL;
    ITERATE_LIST(ch, ch_i)
      char_from_game(ch);
    deleteListIterator(ch_i);
  }

  // remove its exits
  LIST       *ex_list = roomGetExitNames(room);
  LIST_ITERATOR *ex_i = newListIterator(ex_list);
  char           *dir = NULL;
  ITERATE_LIST(dir, ex_i)
    exit_from_game(roomGetExit(room, dir));
  deleteListIterator(ex_i);
  deleteListWith(ex_list, free);
}

void char_from_game(CHAR_DATA *ch) {
  listRemove(mobile_list, ch);
  propertyTableRemove(mob_table, charGetUID(ch));

  // go through all of our fromgame hooks
  hookRun("char_from_game", ch, NULL, NULL);

  // also remove inventory
  if(listSize(charGetInventory(ch)) > 0) {
    LIST_ITERATOR *inv_i = newListIterator(charGetInventory(ch));
    OBJ_DATA *obj = NULL;
    ITERATE_LIST(obj, inv_i)
      obj_from_game(obj);
    deleteListIterator(inv_i);
  }

  // and equipped items
  LIST *eq = bodyGetAllEq(charGetBody(ch));
  if(listSize(eq) > 0) {
    LIST_ITERATOR *eq_i = newListIterator(eq);
    OBJ_DATA *obj = NULL;
    ITERATE_LIST(obj, eq_i)
      obj_from_game(obj);
    deleteListIterator(eq_i);
  }
  deleteList(eq);
}

void obj_from_char(OBJ_DATA *obj) {
  if(objGetCarrier(obj)) {
    listRemove(charGetInventory(objGetCarrier(obj)), obj);
    objSetCarrier(obj, NULL);
  }
}

void obj_from_obj(OBJ_DATA *obj) {
  if(objGetContainer(obj)) {
    listRemove(objGetContents(objGetContainer(obj)), obj);
    objSetContainer(obj, NULL);
  }
}

void obj_from_room(OBJ_DATA *obj) {
  if(objGetRoom(obj)) {
    listRemove(roomGetContents(objGetRoom(obj)), obj);
    objSetRoom(obj, NULL);
  }
}

void obj_to_char(OBJ_DATA *obj, CHAR_DATA *ch) {
  listPut(charGetInventory(ch), obj);
  objSetCarrier(obj, ch);
}

void obj_to_obj(OBJ_DATA *obj, OBJ_DATA *to) {
  listPut(objGetContents(to), obj);
  objSetContainer(obj, to);
}

void obj_to_room(OBJ_DATA *obj, ROOM_DATA *room) {
  listPut(roomGetContents(room), obj);
  objSetRoom(obj, room);
}

void char_from_room(CHAR_DATA *ch) {
  charSetLastRoom(ch, charGetRoom(ch));
  roomRemoveChar(charGetRoom(ch), ch);
  charSetRoom(ch, NULL);
}

void char_to_room(CHAR_DATA *ch, ROOM_DATA *room) {
  if(charGetRoom(ch))
    char_from_room(ch);

  roomAddChar(room, ch);
  charSetRoom(ch, room);
}

void char_from_furniture(CHAR_DATA *ch) {
  objRemoveChar(charGetFurniture(ch), ch);
  charSetFurniture(ch, NULL);
}

void char_to_furniture(CHAR_DATA *ch, OBJ_DATA *furniture) {
  if(charGetFurniture(ch))
    char_from_furniture(ch);

  objAddChar(furniture, ch);
  charSetFurniture(ch, furniture);
}



//*****************************************************************************
// do_get/give/drop/etc...
//*****************************************************************************
void do_get(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container) {
  if(bitIsOneSet(objGetBits(obj), "notake"))
    send_to_char(ch, "You cannot take %s.\r\n", objGetName(obj));
  else if(container) {
    send_to_char(ch, "You get %s from %s.\r\n", 
		 objGetName(obj), objGetName(container));
    message(ch, NULL, obj, container, TRUE, TO_ROOM,
	    "$n gets $o from $O.");
    obj_from_obj(obj);
    obj_to_char(obj, ch);
  }
  else {
    send_to_char(ch, "You get %s.\r\n", objGetName(obj));
    message(ch, NULL, obj, NULL, TRUE, TO_ROOM,
	    "$n gets $o.");
    obj_from_room(obj);
    obj_to_char(obj, ch);
    hookRun("get", ch, obj, NULL);
  }
}

void do_put(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container) {
  if(containerIsClosed(container))
    send_to_char(ch, "%s is closed. Open it first.\r\n", 
		 see_obj_as(ch, container));
  else if(obj == container)
    send_to_char(ch, "You cannot put %s into itself.\r\n", objGetName(obj));
  // make sure we have enough room
  else if(objGetWeight(obj) > 
	  (containerGetCapacity(container) - 
	   objGetWeight(container) + objGetWeightRaw(container)))
    send_to_char(ch, "There is not enough room in %s for %s.\r\n", 
		 see_obj_as(ch, container), see_obj_as(ch, obj));
  // do the move
  else {
    obj_from_char(obj);
    obj_to_obj(obj, container);
    send_to_char(ch, "You put %s into %s.\r\n", 
		 see_obj_as(ch, obj), see_obj_as(ch, container));
    message(ch, NULL, obj, container, TRUE, TO_ROOM,
	    "$n puts $o into $O.");
  }
}


void do_give(CHAR_DATA *ch, CHAR_DATA *recv, OBJ_DATA *obj) {
  message(ch, recv, obj, NULL, TRUE, TO_ROOM, "$n gives $o to $N.");
  message(ch, recv, obj, NULL, TRUE, TO_VICT, "$n gives $o to you.");
  message(ch, recv, obj, NULL, TRUE, TO_CHAR, "You give $o to $N.");
  obj_from_char(obj);
  obj_to_char(obj, recv);

  // run all of our give/receive hooks
  hookRun("give", ch, recv, obj);
}


void do_drop(CHAR_DATA *ch, OBJ_DATA *obj) {
  send_to_char(ch, "You drop %s.\r\n", objGetName(obj));
  message(ch, NULL, obj, NULL, TRUE, TO_ROOM,
	  "$n drops $o.");
  obj_from_char(obj);
  obj_to_room(obj, charGetRoom(ch));

  // run all of our drop hooks
  hookRun("drop", ch, obj, NULL);
}


void do_wear(CHAR_DATA *ch, OBJ_DATA *obj, const char *where) {
  if(!objIsType(obj, "worn"))
    send_to_char(ch, "You cannot wear %s!\r\n", objGetName(obj));
  else {
    obj_from_char(obj);
    if(try_equip(ch, obj, where))
      send_to_char(ch, "You equip %s.\r\n", objGetName(obj));
    else {
      send_to_char(ch, "You could not equip %s.\r\n", objGetName(obj));
      obj_to_char(obj, ch);
    }
  }
}


void do_remove(CHAR_DATA *ch, OBJ_DATA *obj) {
  if(try_unequip(ch, obj)) {
    send_to_char(ch, "You remove %s.\r\n", objGetName(obj));
    obj_to_char(obj, ch);
  }
  else
    send_to_char(ch, "You were unable to remove %s.\r\n", objGetName(obj));
}



//*****************************************************************************
// functions related to equipping and unequipping items
//*****************************************************************************
bool try_equip(CHAR_DATA *ch, OBJ_DATA *obj, const char *poslist) {
  if(!objIsType(obj, "worn"))
    return FALSE;

  bool success       = FALSE;
  char *wanted       = NULL;
  const char *needed = NULL;

  // get a list of the position types we need to equip to

  // see where we _want_ to equip to
  if(poslist && *poslist)
    wanted = list_postypes(charGetBody(ch), poslist);
  needed = wornGetPositions(obj);

  // just equip to the first free slots
  if(!wanted)
    success = bodyEquipPostypes(charGetBody(ch), obj, needed);
  // check to see if all the positions we want to equip to match up with
  // what we need to equip to
  else {
    // build lists of what we want and what we need, and compare
    int i, j, num_want = 0, num_need = 0;
    char **want_names, **need_names;
    bool match = FALSE;
    want_names = parse_keywords(wanted, &num_want);
    need_names = parse_keywords(needed, &num_need); 

    // try to match it all up
    if(num_want == num_need) {
      // assume true, and try to find two that mismatch
      match = TRUE;
      for(i = 0; i < num_want; i++) {
	bool found = FALSE;
	for(j = 0; j < num_need; j++) {
	  if(need_names[j] && !strcmp(want_names[i], need_names[j])) {
	    // use one up
	    free(need_names[j]); 
	    need_names[j] = NULL;
	    found = TRUE;
	    break;
	  }
	}
	if(!found) {
	  match = FALSE;
	  break;
	}
      }
    }

    // try to equip
    if(match)
      success = bodyEquipPosnames(charGetBody(ch), obj, poslist);

    // free everything up
    for(i = 0; i < num_want || i < num_need; i++) {
      if(want_names[i]) free(want_names[i]);
      if(need_names[i]) free(need_names[i]);
    }
    if(want_names) free(want_names);
    if(need_names) free(need_names);
  }

  if(wanted) free(wanted);
  if(success) {
    objSetWearer(obj, ch);
    hookRun("wear", ch, obj, NULL);
  }
  return success;
}

bool try_unequip(CHAR_DATA *ch, OBJ_DATA *obj) {
  if(bodyUnequip(charGetBody(ch), obj)) {
    objSetWearer(obj, NULL);
    hookRun("remove", ch, obj, NULL);
    return TRUE;
  }
  return FALSE;
}

//
// unequip everything the character is wearing, and put it to his or her inv
//
void unequip_all(CHAR_DATA *ch) {
  LIST      *eq = bodyGetAllEq(charGetBody(ch));
  OBJ_DATA *obj = NULL;
  while( (obj = listPop(eq)) != NULL) {
    if(bodyUnequip(charGetBody(ch), obj)) {
      objSetWearer(obj, NULL);
      hookRun("remove", ch, obj, NULL);
      obj_to_char(obj, ch);
    }
  } deleteList(eq);
}



//*****************************************************************************
//
// Functions related to generic_find() and find_specific()
//
//*****************************************************************************

//
// finds the first argument after "arg". return NULL if nothing is found.
// the returned string must be freed afterwards
//
char *after_arg(char *target, char *arg) {
  int len   = strlen(arg);
  int t_len = strlen(target);
  int i = 0;

  // loop through and find our target
  for(i = 0; i < len - t_len - 1; i++) {
    // we've found the target, and there's a space after it
    if(!strncasecmp(arg+i, target, t_len) && isspace(arg[i+t_len])) {
      // skip up to past the next space(s)
      i = i + t_len + 1;
      while(isspace(arg[i])) 
	i++;

      // find where the next space or string terminator is
      int j = i+1;
      while(!(isspace(arg[j]) || arg[j] == '\0'))
	j++;

      // make a buffer to hold the word following our target
      char buf[j-i+1];
      strncpy(buf, arg+i, j-i);
      buf[j-i] = '\0';
      return strdup(buf);
    }
  }

  return NULL;
}


//
// finds the first argument after "at". return NULL if nothing is found.
// the returned string must be freed afterwards
//
char *at_arg(char *arg) {
  return after_arg("at", arg);
}


//
// finds the first argument after "on". return NULL if nothing is found.
// the returned string must be freed afterwards
//
char *on_arg(char *arg) {
  return after_arg("on", arg);
}


//
// finds the first argument after "in". return NULL if nothing is found.
// the returned string must be freed afterwards
//
char *in_arg(char *arg) {
  return after_arg("in", arg);
}


//
// Can find: objects and extra descriptions
//
void *find_on_char(CHAR_DATA *looker,
		   CHAR_DATA *on,
		   int at_count, const char *at,
		   bitvector_t find_types,
		   bitvector_t find_scope,
		   int *found_type) {
  int count = 0;

  // see if it's equipment
  if(IS_SET(find_types, FIND_TYPE_OBJ)) {
    LIST *equipment = bodyGetAllEq(charGetBody(on));
    count += count_objs(looker, equipment, at, NULL,
			(IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      if(found_type)
	*found_type = FOUND_OBJ;
      OBJ_DATA *obj = find_obj(looker, equipment, at_count, at, NULL, 
			       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
      deleteList(equipment);
      return obj;
    }
    else
      deleteList(equipment);
  }

  // see if it's an extra description
  //***********
  // FINISH ME
  //***********

  if(found_type)
    *found_type = FOUND_NONE;
  return NULL;
}


//
// Can find: extra descriptions, chars
//
void *find_on_obj(CHAR_DATA *looker,
		  OBJ_DATA  *on,
		  int at_count, const char *at,
		  bitvector_t find_types,
		  bitvector_t find_scope,
		  int *found_type) {
  int count = 0;

  // see if it's a character
  if(IS_SET(find_types, FIND_TYPE_CHAR)) {
    count = count_chars(looker, objGetUsers(on), at, NULL,
			(IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      if(found_type)
	*found_type = FOUND_CHAR;
      return find_char(looker, objGetUsers(on), at_count, at, NULL,
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
      at_count -= count;
  }

  // see if it's an extra description
  if(IS_SET(find_types, FIND_TYPE_EDESC)) {
    count = (objGetEdesc(on, at) != NULL);
    if(count && at_count == 1) {
      if(found_type)
	*found_type = FOUND_EDESC;
      return edescSetGet(objGetEdescs(on), at);
    }
    else
      at_count--;
  }
 
  if(found_type)
    *found_type = FOUND_NONE;
  return NULL;
}


//
// Can find: objects and extra descriptions
//
void *find_in_obj(CHAR_DATA *looker,
		  OBJ_DATA *in,
		  int at_count, const char *at,
		  int on_count, const char *on,
		  bitvector_t find_types,
		  bitvector_t find_scope,
		  int *found_type) {		  
  if(found_type)
    *found_type = FOUND_NONE;

  // see if we're looking on anything
  if(on && *on && on_count > 0) {
    OBJ_DATA *on_obj = find_obj(looker, objGetContents(in), on_count, on, NULL,
				(IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(!on_obj)
      return NULL;
    else
      return find_on_obj(looker, on_obj, at_count, at, 
			 find_types, find_scope, found_type);
  }
  else {
    int count = count_objs(looker, objGetContents(in), at, NULL,
			    (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      if(found_type)
	*found_type = FOUND_OBJ;
      return find_obj(looker, objGetContents(in), at_count, at, NULL,
		      (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
      return NULL;
  }
}


LIST *find_all(CHAR_DATA *looker, const char *at, bitvector_t find_types,
	       bitvector_t find_scope, int *found_type) {
  if(found_type)
    *found_type = FOUND_LIST;

  /************************************************************/
  /*                        FIND ALL OBJS                     */
  /************************************************************/
  if(find_types == FIND_TYPE_OBJ) {
    LIST *obj_list = newList();
    
    // get everything from our inventory
    if(IS_SET(find_scope, FIND_SCOPE_INV)) {
      LIST *inv_objs = find_all_objs(looker,charGetInventory(looker), at, NULL,
				     (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
      OBJ_DATA *obj = NULL;
      while( (obj = listPop(inv_objs)) != NULL)
	if(!listIn(obj_list, obj))
	  listPut(obj_list, obj);
      deleteList(inv_objs);
    }

    // get everything from the room
    if(IS_SET(find_scope, FIND_SCOPE_ROOM)) {
      LIST *room_objs = find_all_objs(looker, 
				      roomGetContents(charGetRoom(looker)),
				      at, NULL,
				     (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
      OBJ_DATA *obj = NULL;
      while( (obj = listPop(room_objs)) != NULL)
	if(!listIn(obj_list, obj))
	  listPut(obj_list, obj);
      deleteList(room_objs);
    }

    // get everything we are wearing
    if(IS_SET(find_scope, FIND_SCOPE_WORN)) {
      // we have to get a list of all eq as an intermediary step, and then
      // delete the list after we search through it again for everything
      // that we can see.
      LIST *equipment = bodyGetAllEq(charGetBody(looker));
      LIST *eq_objs = find_all_objs(looker, equipment, at, NULL,
				    (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
      deleteList(equipment);
      OBJ_DATA *obj = NULL;
      while( (obj = listPop(eq_objs)) != NULL)
	if(!listIn(obj_list, obj))
	  listPut(obj_list, obj);
      deleteList(eq_objs);
    }

    // get everything in the world
    if(IS_SET(find_scope, FIND_SCOPE_WORLD)) {
      LIST *wld_objs = find_all_objs(looker,
				     object_list, at, NULL, 
				     (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
      OBJ_DATA *obj = NULL;
      while( (obj = listPop(wld_objs)) != NULL)
	if(!listIn(obj_list, obj))
	  listPut(obj_list, obj);
      deleteList(wld_objs);
    }

    // if we didn't find anything, return NULL
    if(listSize(obj_list) < 1) {
      deleteList(obj_list);
      if(found_type)
	*found_type = FOUND_NONE;
      return NULL;
    }
    else
      return obj_list;
  }

  
  /************************************************************/
  /*                        FIND ALL CHARS                    */
  /************************************************************/
  else if(find_types == FIND_TYPE_CHAR) {
    LIST *char_list = newList();

    // find everyone in the room
    if(IS_SET(find_scope, FIND_SCOPE_ROOM)) {
      LIST *room_chars =find_all_chars(looker, 
				       roomGetCharacters(charGetRoom(looker)),
				       at, NULL,
				       (IS_SET(find_scope,FIND_SCOPE_VISIBLE)));
      CHAR_DATA *ch = NULL;
      while( (ch = listPop(room_chars)) != NULL)
	if(!listIn(char_list, ch))
	  listPut(char_list, ch);
      deleteList(room_chars);
    }

    // find everyone in the world
    if(IS_SET(find_scope, FIND_SCOPE_WORLD)) {
      LIST *wld_chars = find_all_chars(looker, 
				       mobile_list,
				       at, NULL,
				       (IS_SET(find_scope,FIND_SCOPE_VISIBLE)));
      CHAR_DATA *ch = NULL;
      while( (ch = listPop(wld_chars)) != NULL)
	if(!listIn(char_list, ch))
	  listPut(char_list, ch);
      deleteList(wld_chars);
    }

    // if we didn't find anything, return NULL
    if(listSize(char_list) < 1) {
      deleteList(char_list);
      if(found_type)
	*found_type = FOUND_NONE;
      return NULL;
    }
    else
      return char_list;
  }
  

  /************************************************************/
  /*                       FIND ALL EDESCS                    */
  /************************************************************/
  else {
    if(found_type)
      *found_type = FOUND_NONE;
    return NULL;
  }
}


void *find_one(CHAR_DATA *looker, 
	       int at_count, const char *at, 
	       bitvector_t find_types,
	       bitvector_t find_scope, int *found_type) {

  // find what we're looking AT
  int count = 0;

  /************************************************************/
  /*                   PERSONAL SEARCHES                      */
  /************************************************************/
  // see if its ourself
  if(IS_SET(find_types, FIND_TYPE_CHAR) &&
     at_count >= 1 && !strcasecmp(at, "self")) {
    at_count--;
    if(at_count == 0) {
      if(found_type)
	*found_type = FOUND_CHAR;
      return looker;
    }
  }

  // seach our inventory
  if(IS_SET(find_scope, FIND_SCOPE_INV) && 
     IS_SET(find_types, FIND_TYPE_OBJ)) {
    count = count_objs(looker, charGetInventory(looker), at, NULL, 
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      if(found_type)
	*found_type = FOUND_OBJ;
      return find_obj(looker, charGetInventory(looker), at_count, at, NULL,
		      (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
      at_count -= count;
  }
  
  // search our equipment
  if(IS_SET(find_scope, FIND_SCOPE_WORN) &&
     IS_SET(find_types, FIND_TYPE_OBJ)) {
    LIST *equipment = bodyGetAllEq(charGetBody(looker));
    count = count_objs(looker, equipment, at, NULL, 
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      if(found_type)
	*found_type = FOUND_OBJ;
      OBJ_DATA *obj = find_obj(looker, equipment, at_count, at, NULL, 
			       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
      deleteList(equipment);
      return obj;
    }
    else {
      deleteList(equipment);
      at_count -= count;
    }
  }


  /************************************************************/
  /*                      LOCAL SEARCHES                      */
  /************************************************************/
  // is it our current room?
  if(IS_SET(find_scope, FIND_SCOPE_ROOM | FIND_SCOPE_WORLD) &&
     IS_SET(find_types, FIND_TYPE_ROOM)  &&
     at_count == 1 && !strcasecmp(at, "room")) {
    if(found_type)
      *found_type = FOUND_ROOM;
    return charGetRoom(looker);
  }

  // search objects in the room
  if(IS_SET(find_scope, FIND_SCOPE_ROOM) && 
     IS_SET(find_types, FIND_TYPE_OBJ)) {
    count = count_objs(looker, roomGetContents(charGetRoom(looker)), at, NULL,
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      if(found_type)
	*found_type = FOUND_OBJ;
      return find_obj(looker, roomGetContents(charGetRoom(looker)), at_count,
		      at, NULL, (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
      at_count -= count;
  }

  // seach the characters in the room
  if(IS_SET(find_scope, FIND_SCOPE_ROOM) &&
     IS_SET(find_types, FIND_TYPE_CHAR)) {
    count = count_chars(looker, roomGetCharacters(charGetRoom(looker)), at,
		       NULL, (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      if(found_type)
	*found_type = FOUND_CHAR;
      return find_char(looker, roomGetCharacters(charGetRoom(looker)), 
		       at_count, at, NULL,
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
	at_count -= count;
  }

  // search for exits in the room
  if(IS_SET(find_scope, FIND_SCOPE_ROOM) &&
     IS_SET(find_types, FIND_TYPE_EXIT)) {
    EXIT_DATA *exit = roomGetExit(charGetRoom(looker), at);

    // no exit... are we using an abbreviation?
    if(exit == NULL && (dirGetAbbrevNum(at) != DIR_NONE))
      exit = roomGetExit(charGetRoom(looker), dirGetName(dirGetAbbrevNum(at)));

    // we found one
    if(exit && (!IS_SET(find_scope, FIND_SCOPE_VISIBLE) || 
		can_see_exit(looker, exit))) {
      at_count--;
      if(at_count == 0) {
	if(found_type)
	  *found_type = FOUND_EXIT;
	return exit;
      }
    }

    LIST       *ex_list = roomGetExitNames(charGetRoom(looker));
    LIST_ITERATOR *ex_i = newListIterator(ex_list);
    char           *dir = NULL;

    ITERATE_LIST(dir, ex_i) {
      exit = roomGetExit(charGetRoom(looker), dir);
      if(exitIsName(exit, at)) {
	if(!IS_SET(find_scope,FIND_SCOPE_VISIBLE) || can_see_exit(looker,exit)){
	  at_count--;
	  if(at_count == 0) {
	    if(found_type)
	      *found_type = FOUND_EXIT;
	    break;
	  }
	}
      }
    } deleteListIterator(ex_i);
    deleteListWith(ex_list, free);

    // we found one
    if(*found_type != FOUND_NONE)
      return exit;
  }

  // search extra descriptions in the room
  if(IS_SET(find_scope, FIND_SCOPE_ROOM) &&
     IS_SET(find_types, FIND_TYPE_EDESC)) {
    count = (roomGetEdesc(charGetRoom(looker), at) != NULL);
    if(count && at_count == 1) {
      if(found_type)
	*found_type = FOUND_EDESC;
      return edescSetGet(roomGetEdescs(charGetRoom(looker)), at);
    }
    else
	at_count--;
  }

  /************************************************************/
  /*                     GLOBAL SEARCHES                      */
  /************************************************************/
  // search rooms in the world
  if(IS_SET(find_scope, FIND_SCOPE_WORLD) &&
     IS_SET(find_types, FIND_TYPE_ROOM)   &&
     at_count == 1) {
    ROOM_DATA *room = worldGetRoom(gameworld, 
	                get_fullkey_relative(at, 
			  get_key_locale(roomGetClass(charGetRoom(looker)))));

    if(room != NULL) {
      if(found_type)
	*found_type = FOUND_ROOM;
      return room;
    }
  }

  // search objects in the world
  if(IS_SET(find_scope, FIND_SCOPE_WORLD) &&
     IS_SET(find_types, FIND_TYPE_OBJ)) {
    count = count_objs(looker, object_list, at, NULL, 
			 (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      if(found_type)
	*found_type = FOUND_OBJ;
      return find_obj(looker, object_list, at_count, at, NULL, 
		      (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
	at_count -= count;
  }

  // search characters in the world
  if(IS_SET(find_scope, FIND_SCOPE_WORLD) &&
     IS_SET(find_types, FIND_TYPE_CHAR)) {
    count = count_chars(looker, mobile_list, at, NULL, 
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      if(found_type)
	*found_type = FOUND_CHAR;
      return find_char(looker, mobile_list, at_count, at, NULL,
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
	at_count -= count;
  }

  // we didn't find anything!
  if(found_type)
    *found_type = FOUND_NONE;
  return NULL;
}


void *generic_find(CHAR_DATA *looker, const char *arg,
		   bitvector_t find_types, 
		   bitvector_t find_scope, 
		   bool all_ok, int *found_type) {
  // make a working buffer...
  char working_arg[SMALL_BUFFER];
  strcpy(working_arg, arg);

  strip_word(working_arg, "the");

  char *at = at_arg(working_arg);
  char *in = in_arg(working_arg);
  char *on = on_arg(working_arg);

  strip_word(working_arg, "at");
  strip_word(working_arg, "in");
  strip_word(working_arg, "on");
  trim(working_arg);

  // if we don't have an "at", and there's a word we haven't pulled,
  // pull the first word before a space
  if(!at &&
     // figure out the difference between the
     // number of words that exist and the number
     // of words we've pulled. If it's > 0, we 
     // haven't pulled a word
     (count_letters(working_arg, ' ', strlen(working_arg)) + 1 - 
      (in != NULL) - (on != NULL)) == 1) {
    at = strtok(working_arg, " ");
    at = strdupsafe(at);
  }

  // make sure at, in, and on are never NULL
  // we always want to search by name, and never vnum
  if(!at) at = strdup("");
  if(!on) on = strdup("");
  if(!in) in = strdup("");

  // do the finding
  void *val = find_specific(looker, 
			    at, on, in,
			    find_types, find_scope, 
			    all_ok, found_type);

  if(at) free(at);
  if(in) free(in);
  if(on) free(on);

  return val;
}


//
// At is what we're looking for
// On is the thing we're looking for it on (e.g. "on sword" "on joe")
// In is the thing we're looking for it in (e.g. "in hole" "in bag")
//
void *find_specific(CHAR_DATA *looker,
		    const char *full_at, 
		    const char *full_on, 
		    const char *full_in,
		    bitvector_t find_types,
		    bitvector_t find_scope,
		    bool all_ok, int *found_type) {
  // for stuff like 2.sword, all.woman, etc...
  int at_count = 1;
  int on_count = 1;
  int in_count = 1;

  // the buffers for storing at, in, on separate from their counts
  char at[SMALL_BUFFER] = "";
  char in[SMALL_BUFFER] = "";
  char on[SMALL_BUFFER] = "";

  // separate the names from their numbers
  get_count(full_at, at, &at_count);
  get_count(full_in, in, &in_count);
  get_count(full_on, on, &on_count);

  if(found_type)
    *found_type = FOUND_NONE;


  // are we trying to find all of something?
  if(all_ok && at_count == COUNT_ALL && !*on && !*in)
    return find_all(looker, at, find_types, find_scope, found_type);

  else if(!*at || at_count == 0) {
    // we're trying to find what the contents of an item are?
    // e.g. "look in portal", "look in bag"
    if(*in && in_count >= 1 &&
       IS_SET(find_types, FIND_TYPE_IN_OBJ)) {
      void *tgt = NULL;
      int next_found_type = FOUND_NONE; // for finding in/on stuff
      char new_at[strlen(in) + 20]; // +20 for digits
      print_count(new_at, in, in_count);
      tgt = find_specific(looker, 
			  new_at, "", "",
			  find_types, find_scope,
			  all_ok, &next_found_type);

      // we couldn't find the thing we were trying to look inside
      if(!tgt)
	return NULL;
      // if we got a list of objects back, scrap 'em
      else if(next_found_type == FOUND_LIST) {
	deleteList(tgt);
	return NULL;
      }
      // we can only look inside of objects
      else if(next_found_type != FOUND_OBJ)
	return NULL;
      // return what we found
      else {
	if(found_type)
	  *found_type = FOUND_IN_OBJ;
	return tgt;
      }
    }
    // we're just not looking for anything
    else {
      if(found_type)
	*found_type = FOUND_NONE;
      return NULL;
    }
  }

  else {
    void *tgt  = NULL; // used for finding what we're looking in/on
    /****************************************************/
    /*                   START LOOK IN                  */
    /****************************************************/
    // check out what we're looking in
    if(*in && in_count > 0) {
      int next_found_type = FOUND_NONE; // for finding in/on stuff
      char new_at[strlen(in) + 20]; // +20 for digits
      print_count(new_at, in, in_count);
      tgt = find_specific(looker, 
			  new_at, "", "",
			  find_types, find_scope,
			  all_ok, &next_found_type);
      // we couldn't find the thing we were trying to look inside
      if(!tgt)
	return NULL;
      // we have to KILL someone before we can look inside of them ;)
      else if(next_found_type != FOUND_OBJ)
	return NULL;
      // apply another find, narrowing the scope to inside this object
      else
	return find_in_obj(looker, tgt, 
			   at_count, at, 
			   on_count, on,
			   find_types, find_scope, found_type);
    }


    /****************************************************/
    /*                   START LOOK ON                  */
    /****************************************************/
    // find out what we're looking on
    else if(*on && on_count > 0) {
      int next_found_type = FOUND_NONE;
      char new_at[strlen(on) + 20]; // +20 for digits
      print_count(new_at, on, on_count);
      tgt = find_specific(looker, 
			  on, "", "",
			  find_types, find_scope,
			  all_ok, &next_found_type);
      // couldn't find what we were trying to look on
      if(tgt == NULL)
	return NULL;
      else if(next_found_type == FOUND_CHAR)
	return find_on_char(looker, tgt, at_count, at, 
			    find_types, find_scope, found_type);
      else if(next_found_type == FOUND_OBJ) {
	return find_on_obj(looker, tgt, at_count, at, find_types, 
			   find_scope, found_type);
      }
      else
	return NULL;
    }


    /****************************************************/
    /*                   START LOOK AT                  */
    /****************************************************/
    return find_one(looker, at_count, at, find_types, find_scope, found_type);
  }
}
