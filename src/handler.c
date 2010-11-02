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
#include "handler.h"
#include "room.h"
#include "exit.h"
#include "extra_descs.h"
#include "character.h"
#include "object.h"
#include "utils.h"
#include "body.h"
#include "items.h"
#include "inform.h"

// optional modules
#ifdef MODULE_SCRIPTS
#include "modules/scripts/script.h"
#endif


//*****************************************************************************
//
// obj/char from/to functions
//
//*****************************************************************************
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

void obj_to_game(OBJ_DATA *obj) {
  listPut(object_list, obj);
  propertyTablePut(obj_table, obj);

  // also add all contents
  if(listSize(objGetContents(obj)) > 0) {
    LIST_ITERATOR *cont_i = newListIterator(objGetContents(obj));
    OBJ_DATA *cont = NULL;
    ITERATE_LIST(cont, cont_i)
      obj_to_game(cont);
    deleteListIterator(cont_i);
  }
}

void obj_from_game(OBJ_DATA *obj) {
  listRemove(object_list, obj);
  propertyTableRemove(obj_table, objGetUID(obj));

  // also remove everything that is contained within the object
  if(listSize(objGetContents(obj)) > 0) {
    LIST_ITERATOR *cont_i = newListIterator(objGetContents(obj));
    OBJ_DATA *cont = NULL;
    ITERATE_LIST(cont, cont_i)
      obj_from_game(cont);
    deleteListIterator(cont_i);
  }
}

void char_to_game(CHAR_DATA *ch) {
  listPut(mobile_list, ch);
  propertyTablePut(mob_table, ch);

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

void char_from_game(CHAR_DATA *ch) {
  listRemove(mobile_list, ch);
  propertyTableRemove(mob_table, charGetUID(ch));

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

void char_from_room(CHAR_DATA *ch) {
  roomRemoveChar(charGetRoom(ch), ch);
  charSetRoom(ch, NULL);
};

void char_to_room(CHAR_DATA *ch, ROOM_DATA *room) {
  if(charGetRoom(ch))
    char_from_room(ch);

  roomAddChar(room, ch);
  charSetRoom(ch, room);
};

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
//
// do_get/give/drop/etc...
//
//*****************************************************************************
void do_get(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container) {
  if(objIsBitSet(obj, BITFIELD_OBJ, OBJ_NOTAKE))
    send_to_char(ch, "You cannot take %s.\r\n", objGetName(obj));
  else if(container) {
    send_to_char(ch, "You get %s from %s.\r\n", 
		 objGetName(obj), objGetName(container));
    message(ch, NULL, obj, container, TRUE, TO_ROOM | TO_NOTCHAR,
	    "$n gets $o from $O.");
    obj_from_obj(obj);
    obj_to_char(obj, ch);
  }
  else {
    send_to_char(ch, "You get %s.\r\n", objGetName(obj));
    message(ch, NULL, obj, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	    "$n gets $o.");
    obj_from_room(obj);
    obj_to_char(obj, ch);
  }
}

void do_put(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container) {
  if(containerIsClosed(container))
    send_to_char(ch, "%s is closed. Open it first.\r\n", objGetName(container));
  else if(obj == container)
    send_to_char(ch, "You cannot put %s into itself.\r\n", objGetName(obj));
  // make sure we have enough room
  else if(objGetWeight(obj) > 
	  (objGetCapacity(container) - 
	   objGetWeight(container) + objGetWeightRaw(container)))
    send_to_char(ch, "There is not enough room in %s for %s.\r\n", 
		 objGetName(container), objGetName(obj));
  // do the move
  else {
    obj_from_char(obj);
    obj_to_obj(obj, container);
    send_to_char(ch, "You put %s into %s.\r\n", 
		 objGetName(obj), objGetName(container));
    message(ch, NULL, obj, container, TRUE, TO_ROOM | TO_NOTCHAR,
	    "$n puts $o into $O.");
  }
}


void do_give(CHAR_DATA *ch, CHAR_DATA *recv, OBJ_DATA *obj) {
  message(ch, recv, obj, NULL, TRUE, TO_ROOM | TO_NOTVICT | TO_NOTCHAR,
	  "$n gives $o to $N.");
  message(ch, recv, obj, NULL, TRUE, TO_VICT,
	  "$n gives $o to you.");
  message(ch, recv, obj, NULL, TRUE, TO_CHAR,
	  "You give $o to $N.");
  obj_from_char(obj);
  obj_to_char(obj, recv);

#ifdef MODULE_SCRIPTS  
  // object give
  try_scripts(SCRIPT_TYPE_GIVE,
	      obj, SCRIPTOR_OBJ,
	      ch, obj, charGetRoom(ch), NULL, NULL, NULL, 0);
  
  // char receive
  try_scripts(SCRIPT_TYPE_GIVE,
	      recv, SCRIPTOR_CHAR,
	      ch, obj, charGetRoom(ch), NULL, NULL, NULL, 0);
#endif
}


void do_drop(CHAR_DATA *ch, OBJ_DATA *obj) {
  send_to_char(ch, "You drop %s.\r\n", objGetName(obj));
  obj_from_char(obj);
  obj_to_room(obj, charGetRoom(ch));

#ifdef MODULE_SCRIPTS  
  // check for triggers
  try_scripts(SCRIPT_TYPE_DROP,
	      charGetRoom(ch), SCRIPTOR_ROOM,
	      ch, obj, charGetRoom(ch), NULL, NULL, NULL, 0);
  try_scripts(SCRIPT_TYPE_DROP,
	      obj, SCRIPTOR_OBJ,
	      ch, obj, charGetRoom(ch), NULL, NULL, NULL, 0);
#endif
}


void do_wear(CHAR_DATA *ch, OBJ_DATA *obj, const char *where) {
  if(objGetType(obj) != ITEM_WORN)
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
//
// functions related to equipping and unequipping items
//
//*****************************************************************************

bool try_equip(CHAR_DATA *ch, OBJ_DATA *obj, const char *poslist) {
  if(objGetType(obj) != ITEM_WORN/* && objGetType(obj) != ITEM_WEAPON*/)
    return FALSE;

  bool success       = FALSE;
  char *wanted       = NULL;
  const char *needed = NULL;

  // get a list of the position types we need to equip to

  // see where we _want_ to equip to
  if(poslist && *poslist)
    wanted = list_postypes(charGetBody(ch), poslist);
  needed = wornGetPositions(objGetSubtype(obj));

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
    //****************************
    // set all of the item affects
    //****************************
  }
  return success;
}


bool try_unequip(CHAR_DATA *ch, OBJ_DATA *obj) {
  if(bodyUnequip(charGetBody(ch), obj)) {
    objSetWearer(obj, NULL);
     //*******************************
    // remove all of the item affects
    //*******************************
    return TRUE;
  }
  return FALSE;
}

//
// unequip everything the character is wearing, and put it to his or her inv
//
void unequip_all(CHAR_DATA *ch) {
  LIST *eq = bodyUnequipAll(charGetBody(ch));
  OBJ_DATA *obj = NULL;
  while( (obj = listPop(eq)) != NULL) {
    objSetWearer(obj, NULL);
    //*******************************
    // remove all of the item affects
    //*******************************
    obj_to_char(obj, ch);
  }
  deleteList(eq);
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
		   int at_count, char *at,
		   bitvector_t find_types,
		   bitvector_t find_scope,
		   int *found_type) {
  int count = 0;

  // see if it's equipment
  if(IS_SET(find_types, FIND_TYPE_OBJ)) {
    LIST *equipment = bodyGetAllEq(charGetBody(on));
    count += count_objs(looker, equipment, at, NOTHING,
			(IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      *found_type = FOUND_OBJ;
      OBJ_DATA *obj = find_obj(looker, equipment, at_count, at, NOTHING, 
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

  *found_type = FOUND_NONE;
  return NULL;
}


//
// Can find: extra descriptions, chars
//
void *find_on_obj(CHAR_DATA *looker,
		  OBJ_DATA  *on,
		  int at_count, char *at,
		  bitvector_t find_types,
		  bitvector_t find_scope,
		  int *found_type) {
  int count = 0;

  // see if it's a character
  if(IS_SET(find_types, FIND_TYPE_CHAR)) {
    count = count_chars(looker, objGetUsers(on), at, NOBODY,
			(IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      *found_type = FOUND_CHAR;
      return find_char(looker, objGetUsers(on), at_count, at, NOBODY,
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
      at_count -= count;
  }

  // see if it's an extra description
  if(IS_SET(find_types, FIND_TYPE_EDESC)) {
    count = (objGetEdesc(on, at) != NULL);
    if(count && at_count == 1) {
      *found_type = FOUND_EDESC;
      return getEdesc(objGetEdescs(on), at);
    }
    else
      at_count--;
  }

  *found_type = FOUND_NONE;
  return NULL;
}


//
// Can find: objects and extra descriptions
//
void *find_in_obj(CHAR_DATA *looker,
		  OBJ_DATA *in,
		  int at_count, char *at,
		  int on_count, char *on,
		  bitvector_t find_types,
		  bitvector_t find_scope,
		  int *found_type) {		  
  *found_type = FOUND_NONE;

  // see if we're looking on anything
  if(on && *on && on_count > 0) {
    OBJ_DATA *on_obj = find_obj(looker, objGetContents(in),on_count,on, NOTHING,
				(IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(!on_obj)
      return NULL;
    else
      return find_on_obj(looker, on_obj, at_count, at, 
			 find_types, find_scope, found_type);
  }
  else {
    int count = count_objs(looker, objGetContents(in), at, NOTHING,
			    (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      *found_type = FOUND_OBJ;
      return find_obj(looker, objGetContents(in), at_count, at, NOTHING,
		      (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
      return NULL;
  }
}


LIST *find_all(CHAR_DATA *looker, const char *at, bitvector_t find_types,
	       bitvector_t find_scope, int *found_type) {
  *found_type = FOUND_LIST;

  /************************************************************/
  /*                        FIND ALL OBJS                     */
  /************************************************************/
  if(find_types == FIND_TYPE_OBJ) {
    LIST *obj_list = newList();
    
    // get everything from our inventory
    if(IS_SET(find_scope, FIND_SCOPE_INV)) {
      LIST *inv_objs = find_all_objs(looker,charGetInventory(looker),at,NOTHING,
				     (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
      OBJ_DATA *obj = NULL;
      while( (obj = listPop(inv_objs)) != NULL)
	listPut(obj_list, obj);
      deleteList(inv_objs);
    }

    // get everything from the room
    if(IS_SET(find_scope, FIND_SCOPE_ROOM)) {
      LIST *room_objs = find_all_objs(looker, 
				      roomGetContents(charGetRoom(looker)),
				      at, NOTHING,
				     (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
      OBJ_DATA *obj = NULL;
      while( (obj = listPop(room_objs)) != NULL)
	listPut(obj_list, obj);
      deleteList(room_objs);
    }

    // get everything we are wearing
    if(IS_SET(find_scope, FIND_SCOPE_WORN)) {
      // we have to get a list of all eq as an intermediary step, and then
      // delete the list after we search through it again for everything
      // that we can see.
      LIST *equipment = bodyGetAllEq(charGetBody(looker));
      LIST *eq_objs = find_all_objs(looker, equipment, at, NOTHING,
				    (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
      deleteList(equipment);
      OBJ_DATA *obj = NULL;
      while( (obj = listPop(eq_objs)) != NULL)
	listPut(obj_list, obj);
      deleteList(eq_objs);
    }

    // get everything in the world
    if(IS_SET(find_scope, FIND_SCOPE_WORLD)) {
      LIST *wld_objs = find_all_objs(looker,
				     object_list, at, NOTHING, 
				     (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
      OBJ_DATA *obj = NULL;
      while( (obj = listPop(wld_objs)) != NULL)
	listPut(obj_list, obj);
      deleteList(wld_objs);
    }

    // if we didn't find anything, return NULL
    if(listSize(obj_list) < 1) {
      deleteList(obj_list);
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
    *found_type = FOUND_NONE;
    return NULL;
  }
  

  /************************************************************/
  /*                       FIND ALL EDESCS                    */
  /************************************************************/
  else {
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
      *found_type = FOUND_CHAR;
      return looker;
    }
  }

  // seach our inventory
  if(IS_SET(find_scope, FIND_SCOPE_INV) && 
     IS_SET(find_types, FIND_TYPE_OBJ)) {
    count = count_objs(looker, charGetInventory(looker), at, NOTHING, 
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      *found_type = FOUND_OBJ;
      return find_obj(looker, charGetInventory(looker), at_count, at, NOTHING,
		      (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
      at_count -= count;
  }
  
  // search our equipment
  if(IS_SET(find_scope, FIND_SCOPE_WORN) &&
     IS_SET(find_types, FIND_TYPE_OBJ)) {
    LIST *equipment = bodyGetAllEq(charGetBody(looker));
    count = count_objs(looker, equipment, at, NOTHING, 
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      *found_type = FOUND_OBJ;
      OBJ_DATA *obj = find_obj(looker, equipment, at_count, at, NOTHING, 
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
  // search objects in the room
  if(IS_SET(find_scope, FIND_SCOPE_ROOM) && 
     IS_SET(find_types, FIND_TYPE_OBJ)) {
    count = count_objs(looker, roomGetContents(charGetRoom(looker)), at, 
		       NOTHING, (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
      *found_type = FOUND_OBJ;
      return find_obj(looker, roomGetContents(charGetRoom(looker)), at_count,
		      at, NOTHING, (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
      at_count -= count;
  }

  // seach the characters in the room
  if(IS_SET(find_scope, FIND_SCOPE_ROOM) &&
     IS_SET(find_types, FIND_TYPE_CHAR)) {
    count = count_chars(looker, roomGetCharacters(charGetRoom(looker)), at,
		       NOBODY, (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
	*found_type = FOUND_CHAR;
	return find_char(looker, roomGetCharacters(charGetRoom(looker)), 
			 at_count, at, NOBODY,
			 (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
	at_count -= count;
  }


  //*****************************************
  // The exit stuff in the next two if
  // blocks is kinda bulky. We should really
  // either cut it down to size, or throw
  // it in its own subfunction.
  //*****************************************

  // search exits in the room
  if(IS_SET(find_scope, FIND_SCOPE_ROOM) &&
     IS_SET(find_types, FIND_TYPE_EXIT)) {
    EXIT_DATA *exit = NULL;

    if(dirGetNum(at) != DIR_NONE) 
	exit = roomGetExit(charGetRoom(looker), dirGetNum(at));
    else if(dirGetAbbrevNum(at) != DIR_NONE)
	exit = roomGetExit(charGetRoom(looker), dirGetAbbrevNum(at));
    else
	exit = roomGetExitSpecial(charGetRoom(looker), at);
    // we found one
    if(exit && 
	 (!IS_SET(find_scope, FIND_SCOPE_VISIBLE) || 
	  can_see_exit(looker, exit))) {
	at_count--;
	if(at_count == 0) {
	  *found_type = FOUND_EXIT;
	  return exit;
	}
    }
  }

  // search doors in the room -> exit keywords
  if(IS_SET(find_scope, FIND_SCOPE_ROOM) &&
     IS_SET(find_types, FIND_TYPE_EXIT)) {
    EXIT_DATA *exit = NULL;
    int ex_i = 0;

    // traditional exits first
    for(ex_i = 0; ex_i < NUM_DIRS; ex_i++) {
      exit = roomGetExit(charGetRoom(looker), ex_i);
      if(!exit)
	continue;
      if(!exitIsName(exit, at))
	continue;

      if((!IS_SET(find_scope, FIND_SCOPE_VISIBLE) || 
	  can_see_exit(looker, exit))) {
	at_count--;
	if(at_count == 0) {
	  *found_type = FOUND_EXIT;
	  return exit;
	}
      }
    }

    // now special exits
    int num_exits = 0;
    char **ex_names = roomGetExitNames(charGetRoom(looker), &num_exits);
    for(ex_i = 0; ex_i < num_exits; ex_i++) {
      exit = roomGetExitSpecial(charGetRoom(looker), ex_names[ex_i]);
      if(!exit)
	continue;
      if(!exitIsName(exit, at))
	continue;

      if((!IS_SET(find_scope, FIND_SCOPE_VISIBLE) || 
	  can_see_exit(looker, exit))) {
	at_count--;
	if(at_count == 0) {
	  *found_type = FOUND_EXIT;
	  // don't return it yet... we need to clean up our mess
	  break;
	}
      }
    }

    // if we iterated to the end, null the exit we 
    // were last at so we don't choose to return it
    if(ex_i == num_exits)
      exit = NULL;

    // clean up our mess
    for(ex_i = 0; ex_i < num_exits; ex_i++)
      free(ex_names[ex_i]);
    free(ex_names);

    // we got one
    if(exit)
      return exit;
  }



  // search extra descriptions in the room
  if(IS_SET(find_scope, FIND_SCOPE_ROOM) &&
     IS_SET(find_types, FIND_TYPE_EDESC)) {
    count = (roomGetEdesc(charGetRoom(looker), at) != NULL);
    if(count && at_count == 1) {
	*found_type = FOUND_EDESC;
	return getEdesc(roomGetEdescs(charGetRoom(looker)), at);
    }
    else
	at_count--;
  }


  /************************************************************/
  /*                     GLOBAL SEARCHES                      */
  /************************************************************/
  // search objects in the world
  if(IS_SET(find_scope, FIND_SCOPE_WORLD) &&
     IS_SET(find_types, FIND_TYPE_OBJ)) {
    count = count_objs(looker, object_list, at, NOTHING, 
			 (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
	*found_type = FOUND_OBJ;
	return find_obj(looker, object_list, at_count, at, NOTHING, 
			(IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
	at_count -= count;
  }

  // search characters in the world
  if(IS_SET(find_scope, FIND_SCOPE_WORLD) &&
     IS_SET(find_types, FIND_TYPE_CHAR)) {
    count = count_chars(looker, mobile_list, at, NOBODY, 
		       (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    if(count >= at_count) {
	*found_type = FOUND_CHAR;
	return find_char(looker, mobile_list, at_count, at, NOBODY,
			 (IS_SET(find_scope, FIND_SCOPE_VISIBLE)));
    }
    else
	at_count -= count;
  }

  // we didn't find anything!
  *found_type = FOUND_NONE;
  return NULL;
}


void *generic_find(CHAR_DATA *looker, char *arg,
		   bitvector_t find_types, 
		   bitvector_t find_scope, 
		   bool all_ok, int *found_type) {
  strip_word(arg, "the");

  char *at = at_arg(arg);
  char *in = in_arg(arg);
  char *on = on_arg(arg);

  strip_word(arg, "at");
  strip_word(arg, "in");
  strip_word(arg, "on");
  trim(arg);

  // if we don't have an "at", and there's a word we haven't pulled,
  // pull the first word before a space
  if(!at &&
     // figure out the difference between the
     // number of words that exist and the number
     // of words we've pulled. If it's > 0, we 
     // haven't pulled a word
     (count_letters(arg, ' ', strlen(arg)) + 1 - 
      (in != NULL) - (on != NULL)) == 1) {
    at = strtok(arg, " ");
    at = strdup(at ? at : "");
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
		    char *at, char *on, char *in,
		    bitvector_t find_types,
		    bitvector_t find_scope,
		    bool all_ok, int *found_type) {
  // for stuff like 2.sword, all.woman, etc...
  int at_count = 1;
  int on_count = 1;
  int in_count = 1;

  // separate the names from their numbers
  get_count(at, at, &at_count);
  get_count(in, in, &in_count);
  get_count(on, on, &on_count);

  *found_type = FOUND_NONE;


  // are we trying to find all of something?
  if(all_ok && at_count == COUNT_ALL && !*on && !*in)
    return find_all(looker, at, find_types, find_scope, found_type);

  else if(!*at || at_count == 0) {
    // we're trying to find what the contents of an item are?
    // e.g. "look in portal", "look in bag"
    if(in && *in && in_count >= 1 &&
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
	*found_type = FOUND_IN_OBJ;
	return tgt;
      }
    }
    // we're just not looking for anything
    else {
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
    if(in && *in && in_count > 0) {
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
    else if(on && *on && on_count > 0) {
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