//*****************************************************************************
//
// ssedit.c
//
// the functions used for editing room/obj/mob script sets.
//
//*****************************************************************************

#include <mud.h>
#include <socket.h>
#include <world.h>
#include <utils.h>

#include "olc.h"


#ifdef MODULE_SCRIPTS
#include "../scripts/script.h"
#include "../scripts/script_set.h"


void ssedit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  LIST *scripts = scriptSetList((SCRIPT_SET *)olcGetData(olc), SCRIPT_TYPE_ANY);
  SCRIPT_DATA *script = NULL;

  send_to_socket(sock,
                 "\033[H\033[J"
		 "{g[{c%s{g]\r\n\r\n", 
		 olcGetArgument(olc));

  // show all the scripts attached
  while((script = listPop(scripts)) != NULL)
    send_to_socket(sock,
		   "  {y[{c%4d{y]{w %-20s {c%s\r\n",
		   scriptGetVnum(script), 
		   scriptTypeName(scriptGetType(script)),
		   scriptGetName(script));
  deleteList(scripts);

  send_to_socket(sock,
		 "\r\n"
		 "{cA{g) add new script\r\n"
		 "{cR{g) remove script\r\n"
		 "\r\n"
		 "Enter your choice (Q to quit) : ");
}



void ssedit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = SSEDIT_MAIN;

  switch(toupper(*arg)) {
  case 'q':
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = SSEDIT_CONFIRM_SAVE;
    break;

  case 'a':
  case 'A':
    send_to_socket(sock, "Which script would you like to add (-1 for none) : ");
    next_substate = SSEDIT_ADD;
    break;

  case 'r':
  case 'R':
    send_to_socket(sock, "Which script would you like to remove (-1 for none) : ");
    next_substate = SSEDIT_REMOVE;
    break;

  default:
    ssedit_menu(sock, olc);
    break;
  }

  olcSetSubstate(olc, next_substate);
}


void ssedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  SCRIPT_SET *set = (SCRIPT_SET *)olcGetData(olc);
  int next_substate = SSEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case SSEDIT_MAIN:
    ssedit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case SSEDIT_CONFIRM_SAVE:
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
      next_substate = SSEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case SSEDIT_ADD: {
    // check to make sure the script exists
    SCRIPT_DATA *script = worldGetScript(gameworld, atoi(arg));
    if(script)
      scriptSetAdd(set, scriptGetVnum(script)); 
  }
    break;

  case SSEDIT_REMOVE:
    scriptSetRemove(set, atoi(arg));
    break;


    /******************************************************/
    /*                        DEFAULT                     */
    /******************************************************/
  default:
    log_string("ERROR: Performing ssedit with invalid substate.");
    send_to_socket(sock, "An error occured while you were in OLC.\r\n");
    socketSetOLC(sock, NULL);
    socketSetState(sock, STATE_PLAYING);
    return;
  }


  /********************************************************/
  /*                 DISPLAY NEXT MENU HERE               */
  /********************************************************/
  ssedit_menu(sock, olc);
  olcSetSubstate(olc, next_substate);
}

#endif // MODULE_SCRIPTS
