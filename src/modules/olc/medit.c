//*****************************************************************************
//
// medit.c
//
// Contains all of the functions needed for online editing of mobiles.
//
//*****************************************************************************

#include <mud.h>
#include <world.h>
#include <character.h>
#include <socket.h>
#include <utils.h>
#include <races.h>
#include <dialog.h>

#include "olc.h"

#ifdef MODULE_SCRIPTS
#include "../scripts/script_set.h"
#include "../scripts/script.h"
#endif


void medit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  CHAR_DATA *mob = (CHAR_DATA *)olcGetData(olc);

  send_to_socket(sock,
                 "\033[H\033[J"
		 "{g[{c%d{g]\r\n"
		 "{g1) Name\r\n"
		 "{c%s\r\n"
		 "{g2) Name for multiple occurances\r\n"
		 "{c%s\r\n"
		 "{g3) Keywords\r\n"
		 "{c%s\r\n"
		 "{g4) Room description\r\n"
		 "{c%s\r\n"
		 "{g5) Room description for multiple occurances\r\n"
		 "{c%s\r\n"
		 "{g6) Description\r\n"
		 "{c%s\r\n"
#ifdef MODULE_SCRIPTS
		 "{gS) Script menu\r\n"
#endif
		 "{gR) Change race  {y[{c%8s{y]\r\n"
		 "{gD) Dialog       {y[{c%8d{y]  {w%s\r\n",
		 charGetVnum(mob),
		 charGetName(mob),
		 charGetMultiName(mob),
		 charGetKeywords(mob),
		 charGetRdesc(mob),
		 charGetMultiRdesc(mob),
		 charGetDesc(mob),
		 raceGetName(charGetRace(mob)),
		 charGetDialog(mob),
		 (worldGetDialog(gameworld, charGetDialog(mob)) ?
		  dialogGetName(worldGetDialog(gameworld, 
					       charGetDialog(mob))) : "")
		 );

  send_to_socket(sock,
		 "\r\n"
		 "{gEnter choice (Q to quit) : {n"
		 );
}

void medit_race_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  int i;
  for(i = 0; i < NUM_RACES; i++)
    send_to_socket(sock, "%2d) %-20s%s", 
		   i, raceGetName(i), (i % 3 == 2 ?"\r\n":""));
  if(i % 3 != 0)
    send_to_socket(sock, "\r\n");
  send_to_socket(sock,
		 "\r\nPlease select a race: ");
}

void medit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = MEDIT_MAIN;

  switch(*arg) {
  case 'q':
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = MEDIT_CONFIRM_SAVE;
    break;

  case '1':
    send_to_socket(sock, "Enter name : ");
    next_substate = MEDIT_NAME;
    break;

  case '2':
    send_to_socket(sock, "Enter name for multiple occurances : ");
    next_substate = MEDIT_MULTI_NAME;
    break;

  case '3':
    send_to_socket(sock, "Enter keywords : ");
    next_substate = MEDIT_KEYWORDS;
    break;

  case '4':
    send_to_socket(sock, "Enter room description : ");
    next_substate = MEDIT_RDESC;
    break;

  case '5':
    send_to_socket(sock, "Enter room description for multiple occurances : ");
    next_substate = MEDIT_MULTI_RDESC;
    break;

  case '6':
    send_to_socket(sock, "Enter description\r\n");
    start_text_editor(sock, 
		      charGetDescPtr((CHAR_DATA *)olcGetData(olc)),
		      MAX_BUFFER, EDITOR_MODE_NORMAL);
    next_substate = MEDIT_MAIN;
    break;

  case 'r':
  case 'R':
    medit_race_menu(sock, olc);
    next_substate = MEDIT_RACE;
    break;

  case 'd':
  case 'D':
    send_to_socket(sock, "Enter new dialog vnum (-1 for none) : ");
    next_substate = MEDIT_DIALOG;
    break;

#ifdef MODULE_SCRIPTS
  case 's':
  case 'S':
    olcSetNext(olc, newOLC(OLC_SSEDIT, SSEDIT_MAIN,
			   copyScriptSet(charGetScripts((CHAR_DATA *)olcGetData(olc))), charGetName((CHAR_DATA *)olcGetData(olc))));
    next_substate = MEDIT_SCRIPTS;
    break;
#endif

  default:
    medit_menu(sock, olc);
    break;
  }

  olcSetSubstate(olc, next_substate);
}



void medit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  CHAR_DATA *mob = (CHAR_DATA *)olcGetData(olc);
  int next_substate = MEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case MEDIT_MAIN:
    medit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case MEDIT_CONFIRM_SAVE:
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
      next_substate = MEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case MEDIT_NAME:
    charSetName(mob, arg);
    break;

  case MEDIT_MULTI_NAME:
    charSetMultiName(mob, arg);
    break;

  case MEDIT_KEYWORDS:
    charSetKeywords(mob, arg);
    break;

  case MEDIT_RDESC:
    charSetRdesc(mob, arg);
    break;

  case MEDIT_MULTI_RDESC:
    charSetMultiRdesc(mob, arg);
    break;

  case MEDIT_RACE: {
    int race = RACE_NONE;
    if(isdigit(*arg)) race = atoi(arg);
    else              race = raceGetNum(arg);
    if(race == RACE_NONE) {
      send_to_socket(sock, "Invalid race! Try again: ");
      next_substate = MEDIT_RACE;
    }
    else {
      charSetRace(mob, race);
      charResetBody(mob);
    }
    break;
  }

  case MEDIT_DIALOG: {
    int vnum = (*arg ? atoi(arg) : NOTHING);
    DIALOG_DATA *dialog = worldGetDialog(gameworld, vnum);
    if(dialog || vnum == NOTHING)
      charSetDialog(mob, vnum);
    next_substate = MEDIT_MAIN;
    break;
  }

#ifdef MODULE_SCRIPTS
  case MEDIT_SCRIPTS:
    // save the changes we made
    if(olcGetSave(olcGetNext(olc))) {
      SCRIPT_SET *scripts = (SCRIPT_SET *)olcGetData(olcGetNext(olc));
      charSetScripts(mob, (scripts ? copyScriptSet(scripts) : newScriptSet()));
    }
    olcSetNext(olc, NULL);
    next_substate = MEDIT_MAIN;
    break;
#endif

    /******************************************************/
    /*                        DEFAULT                     */
    /******************************************************/
  default:
    log_string("ERROR: Performing medit with invalid substate.");
    send_to_socket(sock, "An error occured while you were in OLC.\r\n");
    socketSetOLC(sock, NULL);
    socketSetState(sock, STATE_PLAYING);
    return;
  }

  olcSetSubstate(olc, next_substate);
  if(next_substate == MEDIT_MAIN)
    medit_menu(sock, olc);
}
