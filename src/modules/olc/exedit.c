//*****************************************************************************
//
// exedit.c
//
// contains all of the functions needed for editing exits.
//
//*****************************************************************************

#include <mud.h>
#include <exit.h>
#include <socket.h>
#include <utils.h>

#include "olc.h"


void exedit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  EXIT_DATA *exit = (EXIT_DATA *)olcGetData(olc);

  send_to_socket(sock,
                 "\033[H\033[J"
		 "{g[{c%s{g]\r\n"
		 "{g1) Door name\r\n"
		 "{c%s\r\n"
		 "{g2) Door keywords\r\n"
		 "{c%s\r\n"
		 "{g3) Leave message\r\n"
		 "{c%s\r\n"
		 "{g4) Enter message\r\n"
		 "{c%s\r\n"
		 "{g5) Description\r\n"
		 "{c%s\r\n"
		 "{g6) Exits to:   {y[{c%6d{y]\r\n"
		 "{g7) Closable:   {y[{c%6s{y]\r\n"
		 "{g8) Key vnum:   {y[{c%6d{y]\r\n"
		 "{g9) Pick diff:  {y[{c%6d{y]\r\n"
		 "{g0) Spot diff:  {y[{c%6d{y]\r\n"
		 "\r\n"
		 "{gD) Delete exit\r\n"
		 "\r\n"
		 "{gEnter choice (Q to quit) : {n",
		 olcGetArgument(olc),
		 (*exitGetName(exit) ? exitGetName(exit) : "<NONE>"),
		 (*exitGetKeywords(exit) ? exitGetKeywords(exit) : "<NONE>"),
		 (*exitGetSpecLeave(exit) ? exitGetSpecLeave(exit):"<DEFAULT>"),
		 (*exitGetSpecEnter(exit) ? exitGetSpecEnter(exit):"<DEFAULT>"),
		 exitGetDesc(exit),
		 exitGetTo(exit),
		 (exitIsClosable(exit) ? "Yes" : "No" ),
		 exitGetKey(exit),
		 exitGetPickLev(exit),
		 exitGetHidden(exit)
		 );
}



void exedit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = EXEDIT_MAIN;
  EXIT_DATA *exit = (EXIT_DATA *)olcGetData(olc);

  switch(toupper(*arg)) {
  case 'q':
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = EXEDIT_CONFIRM_SAVE;
    break;

  case 'd':
  case 'D':
    send_to_socket(sock, "Are you sure you want to delete this exit (Y/N) : ");
    next_substate = EXEDIT_CONFIRM_DELETE;
    break;

  case '1':
    send_to_socket(sock, "Enter new name : ");
    next_substate = EXEDIT_NAME;
    break;

  case '2':
    send_to_socket(sock, "Enter new keywords : ");
    next_substate = EXEDIT_KEYWORDS;
    break;

  case '3':
    send_to_socket(sock,"Enter new leave message ($n for character's name) : ");
    next_substate = EXEDIT_LEAVE;
    break;

  case '4':
    send_to_socket(sock,"Enter new enter message ($n for character's name) : ");
    next_substate = EXEDIT_ENTER;
    break;

  case '5':
    send_to_socket(sock, "Enter new description\r\n");
    start_text_editor(sock,
		      exitGetDescPtr(exit),
		      MAX_BUFFER, EDITOR_MODE_NORMAL);
    break;

  case '6':
    send_to_socket(sock, "Where does this exit lead to : ");
    next_substate = EXEDIT_TO;
    break;

  case '7':
    // just toggle it instead of prompting
    exitSetClosable(exit, (exitIsClosable(exit) ? FALSE : TRUE));
    next_substate = EXEDIT_MAIN;
    // since it's normally assumed we show a prompt, 
    // we have to redisplay the main menu
    exedit_menu(sock, olc);

    //    send_to_socket(sock, "Is the exit openable and closable (Y/N) : ");
    //    next_substate = EXEDIT_CLOSABLE;
    break;

  case '8':
    send_to_socket(sock, "Enter the vnum of the key (-1 for none) : ");
    next_substate = EXEDIT_KEY;
    break;

  case '9':
    send_to_socket(sock, "How difficult is the lock to pick : ");
    next_substate = EXEDIT_PICK_DIFF;
    break;

  case '0':
    send_to_socket(sock, "How difficult is the exit to spot : ");
    next_substate = EXEDIT_SPOT_DIFF;
    break;

  default:
    exedit_menu(sock, olc);
    break;
  }

  olcSetSubstate(olc, next_substate);
}




//
// The entry loop for exedit. Figures out what substate we're
// in, and then enters into the appropriate subloop if possible,
// or sets a value based on arg if there is no subloop
//
void exedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  EXIT_DATA *exit = (EXIT_DATA *)olcGetData(olc);
  int next_substate = EXEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case EXEDIT_MAIN:
    exedit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case EXEDIT_CONFIRM_SAVE:
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
      next_substate = EXEDIT_CONFIRM_SAVE;
      break;
    }


    /******************************************************/
    /*                   CONFIRM DELETE                   */
    /******************************************************/
  case EXEDIT_CONFIRM_DELETE:
    switch(*arg) {
    case 'y':
    case 'Y':
      olcSetData(olc, NULL);
      olcSetSave(olc, TRUE);
      olcSetComplete(olc, TRUE);
      return;
    case 'n':
    case 'N':
      break;
    default:
      send_to_socket(sock, "Please enter Y or N : ");
      next_substate = EXEDIT_CONFIRM_DELETE;
      break;
    }


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case EXEDIT_NAME:
    exitSetName(exit, arg);
    break;
  case EXEDIT_KEYWORDS:
    exitSetKeywords(exit, arg);
    break;
  case EXEDIT_LEAVE:
    exitSetSpecLeave(exit, arg);
    break;
  case EXEDIT_ENTER:
    exitSetSpecEnter(exit, arg);
    break;
  case EXEDIT_TO:
    exitSetTo(exit, atoi(arg));
    break;
  case EXEDIT_SPOT_DIFF:
    exitSetHidden(exit, MAX(0, atoi(arg)));
    break;
  case EXEDIT_PICK_DIFF:
    exitSetPickLev(exit, MAX(0, atoi(arg)));
    break;
  case EXEDIT_KEY:
    exitSetKey(exit, MAX(-1, atoi(arg)));
    break;
    /*
     * closable is now just toggled when
     * we choose to edit its closable status
     *
  case EXEDIT_CLOSABLE:
    exitSetClosable(exit, (toupper(*arg) == 'Y'));
    break;
    */

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
  if(next_substate == EXEDIT_MAIN)
    exedit_menu(sock, olc);
}
