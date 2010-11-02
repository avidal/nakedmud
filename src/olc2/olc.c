//*****************************************************************************
//
// olc.c
//
// My first go at OLC was during the pre-module time, and it really wasn't all
// that well put together in the first place. This is the second go at it...
// this time, OLC is more like some general functions that make it easy to do
// online editing, rather than a full system. It will be amenable to adding
// new extentions to the general OLC framework from other modules, and it will
// essentially just be a bunch spiffier.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../world.h"
#include "../character.h"  // for POS_XXX <-- we should change the place of this
#include "../auxiliary.h"
#include "olc.h"



//*****************************************************************************
//
// local data structures, defines, and functions
//
//*****************************************************************************
typedef struct olc_data {
  void    (* menu)(SOCKET_DATA *, void *);
  int  (* chooser)(SOCKET_DATA *, void *, const char *);
  bool  (* parser)(SOCKET_DATA *, void *, int, const char *);
  void *(* copier)(void *);
  void  (* copyto)(void *, void *);
  void (* deleter)(void *);
  void   (* saver)(void *); 
  void *data;
  void *working_copy;
  int  cmd;
} OLC_DATA;

OLC_DATA *newOLC(void    (* menu)(SOCKET_DATA *, void *),
		 int  (* chooser)(SOCKET_DATA *, void *, const char *),
		 bool  (* parser)(SOCKET_DATA *, void *, int, const char *),
		 void *(* copier)(void *),
		 void  (* copyto)(void *, void *),
		 void (* deleter)(void *),
		 void   (* saver)(void *),
		 void *data) {
  OLC_DATA *olc     = malloc(sizeof(OLC_DATA));
  olc->menu         = menu;
  olc->chooser      = chooser;
  olc->parser       = parser;
  olc->copier       = copier;
  olc->copyto       = copyto;
  olc->deleter      = deleter;
  olc->saver        = saver;
  olc->data         = data;
  olc->working_copy = (copier ? copier(data) : data);
  olc->cmd          = MENU_NOCHOICE;
  return olc;
}

void deleteOLC(OLC_DATA *olc) {
  if(olc->deleter) olc->deleter(olc->working_copy);
  free(olc);
}



//*****************************************************************************
//
// auxiliary data we put on the socket
//
//*****************************************************************************
typedef struct olc_aux_data {
  LIST *olc_stack; // the list of OLCs we have opened
} OLC_AUX_DATA;

OLC_AUX_DATA *
newOLCAuxData() {
  OLC_AUX_DATA *data = malloc(sizeof(OLC_AUX_DATA));
  data->olc_stack = newList();
  return data;
}

void
deleteOLCAuxData(OLC_AUX_DATA *data) {
  if(data->olc_stack) deleteListWith(data->olc_stack, deleteOLC);
  free(data);
}



//*****************************************************************************
//
// Implementation of the OLC framework
//
//*****************************************************************************

//
// display the current menu to the socket, as well as the generic prompt
//
void olc_menu(SOCKET_DATA *sock) {
  // send the current menu
  OLC_AUX_DATA *aux_olc = socketGetAuxiliaryData(sock, "olc_aux_data");
  OLC_DATA         *olc = listGet(aux_olc->olc_stack, 0);
  // don't display then menu if we've made a menu choice
  if(olc->cmd == MENU_NOCHOICE) {
    text_to_buffer(sock, CLEAR_SCREEN);
    olc->menu(sock, olc->working_copy);
    text_to_buffer(sock, "\r\nEnter choice, or Q to quit: ");
  }
}


//
// handle the cleanup process for exiting OLC; pop the top handler off of
// the OLC stack and delete it. Save changes if neccessary. Exit the OLC handler
// if we have no more OLC work to do. Return the new and improved version of
// whatever it was we were editing.
//
void *olc_exit(SOCKET_DATA *sock, bool save) {
  // pull up the OLC data to manipulate
  OLC_AUX_DATA *aux_olc = socketGetAuxiliaryData(sock, "olc_aux_data");
  OLC_DATA         *olc = listGet(aux_olc->olc_stack, 0);
  void            *data = olc->data;

  // pop our OLC off of the OLC stack
  listPop(aux_olc->olc_stack);

  // if we need to do any saving, do it now
  if(save) {
    // first, save the changes on the item
    if(olc->working_copy != olc->data && olc->copyto)
      olc->copyto(olc->working_copy, olc->data);
    // now, make sure the changes go to disk
    if(olc->saver)
      olc->saver(olc->data);
  }

  // delete our OLC data (this also deletes our working copy 
  // if it's not the same as our main copy)
  deleteOLC(olc);

  // if our OLC stack is empty, pop the OLC input handler
  if(listSize(aux_olc->olc_stack) == 0)
    socketPopInputHandler(sock);
  return data;
}


//
// Handle input while in OLC
//
void olc_handler(SOCKET_DATA *sock, char *arg) {
  // pull up the OLC data. Are we entering a new command, or are
  // we entering in an argument from a menu choice?
  OLC_AUX_DATA *aux_olc = socketGetAuxiliaryData(sock, "olc_aux_data");
  OLC_DATA         *olc = listGet(aux_olc->olc_stack, 0);

  // we're giving an argument for a menu choice we've already selected
  if(olc->cmd > MENU_NOCHOICE) {
    // the change went alright. Re-display the menu
    if(olc->parser(sock, olc->working_copy, olc->cmd, arg)) {
      olc->cmd = MENU_NOCHOICE;
      olc_menu(sock);
    }
    else
      text_to_buffer(sock, "Invalid choice!\r\nTry again: ");
  }

  // we're being prompted if we want to save our changes or not before quitting
  else if(olc->cmd == MENU_CHOICE_CONFIRM_SAVE) {
    switch(toupper(*arg)) {
    case 'Y':
    case 'N':
      olc_exit(sock, (toupper(*arg) == 'Y' ? TRUE : FALSE));
      break;
    default:
      text_to_buffer(sock, "Invalid choice!\r\nTry again: ");
      break;
    }
  }

  // we're entering a new choice from our current menu
  else {
    switch(*arg) {
      // we're quitting
    case 'q':
    case 'Q':
      // if our working copy is different from our actual data, prompt to
      // see if we want to save our changes or not
      if(olc->working_copy != olc->data) {
	text_to_buffer(sock, "Save changes (Y/N): ");
	olc->cmd = MENU_CHOICE_CONFIRM_SAVE;
      }
      else {
	olc_exit(sock, TRUE);
      }
      break;

    default: {
      int cmd = olc->chooser(sock, olc->working_copy, arg);
      // the menu choice we entered wasn't a valid one. redisplay the menu
      if(cmd == MENU_CHOICE_INVALID)
	olc_menu(sock);
      // the menu choice was acceptable. Note this in our data
      else if(cmd > MENU_NOCHOICE)
	olc->cmd = cmd;
      break;
    }
    }
  }
}



//*****************************************************************************
//
// implementation of olc.h
//
//*****************************************************************************

void init_olc2() {
  // install our auxiliary data on the socket so 
  // we can keep track of our olc data
  auxiliariesInstall("olc_aux_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_SOCKET,
				       newOLCAuxData, deleteOLCAuxData,
				       NULL, NULL, NULL, NULL));

  // initialize all of our basic OLC commands
  extern COMMAND(cmd_redit);
  extern COMMAND(cmd_zedit);
  extern COMMAND(cmd_dedit);
  extern COMMAND(cmd_medit);
  extern COMMAND(cmd_oedit);
  add_cmd("zedit", NULL, cmd_zedit, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, TRUE);
  add_cmd("redit", NULL, cmd_redit, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, TRUE);
  add_cmd("dedit", NULL, cmd_dedit, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, TRUE);
  add_cmd("medit", NULL, cmd_medit, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, TRUE);
  add_cmd("oedit", NULL, cmd_oedit, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, TRUE);
}

void do_olc(SOCKET_DATA *sock,
	    void *menu,
	    void *chooser,
	    void *parser,
	    void *copier,
	    void *copyto,
	    void *deleter,
	    void *saver,
	    void *data) {
  // first, we create a new OLC data structure, and then push it onto the stack
  OLC_DATA *olc = newOLC(menu, chooser, parser, copier, copyto, deleter,
			 saver, data);
  OLC_AUX_DATA *aux_olc = socketGetAuxiliaryData(sock, "olc_aux_data");
  listPush(aux_olc->olc_stack, olc);

  // if this is the only olc data on the stack, then enter the OLC handler
  if(listSize(aux_olc->olc_stack) == 1)
    socketPushInputHandler(sock, olc_handler, olc_menu);
}

void save_world(void *olc_data) {
  worldSave(gameworld, WORLD_PATH);
}

void olc_display_table(SOCKET_DATA *sock, const char *getName(int val),
		       int num_vals, int num_cols) {
  int i, print_room;
  static char fmt[100];

  print_room = (80 - 10*num_cols)/num_cols;
  sprintf(fmt, "  {c%%2d{y) {g%%-%ds%%s", print_room);

  for(i = 0; i < num_vals; i++)
    send_to_socket(sock, fmt, i, getName(i), 
		   (i % num_cols == (num_cols - 1) ? "\r\n" : "   "));

  if(i % num_cols != 0)
    send_to_socket(sock, "\r\n");
}

void olc_display_list(SOCKET_DATA *sock, LIST *list, int num_cols) {
  static char fmt[100];
  LIST_ITERATOR *list_i = newListIterator(list);
  int print_room, i = 0;
  char *str = NULL;
  
  print_room = (80 - 10*num_cols)/num_cols;
  sprintf(fmt, "  {c%%2d{y) {g%%-%ds%%s", print_room);

  ITERATE_LIST(str, list_i) {
    send_to_socket(sock, fmt, i, str, (i % num_cols == (num_cols - 1) ? 
				       "\r\n" : "   "));
    i++;
  }
  deleteListIterator(list_i);

  if(i % num_cols != 0)
    send_to_socket(sock, "\r\n");
}
