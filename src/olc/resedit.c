//*****************************************************************************
//
// resedit.c
//
// the functions needed for editing room reset data in OLC
//
//*****************************************************************************

#include "../mud.h"
#include "../socket.h"
#include "../utils.h"
#include "../world.h"
#include "../object.h"
#include "../character.h"
#include "../room_reset.h"

#include "olc.h"

// if we are editing a new on/in/then, as opposed 
// to editing one that we already have
#define EDITING_NEW_RESET      "new_reset"


const char *write_reset_arg(int type, const char *arg) {
  static char buf[SMALL_BUFFER];
  OBJ_DATA *obj  = worldGetObj(gameworld, atoi(arg));
  CHAR_DATA *mob = worldGetMob(gameworld, atoi(arg));
  int pos        = atoi(arg);
  switch(type) { 
  case RESET_LOAD_OBJECT:
    sprintf(buf, "load %s", (obj ? objGetName(obj) : "{RNOTHING{c"));
    break;
  case RESET_LOAD_MOBILE:
    sprintf(buf, "load %s", (mob ? charGetName(mob) : "{RNOBODY{c"));
    break;
  case RESET_POSITION:
    sprintf(buf, "change position to %s", 
	    (pos < 0 || pos >= NUM_POSITIONS ? "{RNOTHING{c":posGetName(pos)));
    break;
  case RESET_FIND_MOBILE:
    sprintf(buf, "find %s", (mob ? charGetName(mob) : "{RNOBODY{c"));
    break;
  case RESET_FIND_OBJECT:
    sprintf(buf, "find %s", (obj ? objGetName(obj) : "{RNOTHING{c"));
    break;
  case RESET_PURGE_MOBILE:
    sprintf(buf, "purge %s", (mob ? charGetName(mob) : "{RNOBODY{c"));
    break;
  case RESET_PURGE_OBJECT:
    sprintf(buf, "purge %s", (obj ? objGetName(obj) : "{RNOTHING{c"));
    break;
  case RESET_OPEN:
    sprintf(buf, "open/unlock dir %s or container", arg);
    break;
  case RESET_CLOSE:
    sprintf(buf, "close/unlock dir %s or container", arg);
    break;
  case RESET_LOCK:
    sprintf(buf, "close/lock dir %s or container", arg);
    break;
  default:
    sprintf(buf, "UNFINISHED OLC");
    break;
  }
  return buf;
}


int indent_line(char *buf, int buflen, int indent) {
  if(indent > 0) {
    char fmt[20];
    sprintf(fmt, "%%-%ds", indent);
    return snprintf(buf, buflen, fmt, " ");
  }
  return 0;
}


int write_reset_buf(RESET_DATA *reset, char *buf, int buflen, int indent,
		    bool indent_first) {
  int i = 0;

  if(indent_first)
    i += indent_line(buf, buflen, indent);
  i += snprintf(buf+i, buflen-i,
		"{c%s with {w%d%% {cchance {w%d {ctime%s (max {w%d{c, rm. {w%d{c)\r\n",
		write_reset_arg(resetGetType(reset), resetGetArg(reset)),
		resetGetChance(reset),
		resetGetTimes(reset),
		(resetGetTimes(reset) == 1 ? "" : "s"),
		resetGetMax(reset),
		resetGetRoomMax(reset));

  // if we've got ONs, then print 'em all out as well
  if(listSize(resetGetOn(reset)) > 0) {
    i += indent_line(buf+i, buflen-i, indent);
    i += snprintf(buf+i, buflen-i, "{yon it: \r\n");
    LIST_ITERATOR *list_i = newListIterator(resetGetOn(reset));
    RESET_DATA     *next  = NULL;
    ITERATE_LIST(next, list_i)
      i += write_reset_buf(next, buf+i, buflen-i, indent+2, TRUE);
    deleteListIterator(list_i);
  }

  // if we've got INs, then print 'em all out as well
  if(listSize(resetGetIn(reset)) > 0) {
    i += indent_line(buf+i, buflen-i, indent);
    i += snprintf(buf+i, buflen-i, "{yin it: \r\n");
    LIST_ITERATOR *list_i = newListIterator(resetGetIn(reset));
    RESET_DATA     *next  = NULL;
    ITERATE_LIST(next, list_i)
      i += write_reset_buf(next, buf+i, buflen-i, indent+2, TRUE);
    deleteListIterator(list_i);
  }

  // if we've got THENs, print 'em all out as well
  if(listSize(resetGetThen(reset)) > 0) {
    i += indent_line(buf+i, buflen-i, indent);
    i += snprintf(buf+i, buflen-i, "{ywhen successful, also: \r\n");
    LIST_ITERATOR *list_i = newListIterator(resetGetThen(reset));
    RESET_DATA     *next  = NULL;
    ITERATE_LIST(next, list_i)
      i += write_reset_buf(next, buf+i, buflen-i, indent+2, TRUE);
    deleteListIterator(list_i);
  }
  return i;
}


const char *write_reset(RESET_DATA *reset, int indent, bool indent_first) {
  static char buf[MAX_BUFFER];
  write_reset_buf(reset, buf, MAX_BUFFER, indent, indent_first);
  return buf;
}


//
// returns the list of resets we're trying to edit, based on the substate
//
LIST *resedit_next_list(OLC_DATA *olc, int substate) {
  if(substate == RESEDIT_IN)
    return resetGetIn(olcGetData(olc));
  if(substate == RESEDIT_ON)
    return resetGetOn(olcGetData(olc));
  if(substate == RESEDIT_THEN)
    return resetGetThen(olcGetData(olc));
  // should never get this far
  return NULL;
}


//
// What is the state that we got here from?
//
int resedit_prev_state(int substate) {
  if(substate == RESEDIT_DELETE_IN || substate == RESEDIT_EDIT_IN)
    return RESEDIT_IN;
  if(substate == RESEDIT_DELETE_ON || substate == RESEDIT_EDIT_ON)
    return RESEDIT_ON;
  if(substate == RESEDIT_DELETE_THEN || substate == RESEDIT_EDIT_THEN)
    return RESEDIT_THEN;
  // should never get this far
  return RESEDIT_MAIN;
}


//
// returns the next editing state, after our current substate
//
int resedit_next_edit_substate(int substate) {
  if(substate == RESEDIT_IN)
    return RESEDIT_EDIT_IN;
  if(substate == RESEDIT_ON)
    return RESEDIT_EDIT_ON;
  if(substate == RESEDIT_THEN)
    return RESEDIT_EDIT_THEN;
  // we shouldn't get this far
  return RESEDIT_MAIN;
}


//
// returns the proper delete substate
//
int resedit_next_delete_substate(int substate) {
  if(substate == RESEDIT_IN)
    return RESEDIT_DELETE_IN;
  if(substate == RESEDIT_ON)
    return RESEDIT_DELETE_ON;
  if(substate == RESEDIT_THEN)
    return RESEDIT_DELETE_THEN;
  // we shouldn't get this far
  return RESEDIT_MAIN;
}


void resedit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  RESET_DATA *reset = olcGetData(olc);
  send_to_socket(sock,
                 "\033[H\033[J"
		 "{g1) Type:       {c%s\r\n"
		 "{g2) Times:      {c%d\r\n"
		 "{g3) Chance:     {c%d\r\n"
		 "{g4) Max:        {c%d\r\n"
		 "{g5) Room Max:   {c%d\r\n"
		 "{g6) Argument:   {c%s\r\n"
		 "{g7) Load on menu\r\n"
		 "{g8) Load in menu\r\n"
		 "{g9) Success menu\r\n"
		 "---------------------------------------------------------\r\n"
		 "%s",
		 resetTypeGetName(resetGetType(reset)),
		 resetGetTimes(reset),
		 resetGetChance(reset),
		 resetGetMax(reset),
		 resetGetRoomMax(reset),
		 resetGetArg(reset),
		 write_reset(reset, 0, FALSE)
		 );

  send_to_socket(sock, "\r\n{gEnter choice (Q to quit) : {n");
}


//
// display one of the child reset lists
//
void resedit_next_menu(SOCKET_DATA *sock, OLC_DATA *olc, int substate) {
  LIST *list           = resedit_next_list(olc, substate);
  LIST_ITERATOR *res_i = newListIterator(list);
  RESET_DATA    *reset = NULL;
  int count = 0;

  send_to_socket(sock,
                 "\033[H\033[J"
		 "{wCurrent reset commands:\r\n");

  ITERATE_LIST(reset, res_i) {
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



void show_reset_type_menu(SOCKET_DATA *sock) {
  int i;
  for(i = 0; i < NUM_RESETS; i++)
    send_to_socket(sock, "  %2d) %s\r\n", i, resetTypeGetName(i));
  send_to_socket(sock, "\r\nEnter choice : ");
}


//
// parse a command for editing one of our lists of reset commands
//
void resedit_next_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg, int substate) {
  int next_substate = substate;

  switch(toupper(*arg)) {
  case 'Q':
    next_substate = RESEDIT_MAIN;
    break;

  case 'N':
    olcSetNext(olc, newOLC(OLC_RESEDIT, RESEDIT_MAIN,
			   newReset(), strdup(EDITING_NEW_RESET)));
    next_substate = resedit_next_edit_substate(substate);
    break;

  case 'D':
    next_substate = resedit_next_delete_substate(substate);
    send_to_socket(sock, "Which entry would you like to delete : ");
    break;

  default:
    // see if maybe they're trying to choose a description to edit
    if(!isdigit(*arg))
      resedit_next_menu(sock, olc, substate);
    else {
      int num = atoi(arg);
      LIST *list = resedit_next_list(olc, substate);
      RESET_DATA *reset = listGet(list, num);
      // if the one found is null, just show the menu
      if(reset == NULL)
	resedit_next_menu(sock, olc, substate);
      else {
	olcSetNext(olc, newOLC(OLC_RESEDIT, RESEDIT_MAIN, 
			       resetCopy(reset), strdup(arg)));
	next_substate = resedit_next_edit_substate(substate);
	break;
      }
    }
    break;
  }

  olcSetSubstate(olc, next_substate);
  if(next_substate == RESEDIT_MAIN)
    resedit_menu(sock, olc);
}



void resedit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = RESEDIT_MAIN;
  switch(toupper(*arg)) {
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = RESEDIT_CONFIRM_SAVE;
    break;

  case '1':
    show_reset_type_menu(sock);
    next_substate = RESEDIT_TYPE;
    break;

  case '2':
    send_to_socket(sock, "How many times should it run (1 - 10) : ");
    next_substate = RESEDIT_TIMES;
    break;

  case '3':
    send_to_socket(sock, "What is its chance to happen (0 - 100) : ");
    next_substate = RESEDIT_CHANCE;
    break;

  case '4':
    send_to_socket(sock, "What is the max that can exist in game (0 = no limit) : ");
    next_substate = RESEDIT_MAX;
    break;

  case '5':
    send_to_socket(sock, "What is the max that can exist in room (0 = no limit) : ");
    next_substate = RESEDIT_ROOM_MAX;
    break;

  case '6':
    send_to_socket(sock, "What is your argument : ");
    next_substate = RESEDIT_ARG;
    break;

  case '7':
    next_substate = RESEDIT_ON;
    resedit_next_menu(sock, olc, next_substate);
    break;

  case '8':
    next_substate = RESEDIT_IN;
    resedit_next_menu(sock, olc, next_substate);
    break;

  case '9':
    next_substate = RESEDIT_THEN;
    resedit_next_menu(sock, olc, next_substate);
    break;

  default:
    resedit_menu(sock, olc);
    break;
  }

  olcSetSubstate(olc, next_substate);
}


void resedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = RESEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case RESEDIT_MAIN:
    resedit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*               SUBSTATE LOOPS MENU                  */
    /******************************************************/
  case RESEDIT_ON:
  case RESEDIT_IN:
  case RESEDIT_THEN:
    resedit_next_loop(sock, olc, arg, olcGetSubstate(olc));
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case RESEDIT_CONFIRM_SAVE:
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
      next_substate = RESEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case RESEDIT_TYPE:
    resetSetType(olcGetData(olc), MAX(0, MIN(NUM_RESETS, atoi(arg))));
    // reset all of the data to defaults
    resetSetArg(olcGetData(olc), "");
    resetSetChance(olcGetData(olc), 100);
    resetSetMax(olcGetData(olc), 0);
    resetSetRoomMax(olcGetData(olc), 0);
    resetSetTimes(olcGetData(olc), 1);
    break;

  case RESEDIT_TIMES:
    resetSetTimes(olcGetData(olc), MAX(1, MIN(10, atoi(arg))));
    break;

  case RESEDIT_CHANCE:
    resetSetChance(olcGetData(olc), MAX(1, MIN(100, atoi(arg))));
    break;

  case RESEDIT_MAX:
    resetSetMax(olcGetData(olc), MAX(0, MIN(1000, atoi(arg))));
    break;

  case RESEDIT_ROOM_MAX:
    resetSetRoomMax(olcGetData(olc), MAX(0, MIN(1000, atoi(arg))));
    break;

  case RESEDIT_ARG:
    resetSetArg(olcGetData(olc), arg);
    break;


    /******************************************************/
    /*                  EDIT CHILD LISTS                  */
    /******************************************************/
  case RESEDIT_EDIT_ON:
  case RESEDIT_EDIT_IN:
  case RESEDIT_EDIT_THEN:
    // save the changes we made
    if(olcGetSave(olcGetNext(olc))) {
      LIST*list =resedit_next_list(olc,resedit_prev_state(olcGetSubstate(olc)));
      RESET_DATA *reset = olcGetData(olcGetNext(olc));
      if(!strcmp(EDITING_NEW_RESET, olcGetArgument(olcGetNext(olc))))
	listQueue(list, resetCopy(reset));
      else {
	int num = atoi(olcGetArgument(olcGetNext(olc)));
	resetCopyTo(reset, listGet(list, num));
      }
    }
    next_substate = resedit_prev_state(olcGetSubstate(olc));
    break;

  case RESEDIT_DELETE_ON:
  case RESEDIT_DELETE_IN:
  case RESEDIT_DELETE_THEN: {
    LIST *list = resedit_next_list(olc,resedit_prev_state(olcGetSubstate(olc)));
    RESET_DATA *reset = (isdigit(*arg) ? listGet(list, atoi(arg)) : NULL);
    if(reset != NULL) {
      listRemove(list, reset);
      deleteReset(reset);
    }
    next_substate = resedit_prev_state(olcGetSubstate(olc));
    break;
  }


    /******************************************************/
    /*                        DEFAULT                     */
    /******************************************************/
  default:
    log_string("ERROR: Performing resedit with invalid substate.");
    send_to_socket(sock, "An error occured while you were in OLC.\r\n");
    socketSetOLC(sock, NULL);
    socketSetState(sock, STATE_PLAYING);
    return;
  }

  olcSetSubstate(olc, next_substate);
  if(next_substate == RESEDIT_MAIN)
    resedit_menu(sock, olc);
  else if(next_substate == RESEDIT_ON || next_substate == RESEDIT_IN ||
	  next_substate == RESEDIT_THEN)
    resedit_next_menu(sock, olc, next_substate);
}
