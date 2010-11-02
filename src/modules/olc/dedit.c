//*****************************************************************************
//
// dialog_edit.c
//
// the functions needed for dialog editing in olc.
//
//*****************************************************************************

#include <mud.h>
#include <socket.h>
#include <utils.h>
#include <dialog.h>

#include "olc.h"


#define EDITING_NEW_RESPONSE      "new_response"


//*****************************************************************************
//
// extra description sets
//
//*****************************************************************************
void dedit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  DIALOG_DATA *dialog = (DIALOG_DATA *)olcGetData(olc);
  int i, entries = dialogGetSize(dialog);
  int half_entries = entries/2 + (entries % 2 == 1);
  // entries are printed side by side... these are used to hold 'em
  char left_buf[SMALL_BUFFER];
  char right_buf[SMALL_BUFFER];


  send_to_socket(sock,
                 "\033[H\033[J"
		 "{gN) Dialog name\r\n"
		 "{c%s\r\n"
		 "{gG) Greet/Approach message\r\n"
		 "{c%s\r\n"
		 "\r\n"
		 "{gResponse Entries: {c%d\r\n", 
		 dialogGetName(dialog), 
		 (*dialogGetGreet(dialog) ? dialogGetGreet(dialog) : "<NONE>"), 
		 entries);

  // print out responses, two to a line
  for(i = 0; i < half_entries; i++) {
    RESPONSE_DATA *left_entry = dialogGetNum(dialog, i);
    sprintf(left_buf, "  {g%2d) {c%s", i, responseGetKeywords(left_entry));
    if(i + half_entries < entries) {
      RESPONSE_DATA *right_entry = dialogGetNum(dialog, i + half_entries);
      sprintf(right_buf, "  {g%2d) {c%s", 
	      i + half_entries, responseGetKeywords(right_entry));
    }
    else
      *right_buf = '\0';

    send_to_socket(sock, "%-35s %s\r\n", left_buf, right_buf);
  }

  send_to_socket(sock,
		 "\r\n"
		 "{gA) Add response\r\n"
		 "{gR) Remove response\r\n"
		 "Enter number to edit a specific entry\r\n"
		 "\r\n"
		 "{gEnter choice (Q to quit) : "
		 );
}


void dedit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = DEDIT_MAIN;

  switch(toupper(*arg)) {
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = DEDIT_CONFIRM_SAVE;
    break;

  case 'A': {
    olcSetNext(olc, newOLC(OLC_RESPEDIT, RESPEDIT_MAIN,
			   newResponse("", ""), strdup(EDITING_NEW_RESPONSE)));
    next_substate = DEDIT_ENTRY;
    break;
  }

  case 'R':
    send_to_socket(sock, "Which response do you want to delete : ");
    next_substate = DEDIT_DELETE;
    break;

  case 'N':
    send_to_socket(sock, "Enter new name : ");
    next_substate = DEDIT_NAME;
    break;

  case 'G':
    send_to_socket(sock, "Enter new greeting : ");
    next_substate = DEDIT_GREET;
    break;

  default:
    // see if maybe they're trying to choose a description to edit
    if(!isdigit(*arg))
      dedit_menu(sock, olc);
    else {
      int num = atoi(arg);
      RESPONSE_DATA *resp = dialogGetNum((DIALOG_DATA *)olcGetData(olc), num);
      // if the one found is null, just show the menu
      if(resp == NULL)
	dedit_menu(sock, olc);
      else {
	olcSetNext(olc, newOLC(OLC_RESPEDIT, RESPEDIT_MAIN, 
			       responseCopy(resp), strdup(arg)));
	next_substate = DEDIT_ENTRY;
	break;
      }
    }
    break;
  }
  olcSetSubstate(olc, next_substate);
}


void dedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = DEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case DEDIT_MAIN:
    dedit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case DEDIT_CONFIRM_SAVE:
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
      next_substate = DEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case DEDIT_ENTRY:
    // save the changes we made
    if(olcGetSave(olcGetNext(olc))) {
      DIALOG_DATA  *dialog   = (DIALOG_DATA *)olcGetData(olc);
      RESPONSE_DATA *entry   = (RESPONSE_DATA *)olcGetData(olcGetNext(olc));

      if(!strcmp(EDITING_NEW_RESPONSE, (olcGetArgument(olcGetNext(olc)))))
	dialogPut(dialog, responseCopy(entry));
      else {
	int num = atoi(olcGetArgument(olcGetNext(olc)));
	responseCopyTo(entry, dialogGetNum(dialog, num));
      }
    }
    olcSetNext(olc, NULL);
    next_substate = DEDIT_MAIN;
    break;

  case DEDIT_DELETE: {
    int num = atoi(arg);
    RESPONSE_DATA *entry = ((RESPONSE_DATA *)
			    dialogGetNum((DIALOG_DATA *)olcGetData(olc), num));
    if(entry) {
      dialogRemove((DIALOG_DATA *)olcGetData(olc), entry);
      deleteResponse(entry);
    }
    next_substate = DEDIT_MAIN;
    break;
  }

  case DEDIT_NAME:
    dialogSetName((DIALOG_DATA *)olcGetData(olc), arg);
    next_substate = DEDIT_MAIN;
    break;

  case DEDIT_GREET:
    dialogSetGreet((DIALOG_DATA *)olcGetData(olc), arg);
    next_substate = DEDIT_MAIN;
    break;

    /******************************************************/
    /*                        DEFAULT                     */
    /******************************************************/
  default:
    log_string("ERROR: Performing dedit with invalid substate.");
    send_to_socket(sock, "An error occured while you were in OLC.\r\n");
    socketSetOLC(sock, NULL);
    socketSetState(sock, STATE_PLAYING);
    return;
  }

  olcSetSubstate(olc, next_substate);
  if(next_substate == DEDIT_MAIN)
    dedit_menu(sock, olc);
}


//*****************************************************************************
//
// single response editing
//
//*****************************************************************************
void respedit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  RESPONSE_DATA *response = (RESPONSE_DATA *)olcGetData(olc);

  send_to_socket(sock,
                 "\033[H\033[J"
		 "{g1) Keywords\r\n"
		 "{c%s\r\n"
		 "{g2) Message\r\n"
		 "{c%s\r\n"
		 "{gEnter choice (Q to quit) : ",
		 responseGetKeywords(response), 
		 responseGetMessage(response)
		 );
}


void respedit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = RESPEDIT_MAIN;

  switch(*arg) {
  case 'q':
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = RESPEDIT_CONFIRM_SAVE;
    break;

  case '1':
    send_to_socket(sock, "Enter new keywords : ");
    next_substate = RESPEDIT_KEYWORDS;
    break;

  case '2':
    send_to_socket(sock, "Enter new message : ");
    next_substate = RESPEDIT_MESSAGE;
    break;

  default:
    respedit_menu(sock, olc);
    break;
  }

  olcSetSubstate(olc, next_substate);
}


void respedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = RESPEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case RESPEDIT_MAIN:
    respedit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case RESPEDIT_CONFIRM_SAVE:
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
      next_substate = RESPEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case RESPEDIT_KEYWORDS:
    responseSetKeywords((RESPONSE_DATA *)olcGetData(olc), arg);
    break;
  case RESPEDIT_MESSAGE:
    responseSetMessage((RESPONSE_DATA *)olcGetData(olc), arg);
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
  if(next_substate == RESPEDIT_MAIN)
    respedit_menu(sock, olc);
}
