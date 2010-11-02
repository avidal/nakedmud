//*****************************************************************************
//
// socedit.c
//
// contains all of the menus and OLC loops for editing socials.
//
//*****************************************************************************

#include "../mud.h"

#ifdef MODULE_SOCIALS
#include "../socket.h"
#include "../utils.h"
#include "../character.h"
#include "../socials/socials.h"

#include "olc.h"


void socedit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  SOCIAL_DATA *social = olcGetData(olc);
  send_to_socket(sock,
		 CLEAR_SCREEN
		 "{y[{c%s{y]\r\n"
		 "{g1) to char notgt: {c%s\r\n"
		 "{g2) to room notgt: {c%s\r\n"
		 "{g3) to char self : {c%s\r\n"
		 "{g4) to room self : {c%s\r\n"
		 "{g5) to char tgt  : {c%s\r\n"
		 "{g6) to vict tgt  : {c%s\r\n"
		 "{g7) to room tgt  : {c%s\r\n"
		 "{g8) minimum pos  : {c%s\r\n"
		 "{g9) maximum pos  : {c%s\r\n"
		 "\r\n"
		 "{gTo assocciate/unassociate commands, use soclink and socunlink\r\n"
		 "\r\n"
		 "{gEnter command (Q to quit) : ",
		 socialGetCmds(social),
		 socialGetCharNotgt(social),
		 socialGetRoomNotgt(social),
		 socialGetCharSelf(social),
		 socialGetRoomSelf(social),
		 socialGetCharTgt(social),
		 socialGetVictTgt(social),
		 socialGetRoomTgt(social),
		 posGetName(socialGetMinPos(social)),
		 posGetName(socialGetMaxPos(social))
		 );
}


//
// Display the list of positions a social can be completed from
//
void socedit_pos_menu(SOCKET_DATA *sock) {
  int i = 0;
  for(i = 0; i < NUM_POSITIONS; i++)
    send_to_socket(sock, "{g%2d) {c%s\r\n", i, posGetName(i));
  send_to_socket(sock, 
		 "\r\n"
		 "Enter the number of the new position : ");
}


void socedit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = SOCEDIT_MAIN;
  switch(toupper(*arg)) {
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N)? ");
    next_substate = SOCEDIT_CONFIRM_SAVE;
    break;
  case '1':
    send_to_socket(sock, 
		   "The message to character when no target is supplied : ");
    next_substate = SOCEDIT_CHAR_NOTGT;
    break;
  case '2':
    send_to_socket(sock, 
		   "The message to room when no target is supplied : ");
    next_substate = SOCEDIT_ROOM_NOTGT;
    break;
  case '3':
    send_to_socket(sock, 
		   "The message to character when target is self : ");
    next_substate = SOCEDIT_CHAR_SELF;
    break;
  case '4':
    send_to_socket(sock, 
		   "The message to room when target is self : ");
    next_substate = SOCEDIT_ROOM_SELF;
    break;
  case '5':
    send_to_socket(sock, 
		   "The message to character when a target is found : ");
    next_substate = SOCEDIT_CHAR_TGT;
    break;
  case '6':
    send_to_socket(sock, 
		   "The message to target when a target is found : ");
    next_substate = SOCEDIT_VICT_TGT;
    break;
  case '7':
    send_to_socket(sock, 
		   "The message to room when a target is found : ");
    next_substate = SOCEDIT_ROOM_TGT;
    break;
  case '8':
    socedit_pos_menu(sock);
    next_substate = SOCEDIT_MIN_POS;
    break;
  case '9':
    socedit_pos_menu(sock);
    next_substate = SOCEDIT_MAX_POS;
    break;
  default:
    break;
  }
  olcSetSubstate(olc, next_substate);
}


void socedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = SOCEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case SOCEDIT_MAIN:
    socedit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case SOCEDIT_CONFIRM_SAVE:
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
      next_substate = SOCEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case SOCEDIT_CHAR_NOTGT:
    socialSetCharNotgt(olcGetData(olc), arg);
    break;
  case SOCEDIT_ROOM_NOTGT:
    socialSetRoomNotgt(olcGetData(olc), arg);
    break;
  case SOCEDIT_CHAR_SELF:
    socialSetCharSelf(olcGetData(olc), arg);
    break;
  case SOCEDIT_ROOM_SELF:
    socialSetRoomSelf(olcGetData(olc), arg);
    break;
  case SOCEDIT_CHAR_TGT:
    socialSetCharTgt(olcGetData(olc), arg);
    break;
  case SOCEDIT_VICT_TGT:
    socialSetVictTgt(olcGetData(olc), arg);
    break;
  case SOCEDIT_ROOM_TGT:
    socialSetRoomTgt(olcGetData(olc), arg);
    break;

  case SOCEDIT_MIN_POS:
    if(!isdigit(*arg))
      break;
    else {
      int pos = atoi(arg);
      if(pos < 0 || pos >= NUM_POSITIONS)
	break;
      socialSetMinPos(olcGetData(olc), pos);
    }
    break;

  case SOCEDIT_MAX_POS:
    if(!isdigit(*arg))
      break;
    else {
      int pos = atoi(arg);
      if(pos < 0 || pos >= NUM_POSITIONS)
	break;
      socialSetMaxPos(olcGetData(olc), pos);
    }
    break;

    /******************************************************/
    /*                        DEFAULT                     */
    /******************************************************/
  default:
    log_string("ERROR: Performing socedit with invalid substate.");
    send_to_socket(sock, "An error occured while you were in OLC.\r\n");
    socketSetOLC(sock, NULL);
    socketSetState(sock, STATE_PLAYING);
    return;
  }

  // display the menu if we need to
  olcSetSubstate(olc, next_substate);
  if(next_substate == SOCEDIT_MAIN)
    socedit_menu(sock, olc);
}

#endif // MODULE_SOCIALS
