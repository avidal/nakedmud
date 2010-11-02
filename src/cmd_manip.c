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
#include "object.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "items/items.h"
#include "items/container.h"



//*****************************************************************************
// local functions
//*****************************************************************************

//
// used by open, close, lock, and unlock. When an exit is manipulated on one
// side, it is the case that we'll want to do an identical manipulation on the
// other side. That's what we do here. Note: Can only do close OR lock with
// one call to this function. Cannot handle both at the same time!
void try_manip_other_exit(ROOM_DATA *room, EXIT_DATA *exit,
			  bool closed, bool locked) {
  // see if there's a room on the other side
  ROOM_DATA         *to = worldGetRoom(gameworld, exitGetTo(exit));
  EXIT_DATA *other_exit = NULL;
  const char   *opp_dir = NULL;
  if(to == NULL)
    return;

  // check to see if we can figure out a return direction
  if(*exitGetOpposite(exit))
    opp_dir = exitGetOpposite(exit);
  else {
    int opp_num = dirGetNum(roomGetExitDir(room, exit));
    if(opp_num != DIR_NONE)
      opp_dir = dirGetName(dirGetOpposite(opp_num));
  }

  // do we have an opposite direction to manipulate?
  if(opp_dir == NULL)
    return;

  // do we have an opposite exit to manipulate?
  if( (other_exit = roomGetExit(to, opp_dir)) != NULL) {
    // are we changing the close state, and the exit's not locked?
    if(exitIsClosed(other_exit) != closed && !exitIsLocked(other_exit)) {
      exitSetClosed(other_exit, closed);
      send_to_list(roomGetCharacters(to),
		   "%s %s from the other side.\r\n",
		   (*exitGetName(other_exit)?exitGetName(other_exit):
		    "An exit"),
		   (closed ? "closes" : "opens"));
    }

    // are we changing the lock state, and the exit is closed?
    if(exitIsLocked(other_exit) != locked && exitIsClosed(other_exit)) {
      exitSetLocked(other_exit, locked);
      send_to_list(roomGetCharacters(to),
		   "%s %s from the other side.\r\n",
		   (*exitGetName(other_exit)?exitGetName(other_exit):
		    "An exit"),
		   (locked ? "locks" : "unlocks"));
    }
  }
}



//*****************************************************************************
// player commands
//*****************************************************************************


//
// try to lock an exit or container. The container can be anything in our
// immediate visible range (room, inventory, body). do_lock automatically
// checks if we have the key on us.
//
//  examples:
//    lock door                lock a door in the room
//    lock south               lock the south exit
//    lock 2.chest             lock the 2nd chest in our visible range
COMMAND(cmd_lock) {
  int found_type = PARSE_NONE;
  void    *found = NULL;

  if(!parse_args(ch, TRUE, cmd, arg, "[the] { obj.room.inv.eq exit }", 
		 &found, &found_type))
    return;

  // did we find an exit or an object?
  if(found_type == PARSE_EXIT) {
    if(!exitIsClosed(found))
      send_to_char(ch, "%s must be closed first.\r\n", exitGetName(found));
    else if(exitIsLocked(found))
      send_to_char(ch, "%s is already locked.\r\n", exitGetName(found));
    else if(!*exitGetKey(found))
      send_to_char(ch, "You cannot figure out how %s would be locked.\r\n",
		   exitGetName(found));
    else if(!has_obj(ch, get_fullkey_relative(exitGetKey(found), 
			     get_key_locale(roomGetClass(charGetRoom(ch))))))
      send_to_char(ch, "You cannot seem to find the key.\r\n");
    else {
      send_to_char(ch, "You lock %s.\r\n", exitGetName(found));
      send_around_char(ch, TRUE, "%s locks %s.\r\n", 
		       charGetName(ch), exitGetName(found));
      exitSetLocked(found, TRUE);

      // and try the other side
      try_manip_other_exit(charGetRoom(ch), found, exitIsClosed(found),
			   TRUE);
    }
  }

  // object found
  else { // if(found_type == PARSE_OBJ) {
    if(!objIsType(found, "container"))
      send_to_char(ch, "%s is not a container.\r\n", objGetName(found));
    else if(!containerIsClosed(found))
      send_to_char(ch, "%s must be closed first.\r\n", objGetName(found));
    else if(containerIsLocked(found))
      send_to_char(ch, "%s is already locked.\r\n", objGetName(found));
    else if(!*containerGetKey(found))
      send_to_char(ch, "You cannot figure out how %s would be locked.\r\n",
		   objGetName(found));
    else if(!has_obj(ch, get_fullkey_relative(containerGetKey(found),
				     get_key_locale(objGetClass(found)))))
      send_to_char(ch, "You cannot seem to find the key.\r\n");
    else {
      send_to_char(ch, "You lock %s.\r\n", objGetName(found));
      message(ch, NULL, found, NULL, TRUE, TO_ROOM, "$n locks $o.");
      containerSetLocked(found, TRUE);
    }
  }
}


//
// the opposite of lock
COMMAND(cmd_unlock) {
  int found_type = PARSE_NONE;
  void    *found = NULL;

  if(!parse_args(ch, TRUE, cmd, arg, "[the] { obj.room.inv exit }", 
		 &found, &found_type))
    return;

  // did we find something?
  if(found_type == PARSE_EXIT) {
    if(!exitIsLocked(found))
      send_to_char(ch, "%s is not locked.\r\n", exitGetName(found));
    else if(!*exitGetKey(found))
      send_to_char(ch, "You cannot figure out how %s would be unlocked.\r\n",
		   exitGetName(found));
    else if(!has_obj(ch, get_fullkey_relative(exitGetKey(found), 
			     get_key_locale(roomGetClass(charGetRoom(ch))))))
      send_to_char(ch, "You cannot seem to find the key.\r\n");
    else {
      send_to_char(ch, "You unlock %s.\r\n", exitGetName(found));
      send_around_char(ch, TRUE, "%s unlocks %s.\r\n", 
		       charGetName(ch), exitGetName(found));
      exitSetLocked(found, FALSE);

      // and try the other side
      try_manip_other_exit(charGetRoom(ch), found, exitIsClosed(found),
			   FALSE);
    }
  }

  else { // if(found_type == PARSE_OBJ) {
    if(!objIsType(found, "container"))
      send_to_char(ch, "%s is not a container.\r\n", objGetName(found));
    else if(!containerIsLocked(found))
      send_to_char(ch, "%s is not locked.\r\n", objGetName(found));
    else if(!*containerGetKey(found))
      send_to_char(ch, "You cannot figure out how %s would be unlocked.\r\n",
		   objGetName(found));
    else if(!has_obj(ch, get_fullkey_relative(containerGetKey(found), 
				     get_key_locale(objGetClass(found)))))
      send_to_char(ch, "You cannot seem to find the key.\r\n");
    else {
      send_to_char(ch, "You unlock %s.\r\n", objGetName(found));
      message(ch, NULL, found, NULL, TRUE, TO_ROOM, "$n unlocks $o.");
      containerSetLocked(found, FALSE);
    }
  }
}


//
//  put one thing into another. The thing you wish to put must be in
//  your inventory. The container must be in your immediate visible range
//  (room, inventory, body)
//
//  usage: put [the] <thing> [in the] <container>
//
//  examples:
//    put coin bag             put a coin into the bag
//    put all.shirt closet     put all of the shirts in the closet
COMMAND(cmd_put) {
  void    *found = NULL;
  bool  multiple = FALSE;
  OBJ_DATA *cont = NULL;

  if(!parse_args(ch, TRUE, cmd, arg, 
		 "[the] obj.inv.multiple [in the] obj.room.inv",
		 &found, &multiple, &cont))
    return;
  
  // make sure we have a container
  if(!objIsType(cont, "container"))
    send_to_char(ch, "%s is not a container.\r\n", objGetName(cont));

  // did we find a list of things or a single item?
  else if(multiple == FALSE)
    do_put(ch, found, cont);

  // we have to move a bunch of things
  else {
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
//  usage: open [the] <thing>
//
//  examples:
//    open door               open a door
//    open 2.bag              open your second bag
//    open east               open the east exit
//    open backpack on self   open a backpack you are wearing
COMMAND(cmd_open) {
  void    *found = NULL;
  int found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, "{ obj.room.inv.eq exit }",
		 &found, &found_type))
    return;

  // open an exit
  if(found_type == PARSE_EXIT) {
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
      mssgprintf(ch, NULL, NULL, NULL, FALSE, TO_ROOM, "$n opens %s.", 
		 (*exitGetName(found) ? exitGetName(found) : "an exit"));
      send_to_char(ch, "You open %s.\r\n",
		   (*exitGetName(found) ? exitGetName(found) : "the exit"));
      exitSetClosed(found, FALSE);

      // try opening the other side
      try_manip_other_exit(charGetRoom(ch), found, FALSE, exitIsLocked(found));
    }
  }

  // open a container
  else { // if(found_type == FOUND_OBJ) {
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
}


//
// cmd_close is used to close containers and exits.
//   usage: open <thing>
//
//   examples:
//     close door               close a door
//     close 2.bag              close your second bag
//     close east               close the east exit
//     close backpack on self   close a backpack you are wearing
COMMAND(cmd_close) {
  void    *found = NULL;
  int found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, "{ obj.room.eq.inv exit }",
		 &found, &found_type))
    return;

  // close an exit
  if(found_type == PARSE_EXIT) {
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

      // try opening the other side
      try_manip_other_exit(charGetRoom(ch), found, TRUE, exitIsLocked(found));
    }
  }

  // close a container
  else { // if(found_type == PARSE_OBJ) {
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
}


//
// cmd_get is used to move objects from containers or the room to your inventory
//   usage: get <object> <from>
//
//   examples:
//     get sword            get a sword from the room
//     get 2.cupcake bag    get the second cupcake from your bag
//     get all.coin         get all of the coins on the ground
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
      found      = find_all_objs(ch, objGetContents(cont), name, NULL, TRUE);
    }
    else {
      found_type = FOUND_OBJ;
      found      = find_obj(ch, objGetContents(cont), count, name, NULL, TRUE);
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
//   usage: give [the] <object> [to] <person>
//
//   examples:
//     give doll girl           give a doll in your inventory to a girl
//     give all.coin robber     give all of your money to the robber
//
COMMAND(cmd_give) {
  CHAR_DATA *recv = NULL;  // the person we're giving stuff to
  void   *to_give = NULL;  // may be a list or a single item
  bool   multiple = FALSE; // are we giving one or multiple items?

  // try to give objects from our inventory. We can give multiple items. Give
  // them to a person in the room who is not ourself. The fact we can see the
  // receiver is implied. If we fail to find our items or receiver, parse_args
  // will tell the character what he did wrong, and we will halt the command
  if(!parse_args(ch,TRUE,cmd,arg, "[the] obj.inv.multiple [to] ch.room.noself",
		 &to_give, &multiple, &recv))
    return;

  // just a single item to give...
  if(multiple == FALSE)
    do_give(ch, recv, to_give);
  // we have a list of items to give
  else {
    LIST_ITERATOR *obj_i = newListIterator(to_give);
    OBJ_DATA        *obj = NULL;
    ITERATE_LIST(obj, obj_i) {
      do_give(ch, recv, obj);
    } deleteListIterator(obj_i);

    // we also have to delete the list that parse_args sent us
    deleteList(to_give);
  }
}


//
// cmd_drop is used to transfer an object in your inventory to the ground
//   usage: drop <item>
//
//   examples:
//     drop bag          drop a bag you have
//     drop all.bread    drop all of the bread you are carrying
//     drop 2.cupcake    drop the second cupcake in your posession
COMMAND(cmd_drop) {
  void   *found = NULL;
  bool multiple = FALSE;

  if(!parse_args(ch, TRUE, cmd, arg, "[the] obj.inv.multiple",&found,&multiple))
    return;

  // are we dropping a list of things, or just one?
  if(multiple == FALSE)
    do_drop(ch, found);

  // we got a list of things... drop 'em all
  else {
    OBJ_DATA *obj = NULL;
    while( (obj = listPop(found)) != NULL)
      do_drop(ch, obj);
    deleteList(found);
  }
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
COMMAND(cmd_wear) {
  void   *found = NULL;
  bool multiple = FALSE;
  char   *where = NULL;

  if(!parse_args(ch, TRUE, cmd, arg, "[the] obj.inv.multiple | [on my] string", 
		 &found, &multiple, &where))
    return;

  // are we wearing one thing, or multiple things?
  if(multiple == FALSE)
    do_wear(ch, found, where);

  // we're trying to wear multiple items
  else {
    OBJ_DATA *obj = NULL;
    while( (obj = listPop(found)) != NULL)
      do_wear(ch, obj, where);
    deleteList(found);
  }
}


//
// cmd_remove is used to unequip items on your body to your inventory
//   usage: remove <item>
//
//   examples:
//     remove mask             remove the mask you are wearing
//     remove all.ring         remove all the rings you have on
//     remove 2.ring           remove the 2nd ring you have equipped
COMMAND(cmd_remove) {
  void   *found = NULL;
  bool multiple = FALSE;

  if(!parse_args(ch, TRUE, cmd, arg, "obj.eq.multiple", &found, &multiple))
    return;

  // are we trying to remove one thing, or multiple things?
  if(multiple == FALSE)
    do_remove(ch, found);

  // removing multiple things...
  else {
    OBJ_DATA *obj = NULL;
    while( (obj = listPop(found)) != NULL)
      do_remove(ch, obj);
    deleteList(found);
  }
}
