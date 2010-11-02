//*****************************************************************************
//
// scedit.c
//
// These are the functions used for editing scripts online
//
//*****************************************************************************

#include "../mud.h"
#include "../socket.h"
#include "../utils.h"

#include "olc.h"

#ifdef MODULE_SCRIPTS
#include "../scripts/script.h"

void scedit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  SCRIPT_DATA *script = (SCRIPT_DATA *)olcGetData(olc);
  send_to_socket(sock,
                 "\033[H\033[J"
		 "{g[{c%d{g]\r\n"
		 "{g1) Name         : {c%s\r\n"
		 "{g2) Script type  : {c%s\r\n"
		 "{g3) Arguments    : {c%s\r\n"
		 "{g4) Num. Argument: {c%d\r\n"
		 "{g5) Script Code\r\n",
		 scriptGetVnum(script),
		 scriptGetName(script),
		 scriptTypeName(scriptGetType(script)),
		 (*scriptGetArgs(script) ? scriptGetArgs(script) : "<NONE>"),
		 scriptGetNumArg(script));

  script_display(sock, scriptGetCode(script), FALSE);

  send_to_socket(sock,
		 "\r\n"
		 "{gEnter choice (Q to quit) : {n");

}


//
// Show the different script types to the character
//
void show_script_type_menu(SOCKET_DATA *sock) {
  int i;
  for(i = 0; i < NUM_SCRIPTS; i++)
    send_to_socket(sock, "  {c%2d{g) %s\r\n", i, scriptTypeName(i));
  send_to_socket(sock, "\r\n");
}


void scedit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = SCEDIT_MAIN;

  switch(toupper(*arg)) {
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = SCEDIT_CONFIRM_SAVE;
    break;

  case '1':
    send_to_socket(sock, "Enter new name : ");
    next_substate = SCEDIT_NAME;
    break;

  case '2':
    show_script_type_menu(sock);
    send_to_socket(sock, "Enter new type : ");
    next_substate = SCEDIT_TYPE;
    break;

  case '3':
    send_to_socket(sock, "Enter new arguments : ");
    next_substate = SCEDIT_ARGS;
    break;

  case '4':
    next_substate = SCEDIT_NARG;
    switch(scriptGetType(olcGetData(olc))) {
      // 0 = triggers always
      // 1 = triggers if the scriptor can see the char
    case SCRIPT_TYPE_GIVE:
    case SCRIPT_TYPE_ENTER:
    case SCRIPT_TYPE_EXIT:
      next_substate = SCEDIT_NARG;
      send_to_socket(sock,
		     "If the scriptor is a mob:\r\n"
		     "  0 = always triggers\r\n"
		     "  1 = triggers if the scriptor can see the char\r\n"
		     "\r\n"
		     "Enter choice : ");
      break;

    case SCRIPT_TYPE_COMMAND:
      next_substate = SCEDIT_NARG;
      send_to_socket(sock,
		     "Control for the actual MUD command:\r\n"
		     "  0 = follow through with the MUD command\r\n"
		     "  1 = cancel the MUD command.\r\n"
		     "\r\n"
		     "Enter choice : ");
      break;

    default:
      next_substate = SCEDIT_MAIN;
      send_to_socket(sock, 
		     "This script type does not use numeric arguments.\r\n"
		     "Enter choice (Q to quit) : ");
      break;
    }
    break;

  case '5':
    send_to_socket(sock, "Editing the script code\r\n");
    start_text_editor(sock, 
		      scriptGetCodePtr((SCRIPT_DATA *)olcGetData(olc)),
		      MAX_SCRIPT, EDITOR_MODE_SCRIPT);
    break;

  default:
    scedit_menu(sock, olc);
    break;
  }

  olcSetSubstate(olc, next_substate);
}



void scedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  SCRIPT_DATA *script = (SCRIPT_DATA *)olcGetData(olc);
  int next_substate = SCEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case SCEDIT_MAIN:
    scedit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case SCEDIT_CONFIRM_SAVE:
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
      next_substate = SCEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case SCEDIT_NAME:
    scriptSetName(script, arg);
    break;

  case SCEDIT_ARGS:
    scriptSetArgs(script, arg);
    break;

  case SCEDIT_NARG:
    switch(scriptGetType(script)) {
      // 0 = triggers always
      // 1 = triggers if the scriptor can see the char
    case SCRIPT_TYPE_GIVE:
    case SCRIPT_TYPE_ENTER:
    case SCRIPT_TYPE_EXIT:
      scriptSetNumArg(script, MIN(1, MAX(0, atoi(arg))));
      break;

      // 0 = follow through with normal command
      // 1 = cancel normal command
    case SCRIPT_TYPE_COMMAND:
      scriptSetNumArg(script, MIN(1, MAX(0, atoi(arg))));
      break;

    default:
      break;
    }
    break;

  case SCEDIT_TYPE: {
    int num = atoi(arg);
    if(num < 0 || num >= NUM_SCRIPTS) {
      show_script_type_menu(sock);
      next_substate = SCEDIT_TYPE;
    }
    else {
      scriptSetType(script, num);
      // reset arguments
      if(scriptGetArgs(script))
	scriptSetArgs(script, "");
      scriptSetNumArg(script, 0);
    }
    break;
  }

    /******************************************************/
    /*                        DEFAULT                     */
    /******************************************************/
  default:
    log_string("ERROR: Performing scedit with invalid substate.");
    send_to_socket(sock, "An error occured while you were in OLC.\r\n");
    socketSetOLC(sock, NULL);
    socketSetState(sock, STATE_PLAYING);
    return;
  }


  /********************************************************/
  /*                 DISPLAY NEXT MENU HERE               */
  /********************************************************/
  olcSetSubstate(olc, next_substate);
  if(next_substate == SCEDIT_MAIN)
    scedit_menu(sock, olc);
}
#endif // MODULE_SCRIPTS
