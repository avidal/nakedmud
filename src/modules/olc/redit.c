//*****************************************************************************
//
// redit.c
//
// Contains all of the functions needed for online editing of rooms.
//
//*****************************************************************************

#include <mud.h>
#include <world.h>
#include <room.h>
#include <exit.h>
#include <room_reset.h>
#include <extra_descs.h>
#include <socket.h>
#include <utils.h>

#include "olc.h"

#ifdef MODULE_SCRIPTS
#include "../scripts/script_set.h"
#include "../scripts/script.h"
#endif
#ifdef MODULE_TIME
#include "../time/mudtime.h"
#endif

// if we're in the reset menu and we want to make
// a new entry, this needs to be used, instead of
// the position in the list the entry is
#define EDITING_NEW_RESET  "new_reset"


//
// Display a list of available terrains
//
void redit_terrain_menu(SOCKET_DATA *sock) {
  int i;

  for(i = 0; i < NUM_TERRAINS; i++)
    send_to_socket(sock, "  {c%2d{y) {g%-15s%s",
		   i, terrainGetName(i), (i % 3 == 2 ? "\r\n" : "    "));

  if(i % 3 != 0)
    send_to_socket(sock, "\r\n");
}


//
// display the room reset menu to the character
//
void redit_reset_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  LIST *list           = roomGetResets(olcGetData(olc));
  LIST_ITERATOR *res_i = newListIterator(list);
  RESET_DATA    *reset = NULL;
  int count = 0;

  send_to_socket(sock,
                 "\033[H\033[J"
		 "{wCurrent reset commands:\r\n");

  while( (reset = listIteratorCurrent(res_i)) != NULL) {
    listIteratorNext(res_i);
    send_to_socket(sock, " {g%2d) %s", count, write_reset(reset, 5, FALSE));
    count++;
  }
  deleteListIterator(res_i);

  send_to_socket(sock,
		 "\r\n"
		 "  {gN) new entry\r\n"
		 "  D) delete entry\r\n"
		 "  Use number to edit specific entry\r\n"
		 "\r\n"
		 "{gEnter choice (Q to quit) : {n"
		 );
}


//
// Display the exits the socket can edit
//
void redit_exit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  int i;
  ROOM_DATA *room = (ROOM_DATA *)olcGetData(olc);

  // normal exits first
  for(i = 0; i < NUM_DIRS; i++) {
    EXIT_DATA *exit = roomGetExit(room, i);
    send_to_socket(sock, "   {g%-10s : {y[%s%6d{y]%s",
		   dirGetName(i), 
		   (exit ? "{c" : "{y" ),
		   (exit ? exitGetTo(exit) : -1),
		   (!(i % 2) ? "   " : "\r\n"));
  }

  // now special exits
  int num_spec_exits = 0;
  char **room_names = roomGetExitNames(room, &num_spec_exits);
  for(i = 0; i < num_spec_exits; i++) {
    EXIT_DATA *exit = roomGetExitSpecial(room, room_names[i]);
    send_to_socket(sock, "   {g%-10s : {y[{c%6d{y]%s",
		   room_names[i],
		   exitGetTo(exit),
		   (!(i % 2) ? "   " : "\r\n"));
  }

  // make sure we've printed the last newline if needed
  if(i % 2 == 1)
    send_to_socket(sock, "\r\n");

  // clean up our mess
  for(i = 0; i < num_spec_exits; i++)
    free(room_names[i]);
  free(room_names);
}


void redit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  ROOM_DATA *room = (ROOM_DATA *)olcGetData(olc);

  send_to_socket(sock,
                 "\033[H\033[J"
		 "{g[{c%d{g]\r\n"
		 "{g1) Name\r\n"
		 "{c%s\r\n"
		 "{g2) Description\r\n"
		 "{c%s\r\n"
#ifdef MODULE_TIME
		 "{g3) Night Description (optional)\r\n"
		 "{c%s\r\n"
#endif
		 "{gT) Terrain type {y[{c%s{y]\r\n"
		 "{gX) Extra descriptions menu\r\n"
		 "{gR) Reset menu\r\n"
#ifdef MODULE_SCRIPTS
		 "{gS) Script menu\r\n"
#endif
		 "{gE) Exit menu\r\n",
		 roomGetVnum(room),
		 roomGetName(room),
		 roomGetDesc(room),
#ifdef MODULE_TIME
		 roomGetNightDesc(room),
#endif
		 terrainGetName(roomGetTerrain(room))
		 );

  redit_exit_menu(sock, olc);
  send_to_socket(sock,
		 "\r\n"
		 "{gEnter choice (Q to quit) : {n"
		 );
}


//
// the room reset main loop
//
void redit_reset_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = REDIT_RESET;

  switch(toupper(*arg)) {
  case 'Q':
    next_substate = REDIT_MAIN;
    break;

  case 'N':
    olcSetNext(olc, newOLC(OLC_RESEDIT, RESEDIT_MAIN,
			   newReset(), strdup(EDITING_NEW_RESET)));
    next_substate = REDIT_EDIT_RESET;
    break;

  case 'D':
    next_substate = REDIT_DELETE_RESET;
    send_to_socket(sock, "Which entry would you like to delete : ");
    break;

  default:
    // see if maybe they're trying to choose a description to edit
    if(!isdigit(*arg))
      redit_reset_menu(sock, olc);
    else {
      int num = atoi(arg);
      RESET_DATA *reset = listGet(roomGetResets(olcGetData(olc)), num);
      // if the one found is null, just show the menu
      if(reset == NULL)
	redit_reset_menu(sock, olc);
      else {
	olcSetNext(olc, newOLC(OLC_RESEDIT, RESEDIT_MAIN, 
			       resetCopy(reset), strdup(arg)));
	next_substate = REDIT_EDIT_RESET;
	break;
      }
    }
    break;
  }

  olcSetSubstate(olc, next_substate);
  if(next_substate == REDIT_MAIN)
    redit_menu(sock, olc);
}



//
// The main loop for editing rooms
//
void redit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = REDIT_MAIN;

  switch(toupper(*arg)) {
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = REDIT_CONFIRM_SAVE;
    break;

  case '1':
    send_to_socket(sock, "Enter new name : ");
    next_substate = REDIT_NAME;
    break;

  case '2':
    send_to_socket(sock, "Enter new description\r\n");
    start_text_editor(sock, 
		      roomGetDescPtr((ROOM_DATA *)olcGetData(olc)),
		      MAX_BUFFER, EDITOR_MODE_NORMAL);
    next_substate = REDIT_MAIN;
    break;

  case 'T':
    redit_terrain_menu(sock);
    send_to_socket(sock, "Pick a new terrain number : ");
    next_substate = REDIT_TERRAIN;
    break;

  case 'R':
    redit_reset_menu(sock, olc);
    next_substate = REDIT_RESET;
    break;

  case 'E':
    send_to_socket(sock, "Which exit do you want to edit : ");
    next_substate = REDIT_CHOOSE_EXIT;
    break;

  case 'X':
    olcSetNext(olc, newOLC(OLC_EDSEDIT, EDSEDIT_MAIN, 
			   copyEdescSet(roomGetEdescs((ROOM_DATA *)olcGetData(olc))), NULL));
    next_substate = REDIT_EDESCS;
    break;

#ifdef MODULE_SCRIPTS
  case 's':
  case 'S':
    olcSetNext(olc, newOLC(OLC_SSEDIT, SSEDIT_MAIN,
			   copyScriptSet(roomGetScripts((ROOM_DATA *)olcGetData(olc))), roomGetName((ROOM_DATA *)olcGetData(olc))));
    next_substate = REDIT_SCRIPTS;
    break;
#endif

#ifdef MODULE_TIME
  case '3':
    send_to_socket(sock, "Enter new description\r\n");
    start_text_editor(sock, 
		      roomGetNightDescPtr((ROOM_DATA *)olcGetData(olc)),
		      MAX_BUFFER, EDITOR_MODE_NORMAL);
    next_substate = REDIT_MAIN;
    break;
#endif

  default:
    redit_menu(sock, olc);
    break;
  }

  olcSetSubstate(olc, next_substate);
}



//
// The entry loop for redit. Figures out what substate we're
// in, and then enters into the appropriate subloop if possible,
// or sets a value based on arg if there is no subloop
//
void redit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  ROOM_DATA *room = (ROOM_DATA *)olcGetData(olc);
  int next_substate = REDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case REDIT_MAIN:
    redit_main_loop(sock, olc, arg);
    return;

  case REDIT_RESET:
    redit_reset_loop(sock, olc, arg);
    return;

    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case REDIT_CONFIRM_SAVE:
    switch(*arg) {
    case 'y':
    case 'Y':
      olcSetSave(olc, TRUE);
      // fall through
    case 'n':
    case 'N':
      olcSetComplete(olc, TRUE);
      return;
    default:
      send_to_socket(sock, "Please enter Y or N : ");
      next_substate = REDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case REDIT_NAME:
    roomSetName(room, arg);
    break;
  case REDIT_TERRAIN:
    roomSetTerrain(room, 
		   MIN(NUM_TERRAINS - 1,
		       MAX(TERRAIN_NONE + 1, atoi(arg))));
    break;

  case REDIT_EDESCS:
    // save the changes we made
    if(olcGetSave(olcGetNext(olc))) {
      EDESC_SET *edescs = (EDESC_SET *)olcGetData(olcGetNext(olc));
      roomSetEdescs(room, (edescs ? copyEdescSet(edescs) : newEdescSet()));
    }

    olcSetNext(olc, NULL);
    next_substate = REDIT_MAIN;
    break;

#ifdef MODULE_SCRIPTS
  case REDIT_SCRIPTS:
    // save the changes we made
    if(olcGetSave(olcGetNext(olc))) {
      SCRIPT_SET *scripts = (SCRIPT_SET *)olcGetData(olcGetNext(olc));
      roomSetScripts(room, (scripts ? copyScriptSet(scripts) : newScriptSet()));
    }
    olcSetNext(olc, NULL);
    next_substate = REDIT_MAIN;
    break;
#endif


    /******************************************************/
    /*                    RESET EDITOR                    */
    /******************************************************/
  case REDIT_DELETE_RESET: {
    RESET_DATA *reset = (isdigit(*arg)? listGet(roomGetResets(room),atoi(arg)) :
			 NULL);
    if(reset != NULL) {
      listRemove(roomGetResets(room), reset);
      deleteReset(reset);
    }
    next_substate = REDIT_RESET;
    break;
  }

  case REDIT_EDIT_RESET: {
    // save the changes we made
    if(olcGetSave(olcGetNext(olc))) {
      RESET_DATA *reset = olcGetData(olcGetNext(olc));
      if(!strcmp(EDITING_NEW_RESET, olcGetArgument(olcGetNext(olc))))
	listQueue(roomGetResets(room), resetCopy(reset));
      else {
	int num = atoi(olcGetArgument(olcGetNext(olc)));
	resetCopyTo(reset, listGet(roomGetResets(room), num));
      }
    }
    next_substate = REDIT_RESET;
    break;
  }


    /******************************************************/
    /*                     EXIT EDITOR                    */
    /******************************************************/
  case REDIT_EXIT: {
    // save the changes we made
    if(olcGetSave(olcGetNext(olc))) {
      EXIT_DATA *exit = (EXIT_DATA *)olcGetData(olcGetNext(olc));
      const char *dirName = olcGetArgument(olcGetNext(olc));
      int dir = dirGetNum(dirName);
      // special exit
      if(dir == DIR_NONE)
	roomSetExitSpecial(room, dirName, (exit ? exitCopy(exit) : NULL));
      // normal dir
      else
	roomSetExit(room, dir, (exit ? exitCopy(exit) : NULL));
    }

    olcSetNext(olc, NULL);
    next_substate = REDIT_MAIN;
    break;
  }

  case REDIT_CHOOSE_EXIT: {
    EXIT_DATA *exit = NULL;
    int dir = dirGetNum(arg);
    char *dirName = NULL;

    if(dir == DIR_NONE)
      dir = dirGetAbbrevNum(arg);

    // find the exit we're editing
    if(dir != DIR_NONE) {
      if(roomGetExit((ROOM_DATA *)olcGetData(olc), dir))
	exit = exitCopy(roomGetExit((ROOM_DATA *)olcGetData(olc), dir));
      dirName = strdup(dirGetName(dir));
    }
    else if(roomGetExitSpecial((ROOM_DATA *)olcGetData(olc), arg)) {
      if(roomGetExitSpecial((ROOM_DATA *)olcGetData(olc), arg))
	exit = exitCopy(roomGetExitSpecial((ROOM_DATA *)olcGetData(olc), arg));
      dirName = strdup(arg);
    }
    else
      dirName = strdup(arg);

    // if the exit didn't exist already, create it
    if(exit == NULL)
      exit = newExit();

    olcSetNext(olc, newOLC(OLC_EXEDIT, EXEDIT_MAIN, exit, dirName));
    next_substate = REDIT_EXIT;
    free(dirName);
    break;
  }


    /******************************************************/
    /*                        DEFAULT                     */
    /******************************************************/
  default:
    log_string("ERROR: Performing redit with invalid substate.");
    send_to_socket(sock, "An error occured while you were in OLC.\r\n");
    socketSetOLC(sock, NULL);
    socketSetState(sock, STATE_PLAYING);
    return;
  }

  olcSetSubstate(olc, next_substate);
  if(next_substate == REDIT_MAIN)
    redit_menu(sock, olc);
  else if(next_substate == REDIT_RESET)
    redit_reset_menu(sock, olc);
}
