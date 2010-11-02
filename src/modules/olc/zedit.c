//*****************************************************************************
//
// zedit.c
//
// Contains all of the functions needed for online editing of zones.
//
//*****************************************************************************

#include <mud.h>
#include <world.h>
#include <zone.h>
#include <socket.h>
#include <utils.h>

#include "olc.h"


void zedit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  ZONE_DATA *zone = (ZONE_DATA *)olcGetData(olc);

  send_to_socket(sock,
                 "\033[H\033[J"
		 "{g[{c%d{g]\r\n"
		 "{g1) Name\r\n"
		 "{c%s\r\n"
		 "{g2) Editors\r\n"
		 "{c%s\r\n"
		 "{g3) Reset timer: {c%d {gmins\r\n"
		 "{g4) Description\r\n"
		 "{c%s\r\n"
		 "\r\n"
		 "{gEnter choice (Q to quit) : {n",
		 zoneGetVnum(zone),
		 zoneGetName(zone),
		 zoneGetEditors(zone),
		 zoneGetPulseTimer(zone),
		 zoneGetDesc(zone)
		 );
}


void zedit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = ZEDIT_MAIN;

  switch(*arg) {
  case 'q':
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = ZEDIT_CONFIRM_SAVE;
    break;

  case '1':
    send_to_socket(sock, "Enter new name : ");
    next_substate = ZEDIT_NAME;
    break;

  case '2':
    send_to_socket(sock, "Enter a new list of editors : ");
    next_substate = ZEDIT_EDITORS;
    break;

  case '3':
    send_to_socket(sock, "Enter reset timer (-1 for no resets) : ");
    next_substate = ZEDIT_RESET;
    break;

  case '4':
    send_to_socket(sock, "Enter new description\r\n");
    start_text_editor(sock, 
		      zoneGetDescPtr((ZONE_DATA *)olcGetData(olc)),
		      MAX_BUFFER, EDITOR_MODE_NORMAL);
    next_substate = ZEDIT_MAIN;
    break;

  default:
    zedit_menu(sock, olc);
    break;
  }

  olcSetSubstate(olc, next_substate);
}



void zedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  ZONE_DATA *zone = (ZONE_DATA *)olcGetData(olc);
  int next_substate = ZEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case ZEDIT_MAIN:
    zedit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case ZEDIT_CONFIRM_SAVE:
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
      next_substate = ZEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case ZEDIT_NAME:
    zoneSetName(zone, arg);
    break;

  case ZEDIT_EDITORS:
    zoneSetEditors(zone, arg);
    break;

  case ZEDIT_RESET:
    zoneSetPulseTimer(zone, atoi(arg));
    break;

    /******************************************************/
    /*                        DEFAULT                     */
    /******************************************************/
  default:
    log_string("ERROR: Performing zedit with invalid substate.");
    send_to_socket(sock, "An error occured while you were in OLC.\r\n");
    socketSetOLC(sock, NULL);
    socketSetState(sock, STATE_PLAYING);
    return;
  }

  olcSetSubstate(olc, next_substate);
  if(next_substate == ZEDIT_MAIN)
    zedit_menu(sock, olc);
}
