//*****************************************************************************
//
// edesc_edit.c
//
// the functions needed for extra description/extra description set editing
// in olc.
//
//***************************************************************************** 

#include <mud.h>
#include <socket.h>
#include <utils.h>
#include <extra_descs.h>

#include "olc.h"


#define EDITING_NEW_EDESC      "new_edesc"


//*****************************************************************************
//
// extra description sets
//
//*****************************************************************************
void edsedit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  EDESC_SET *set = (EDESC_SET *)olcGetData(olc);
  int i, entries = getEdescSetSize(set);

  send_to_socket(sock,
                 "\033[H\033[J"
		 "{gEntries: {c%d\r\n", entries);

  // print out each one
  for(i = 0; i < entries; i++) {
    EDESC_DATA *entry = getEdescNum(set, i);
    send_to_socket(sock,
		   "  {g%2d) {c%s\r\n", i, getEdescKeywords(entry));
  }

  send_to_socket(sock,
		 "\r\n"
		 "{gN) new entry\r\n"
		 "{gD) delete entry\r\n"
		 "Enter number to edit a specific entry\r\n"
		 "\r\n"
		 "{gEnter choice (Q to quit) : "
		 );
}


void edsedit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = EDSEDIT_MAIN;

  switch(*arg) {
  case 'q':
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = EDSEDIT_CONFIRM_SAVE;
    break;


  case 'n':
  case 'N': {
    olcSetNext(olc, newOLC(OLC_EDEDIT, EDEDIT_MAIN,
			   newEdesc("", ""), strdup(EDITING_NEW_EDESC)));
    next_substate = EDSEDIT_ENTRY;
    break;
  }

  case 'd':
  case 'D':
    send_to_socket(sock, "Which extra description do you want to delete : ");
    next_substate = EDSEDIT_DELETE;
    break;

  default:
    // see if maybe they're trying to choose a description to edit
    if(!isdigit(*arg))
      edsedit_menu(sock, olc);
    else {
      int num = atoi(arg);
      EDESC_DATA *edesc = getEdescNum((EDESC_SET *)olcGetData(olc), num);
      // if the one found is null, just show the menu
      if(edesc == NULL)
	edsedit_menu(sock, olc);
      else {
	olcSetNext(olc, newOLC(OLC_EDEDIT, EDEDIT_MAIN, 
			       copyEdesc(edesc), strdup(arg)));
	next_substate = EDSEDIT_ENTRY;
	break;
      }
    }
    break;
  }
  olcSetSubstate(olc, next_substate);
}


void edsedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = EDSEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case EDSEDIT_MAIN:
    edsedit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case EDSEDIT_CONFIRM_SAVE:
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
      next_substate = EDSEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case EDSEDIT_ENTRY:
    // save the changes we made
    if(olcGetSave(olcGetNext(olc))) {
      EDESC_SET  *set   = (EDESC_SET  *)olcGetData(olc);
      EDESC_DATA *entry = (EDESC_DATA *)olcGetData(olcGetNext(olc));

      if(!strcmp(EDITING_NEW_EDESC, (olcGetArgument(olcGetNext(olc)))))
	edescSetPut(set, copyEdesc(entry));
      else {
	int num = atoi(olcGetArgument(olcGetNext(olc)));
	copyEdescTo(entry, getEdescNum(set, num));
      }
    }
    olcSetNext(olc, NULL);
    next_substate = EDSEDIT_MAIN;
    break;

  case EDSEDIT_DELETE: {
    int num = atoi(arg);
    EDESC_DATA *entry = ((EDESC_DATA *)
			 getEdescNum((EDESC_SET *)olcGetData(olc), num));
    if(entry) {
      removeEdesc((EDESC_SET *)olcGetData(olc), entry);
      deleteEdesc(entry);
    }
    next_substate = EDSEDIT_MAIN;
    break;
  }

    /******************************************************/
    /*                        DEFAULT                     */
    /******************************************************/
  default:
    log_string("ERROR: Performing edesc edit with invalid substate.");
    send_to_socket(sock, "An error occured while you were in OLC.\r\n");
    socketSetOLC(sock, NULL);
    socketSetState(sock, STATE_PLAYING);
    return;
  }

  olcSetSubstate(olc, next_substate);
  if(next_substate == EDSEDIT_MAIN)
    edsedit_menu(sock, olc);
}


//*****************************************************************************
//
// single extra descriptions
//
//*****************************************************************************
void ededit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  EDESC_DATA *edesc = (EDESC_DATA *)olcGetData(olc);

  send_to_socket(sock,
                 "\033[H\033[J"
		 "{g1) Keywords\r\n"
		 "{c%s\r\n"
		 "{g2) Description\r\n"
		 "{c%s\r\n"
		 "{gEnter choice (Q to quit) : ",
		 getEdescKeywords(edesc), 
		 getEdescDescription(edesc)
		 );
}


void ededit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = EDEDIT_MAIN;

  switch(*arg) {
  case 'q':
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = EDEDIT_CONFIRM_SAVE;
    break;

  case '1':
    send_to_socket(sock, "Enter new keywords : ");
    next_substate = EDEDIT_KEYWORDS;
    break;

  case '2':
    send_to_socket(sock, "Enter new description\r\n");
    start_text_editor(sock, 
		      getEdescPtr((EDESC_DATA *)olcGetData(olc)),
		      MAX_BUFFER, EDITOR_MODE_NORMAL);
    next_substate = REDIT_MAIN;
    break;

  default:
    ededit_menu(sock, olc);
    break;
  }

  olcSetSubstate(olc, next_substate);
}


void ededit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = EDEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case EDEDIT_MAIN:
    ededit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case EDEDIT_CONFIRM_SAVE:
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
      next_substate = EDEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case EDEDIT_KEYWORDS:
    setEdescKeywords((EDESC_DATA *)olcGetData(olc), arg);
    break;


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
  if(next_substate == EDEDIT_MAIN)
    ededit_menu(sock, olc);
}
