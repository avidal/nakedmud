//*****************************************************************************
//
// cmd_manip.c
//
// a set of commands that NakedMud(tm) comes with that allows characters to
// manipulate various things. These commands are mostly directed towards
// manipulating objects (e.g. get, put, drop, etc...) but can also affect other
// things like exits (e.g. open, close)
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "handler.h"
#include "inform.h"
#include "character.h"
#include "exit.h"
#include "world.h"
#include "room.h"
#include "extra_descs.h"
#include "object.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "items/items.h"
#include "items/container.h"



//
// try to lock an exit or container. The container can be anything in our
// immediate visible range (room, inventory, body). do_lock automatically
// checks if we have the key on us.
//
//  usage: lock [thing]
//
//  examples:
//    lock door                lock a door in the room
//    lock south               lock the south exit
//    lock 2.chest             lock the 2nd chest in our visible range
//
COMMAND(cmd_lock) {
  if(!arg || !*arg) {
    send_to_char(ch, "What were you wanting to lock?\r\n");
    return;
  }

  int found_type = FOUND_NONE;
  void *found    = generic_find(ch, arg,
				FIND_TYPE_OBJ | FIND_TYPE_EXIT, 
				FIND_SCOPE_IMMEDIATE,
				FALSE, &found_type);


  // did we find something?
  if(found && found_type == FOUND_EXIT) {
    if(!exitIsClosed(found))
      send_to_char(ch, "%s must be closed first.\r\n", exitGetName(found));
    else if(exitIsLocked(found))
      send_to_char(ch, "%s is already locked.\r\n", exitGetName(found));
    else if(exitGetKey(found) == NOTHING)
      send_to_char(ch, "You cannot figure out how %s would be locked.\r\n",
		   exitGetName(found));
    else if(!has_obj(ch, exitGetKey(found)))
      send_to_char(ch, "You cannot seem to find the key.\r\n");
    else {
      send_to_char(ch, "You lock %s.\r\n", exitGetName(found));
      send_around_char(ch, TRUE, "%s locks %s.\r\n", 
		       charGetName(ch), exitGetName(found));
      exitSetLocked(found, TRUE);
    }
  }

  else if(found && found_type == FOUND_OBJ) {
    if(!objIsType(found, "container"))
      send_to_char(ch, "%s is not a container.\r\n", objGetName(found));
    else if(!containerIsClosed(found))
      send_to_char(ch, "%s must be closed first.\r\n", objGetName(found));
    else if(containerIsLocked(found))
      send_to_char(ch, "%s is already locked.\r\n", objGetName(found));
    else if(containerGetKey(found) == NOTHING)
      send_to_char(ch, "You cannot figure out how %s would be locked.\r\n",
		   objGetName(found));
    else if(!has_obj(ch, containerGetKey(found)))
      send_to_char(ch, "You cannot seem to find the key.\r\n");
    else {
      send_to_char(ch, "You lock %s.\r\n", objGetName(found));
      message(ch, NULL, found, NULL, TRUE, TO_ROOM, "$n locks $o.");
      containerSetLocked(found, TRUE);
    }
  }
  else
    send_to_char(ch, "What did you want to lock?\r\n");
}



//
// the opposite of lock
//
COMMAND(cmd_unlock) {
  if(!arg || !*arg) {
    send_to_char(ch, "What were you wanting to unlock?\r\n");
    return;
  }

  int found_type = FOUND_NONE;
  void *found    = generic_find(ch, arg,
				FIND_TYPE_OBJ | FIND_TYPE_EXIT, 
				FIND_SCOPE_IMMEDIATE,
				FALSE, &found_type);


  // did we find something?
  if(found && found_type == FOUND_EXIT) {
    if(!exitIsLocked(found))
      send_to_char(ch, "%s is not locked.\r\n", exitGetName(found));
    else if(exitGetKey(found) == NOTHING)
      send_to_char(ch, "You cannot figure out how %s would be unlocked.\r\n",
		   exitGetName(found));
    else if(!has_obj(ch, exitGetKey(found)))
      send_to_char(ch, "You cannot seem to find the key.\r\n");
    else {
      send_to_char(ch, "You unlock %s.\r\n", exitGetName(found));
      send_around_char(ch, TRUE, "%s unlocks %s.\r\n", 
		       charGetName(ch), exitGetName(found));
      exitSetLocked(found, FALSE);
    }
  }

  else if(found && found_type == FOUND_OBJ) {
    if(!objIsType(found, "container"))
      send_to_char(ch, "%s is not a container.\r\n", objGetName(found));
    else if(!containerIsLocked(found))
      send_to_char(ch, "%s is not locked.\r\n", objGetName(found));
    else if(containerGetKey(found) == NOTHING)
      send_to_char(ch, "You cannot figure out how %s would be unlocked.\r\n",
		   objGetName(found));
    else if(!has_obj(ch, containerGetKey(found)))
      send_to_char(ch, "You cannot seem to find the key.\r\n");
    else {
      send_to_char(ch, "You unlock %s.\r\n", objGetName(found));
      message(ch, NULL, found, NULL, TRUE, TO_ROOM, "$n unlocks $o.");
      containerSetLocked(found, FALSE);
    }
  }
  else
    send_to_char(ch, "What did you want to unlock?\r\n");
}




//
//  put one thing into another. The thing you wish to put must be in
//  your inventory. The container must be in your immediate visible range
//  (room, inventory, body)
//
//  usage: put [thing] [container]
//
//  examples:
//    put coin bag             put a coin into the bag
//    put all.shirt closet     put all of the shirts in the closet
//
COMMAND(cmd_put) {
  if(!arg || !*arg) {
    send_to_char(ch, "Put what where?\r\n");
    return;
  }

  // get the name of what we're trying to move
  char name[SMALL_BUFFER];
  arg = one_arg(arg, name);

  int found_type = FOUND_NONE;
  void *found    = generic_find(ch, name,
				FIND_TYPE_OBJ, 
				FIND_SCOPE_INV | FIND_SCOPE_VISIBLE,
				TRUE, &found_type);

  OBJ_DATA *cont = generic_find(ch, arg,
				FIND_TYPE_OBJ, FIND_SCOPE_IMMEDIATE,
				FALSE, NULL);

  // make sure we've got what we need
  if(!found || !cont) {
    send_to_char(ch, "Put what where?\r\n");
    return;
  }

  // make sure we have a container
  if(!objIsType(cont, "container")) {
    send_to_char(ch, "%s is not a container.\r\n", objGetName(cont));
    return;
  }

  // did we find a list of things or a single item?
  if(found_type == FOUND_OBJ)
    do_put(ch, found, cont);
  // we have to move a bunch of things
  else if(found_type == FOUND_LIST) {
    OBJ_DATA *obj = NULL;
    while( (obj = listPop(found)) != NULL)
      do_put(ch, obj, cont);
    deleteList(found);
  }
}


//
//  attempt to open a door or container. The container must be in our immediate
//  visible range (room, inventory, body).
//
//  usage: open [thing]
//
//  examples:
//    open door               open a door
//    open 2.bag              open your second bag
//    open east               open the east exit
//    open backpack on self   open a backpack you are wearing
//
COMMAND(cmd_open) {
  if(!arg || !*arg) {
    send_to_char(ch, "What did you want to open?\r\n");
    return;
  }

  int found_type = FOUND_NONE;
  void *found = generic_find(ch, arg,
			     FIND_TYPE_OBJ | FIND_TYPE_EXIT,
			     FIND_SCOPE_IMMEDIATE,
			     FALSE, &found_type);

  // open an exit
  if(found && found_type == FOUND_EXIT) {
    if(!exitIsClosable(found))
      send_to_char(ch, "But %s cannot be opened!\r\n",
		   (*exitGetName(found) ? exitGetName(found) : "it"));
    else if(!exitIsClosed(found))
      send_to_char(ch, "It is it easy to open something when someone "
		   "has already done it for you!\r\n");
    else if(exitIsLocked(found))
      send_to_char(ch, "%s appears to be locked.\r\n",
		   (*exitGetName(found) ? exitGetName(found) : "It"));
    else {
      char other_buf[SMALL_BUFFER];
      sprintf(other_buf, "$n opens %s.", (*exitGetName(found) ?
					  exitGetName(found) : "an exit"));
      message(ch, NULL, NULL, NULL, FALSE, TO_ROOM, other_buf);
      send_to_char(ch, "You open %s.\r\n",
		   (*exitGetName(found) ? exitGetName(found) : "the exit"));
      exitSetClosed(found, FALSE);
    }
  }

  // open a container
  else if(found && found_type == FOUND_OBJ) {
    // make sure it's a container and it can be opened
    if(!objIsType(found, "container") || !containerIsClosable(found))
      send_to_char(ch, "But it cannot be opened!\r\n");
    else if(!containerIsClosed(found))
      send_to_char(ch, "It is already opened.\r\n");
    else if(containerIsLocked(found))
      send_to_char(ch, "It appears to be locked.\r\n");
    else {
      send_to_char(ch, "You open %s.\r\n", objGetName(found));
      message(ch, NULL, found, NULL, FALSE, TO_ROOM, "$n opens $o.");
      containerSetClosed(found, FALSE);
    }
  }
  else
    send_to_char(ch, "What did you want to open?\r\n");
}


//
// cmd_close is used to close containers and exits.
//   usage: open [thing]
//
//   examples:
//     close door               close a door
//     close 2.bag              close your second bag
//     close east               close the east exit
//     close backpack on self   close a backpack you are wearing
//
COMMAND(cmd_close) {
  if(!arg || !*arg) {
    send_to_char(ch, "What did you want to close?\r\n");
    return;
  }

  int found_type = FOUND_NONE;
  void *found = generic_find(ch, arg,
			     FIND_TYPE_OBJ | FIND_TYPE_EXIT,
			     FIND_SCOPE_IMMEDIATE,
			     FALSE, &found_type);

  // close an exit
  if(found && found_type == FOUND_EXIT) {
    if(!exitIsClosable(found))
      send_to_char(ch, "But %s cannot be closed!\r\n",
		   (*exitGetName(found) ? exitGetName(found) : "it"));
    else if(exitIsClosed(found))
      send_to_char(ch, "It is easy to close something when someone "
		   "has already done it for you!\r\n");
    else {
      char other_buf[SMALL_BUFFER];
      sprintf(other_buf, "$n closes %s.", (*exitGetName(found) ?
					   exitGetName(found) : "an exit"));
      message(ch, NULL, NULL, NULL, FALSE, TO_ROOM, other_buf);
      send_to_char(ch, "You close %s.\r\n",
		   (*exitGetName(found) ? exitGetName(found) : "the exit"));
      exitSetClosed(found, TRUE);
    }
  }

  // close a container
  else if(found && found_type == FOUND_OBJ) {
    // make sure it's a container and it can be closed
    if(!objIsType(found, "container") || !containerIsClosable(found))
      send_to_char(ch, "But it cannot even be closed!\r\n");
    else if(containerIsClosed(found))
      send_to_char(ch, "It is already closed.\r\n");
    else {
      send_to_char(ch, "You close %s.\r\n", objGetName(found));
      message(ch, NULL, found, NULL, FALSE, TO_ROOM, "$n closes $o.");
      containerSetClosed(found, TRUE);
    }
  }
  else
    send_to_char(ch, "What did you want to close?\r\n");
}


//
// cmd_get is used to move objects from containers or the room to your inventory
//   usage: get [object] [from]
//
//   examples:
//     get sword            get a sword from the room
//     get 2.cupcake bag    get the second cupcake from your bag
//     get all.coin         get all of the coins on the ground
//
COMMAND(cmd_get) {
  if(!arg || !*arg) {
    send_to_char(ch, "What did you want to get?\r\n");
    return;
  }

  // the name of what we're trying to get
  char name[SMALL_BUFFER];
  arg = one_arg(arg, name);

  // first check to see if we're trying to get from a container
  OBJ_DATA *cont = NULL;
  if(*arg) {
    cont = generic_find(ch, arg, FIND_TYPE_OBJ, FIND_SCOPE_IMMEDIATE,
			FALSE, NULL);
    // were we trying to get something from a container 
    // but couldn't find the container?
    if(cont == NULL) {
      send_to_char(ch, "Get what from what?\r\n");
      return;
    }
    else if(!objIsType(cont, "container")) {
      send_to_char(ch, "%s is not a container.\r\n", objGetName(cont));
      return;
    }
    else if(containerIsClosed(cont)) {
      send_to_char(ch, "%s is closed. Try opening it first.\r\n", 
		   objGetName(cont));
      return;
    }
  }

  int found_type = FOUND_NONE;
  void    *found = NULL;

  // if we have a container, just search the container for things
  if(cont != NULL) {
    // oi... this is going to get messy. We have to do some stuff
    // that the generic_find will usually do for us - i.e. checking
    // to see if we need to get a list of items or a single item
    // we should really add a new function in the handler for doing this
    int count = 1;
    get_count(name, name, &count);
    if(count == COUNT_ALL) {
      found_type = FOUND_LIST;
      found      = find_all_objs(ch, objGetContents(cont), name, NOTHING, TRUE);
    }
    else {
      found_type = FOUND_OBJ;
      found      = find_obj(ch, objGetContents(cont), count, name,NOTHING,TRUE);
    }
  }
  // otherwise, search the room for visible things
  else
    found = generic_find(ch, name, 
			 FIND_TYPE_OBJ, 
			 FIND_SCOPE_ROOM | FIND_SCOPE_VISIBLE, 
			 TRUE, &found_type);


  if(found && found_type == FOUND_OBJ)
    do_get(ch, found, cont);
  else if(found && found_type == FOUND_LIST) {
    OBJ_DATA *obj = NULL;
    while( (obj = listPop(found)) != NULL)
      do_get(ch, obj, cont);
    deleteList(found);
  }
  else
    send_to_char(ch, "You can't find what you're looking for.\r\n");
}


//
// cmd_give is used to transfer an object in your possession to 
// another character
//   usage: give [object] [person]
//
//   examples:
//     give doll girl           give a doll in your inventory to a girl
//     give all.coin robber     give all of your money to the robber
//
COMMAND(cmd_give) {
  if(!arg || !*arg) {
    send_to_char(ch, "Give what to whom?\r\n");
    return;
  }
  
  strip_word(arg, "to");
  char obj_name[SMALL_BUFFER];
  arg = one_arg(arg, obj_name);

  int found_type = FOUND_NONE;
  void *found = generic_find(ch, obj_name, 
			     FIND_TYPE_OBJ, 
			     FIND_SCOPE_INV | FIND_SCOPE_VISIBLE, 
			     TRUE, &found_type);

  CHAR_DATA *recv = generic_find(ch, arg,
				 FIND_TYPE_CHAR,
				 FIND_SCOPE_ROOM | FIND_SCOPE_VISIBLE,
				 FALSE, NULL);

  if(!recv)
    send_to_char(ch, "Whom where you looking for?\r\n");
  else if(recv == ch)
    send_to_char(ch, "You don't need to give yourself anything!\r\n");
  else if(found && found_type == FOUND_LIST) {
    OBJ_DATA *obj = NULL;
    while( (obj = listPop(found)) != NULL)
      do_give(ch, recv, obj);
    deleteList(found);
  }
  else if(found && found_type == FOUND_OBJ) {
    do_give(ch, recv, found);
  }
  else
    send_to_char(ch, "Give what to whom?\r\n");
}


//
// cmd_drop is used to transfer an object in your inventory to the ground
//   usage: drop [item]
//
//   examples:
//     drop bag          drop a bag you have
//     drop all.bread    drop all of the bread you are carrying
//     drop 2.cupcake    drop the second cupcake in your posession
//
COMMAND(cmd_drop) {
  if(!arg || !*arg) {
    send_to_char(ch, "What did you want to drop?\r\n");
    return;
  }

  int found_type = FOUND_NONE;
  void *found    = generic_find(ch, arg,
				FIND_TYPE_OBJ,
				FIND_SCOPE_INV | FIND_SCOPE_VISIBLE,
				TRUE, &found_type);

  // we got a list of things... drop 'em all
  if(found_type == FOUND_LIST && found) {
    OBJ_DATA *obj = NULL;
    while( (obj = listPop(found)) != NULL)
      do_drop(ch, obj);
    deleteList(found);
  }
  else if(found_type == FOUND_OBJ && found)
    do_drop(ch, found);
  else 
    send_to_char(ch, "You can't find what you're looking for.\r\n");
}


//
// cmd_wear is used to equip wearable items in your inventory to your body
//   usage: wear [object] [where]
//
//   examples:
//     wear shirt                            equip a shirt
//     wear all.ring                         wear all of the rings in your 
//                                           inventory
//     wear gloves left hand, right hand     wear the gloves on your left and
//                                           right hands
//
COMMAND(cmd_wear) {
  if(!*arg)
    send_to_char(ch, "Wear what where?\r\n");
  else {
    char pos[MAX_BUFFER];
    arg = one_arg(arg, pos);
  
    int found_type = FOUND_NONE;
    void *found = generic_find(ch, pos,
			       FIND_TYPE_OBJ,
			       FIND_SCOPE_INV | FIND_SCOPE_VISIBLE,
			       TRUE, &found_type);

    if(found && found_type == FOUND_LIST) {
      OBJ_DATA *obj = NULL;
      while( (obj = listPop(found)) != NULL)
	do_wear(ch, obj, arg);
      deleteList(found);
    }

    else if(found && found_type == FOUND_OBJ)
      do_wear(ch, found, arg);

    else
      send_to_char(ch, "You can't find what you're looking for.\r\n");
  }
}


//
// cmd_remove is used to unequip items on your body to your inventory
//   usage: remove [item]
//
//   examples:
//     remove mask             remove the mask you are wearing
//     remove all.ring         remove all the rings you have on
//     remove 2.ring           remove the 2nd ring you have equipped
//
COMMAND(cmd_remove) {
  if(!*arg)
    send_to_char(ch, "What did you want to remove?\r\n");
  else {
    int found_type = FOUND_NONE;
    void *found = generic_find(ch, arg,
			       FIND_TYPE_OBJ,
			       FIND_SCOPE_WORN | FIND_SCOPE_VISIBLE,
			       TRUE, &found_type);

    if(found && found_type == FOUND_LIST) {
      OBJ_DATA *obj = NULL;
      while( (obj = listPop(found)) != NULL)
	do_remove(ch, obj);
      deleteList(found);
    }
    else if(found && found_type == FOUND_OBJ)
      do_remove(ch, found);
    else 
      send_to_char(ch, "What did you want to remove?\r\n");
  }
}
