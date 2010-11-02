//*****************************************************************************
//
// oedit.c
//
// Contains all of the functions needed for online editing of objects.
//
//*****************************************************************************

#include "../mud.h"
#include "../world.h"
#include "../object.h"
#include "../extra_descs.h"
#include "../socket.h"
#include "../utils.h"
#include "../body.h"
#include "../items.h"

#include "olc.h"

#ifdef MODULE_SCRIPTS
#include "../scripts/script_set.h"
#include "../scripts/script.h"
#endif


//
// a quick little utility to assert that a number is within a given
// range (max inclusive, min not). If it isn't, make sure the next 
// state we go to is the same one we're currently in.
//
#define ASSERT_VAL_RANGE(num, min, max)                     \
  if(num < min || num > max) {                              \
    oedit_val_menu(sock, olc, olcGetSubstate(olc));         \
    next_substate = olcGetSubstate(olc);                    \
  }                                                         \
  else {                                                    \
    objSetVal(obj, olcGetSubstate(olc) - OEDIT_VAL_0, num); \
    next_substate = olcGetSubstate(olc) + 1;                \
    if(next_substate > OEDIT_VAL_0 + NUM_OBJ_VALUES)        \
      next_substate = OEDIT_MAIN;                           \
  }


void oedit_menu(SOCKET_DATA *sock, OLC_DATA *olc) {
  OBJ_DATA *obj = (OBJ_DATA *)olcGetData(olc);
  char bitbuf[SMALL_BUFFER] = "";
  char valbuf[MAX_BUFFER] = "";
  int i;

  for(i = 0; i < NUM_OBJ_VALUES; i++)
    sprintf(valbuf, "%s%d ", valbuf, objGetVal(obj, i));

  objPrintBits(obj, BITFIELD_OBJ, bitbuf);

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
		 "{gW) Weight:    {c%1.3lf\r\n"
		 "{gC) Capacity:  {c%1.3lf\r\n"
		 "{gB) Edit bits: {c%s\r\n"
		 "{gX) Extra Descriptions menu\r\n"
#ifdef MODULE_SCRIPTS
		 "{gS) Script menu\r\n"
#endif
		 "{gT)    Type: {c%s\r\n"
		 "{g   Subtype: {c%s\r\n"
		 "{g    Values: {c%s\r\n",
		 objGetVnum(obj),
		 objGetName(obj),
		 objGetMultiName(obj),
		 objGetKeywords(obj),
		 objGetRdesc(obj),
		 objGetMultiRdesc(obj),
		 objGetDesc(obj),
		 objGetWeightRaw(obj),
		 objGetCapacity(obj),
		 bitbuf,
		 itemGetType(objGetType(obj)),
		 (numItemSubtypes(objGetType(obj)) > 0 ?
		  itemGetSubtype(objGetType(obj), objGetSubtype(obj)) : "none"),
		 valbuf
		 );
  send_to_socket(sock,
		 "\r\n"
		 "{gEnter choice (Q to quit) : {n"
		 );
}


//
// lists all of the item types available
//
void show_item_type_menu(SOCKET_DATA *sock) {
  int i;

  send_to_socket(sock, "Item types:\r\n");
  for(i = 0; i < NUM_ITEM_TYPES; i++)
    send_to_socket(sock, "{c%4d{g) %-14s%s",
		   i, itemGetType(i), (i % 3 == 2 ? "\r\n" : "    "));
  if(i % 3 != 0)
    send_to_socket(sock, "\r\n");

  send_to_socket(sock, "\r\nEnter your choice: ");
}


//
// lists all of the subtypes of a particular item type
//
void show_item_subtype_menu(SOCKET_DATA *sock, int type) {
  int i;
  int num_subtypes = numItemSubtypes(type);

  if(num_subtypes == 0)
    return;
  send_to_socket(sock, "Item subtypes:\r\n");
  for(i = 0; i < num_subtypes; i++)
    send_to_socket(sock, "{c%4d{g) %-14s%s",
		   i, itemGetSubtype(type, i), (i % 3 == 2 ? "\r\n" : "    "));
  if(i % 3 != 0)
    send_to_socket(sock, "\r\n");
  send_to_socket(sock, "\r\nEnter your choice: ");
}


//
// Show all of the bits
//
void show_obj_bits(SOCKET_DATA *sock, OLC_DATA *olc) {
  OBJ_DATA *obj = olcGetData(olc);
  char buf[SMALL_BUFFER];
  int i = 0;
  
  send_to_socket(sock, CLEAR_SCREEN);
  for(i = 0; i < NUM_OBJ_BITS; i++)
    send_to_socket(sock, "{c%4d{g) %-14s%s",
		   i, objBitGetName(BITFIELD_OBJ, i), 
		   (i % 3 == 2 ? "\r\n" : "    "));
  if(i % 3 != 0)
    send_to_socket(sock, "\r\n");

  objPrintBits(obj, BITFIELD_OBJ, buf);
  send_to_socket(sock, 
		 "\r\n"
		 "Current bits: {c%s{g\r\n"
		 "Enter your choice (Q to quit) : ", buf);
}


//
// Returns the state that we should be in right now, if we're trying
// to edit an olc value with editing state olc_state
//
int oedit_val_menu(SOCKET_DATA *sock, OLC_DATA *olc, int olc_state) {
  int type = objGetType((OBJ_DATA *)olcGetData(olc));
  switch(olc_state) {
    /*************/
    /* OBJ VAL 0 */
    /*************/
  case OEDIT_VAL_0:
    switch(type) {
    case ITEM_PORTAL:
      send_to_socket(sock, "Enter a destination (-1, 1000000) : ");
      break;
    case ITEM_FURNITURE:
      send_to_socket(sock, "What is the seating capacity (1, 100) : ");
      break;
    case ITEM_CONTAINER:
      send_to_socket(sock, "Is the container closable (0 = no, 1 = yes) : ");
      break;
    default: 
      return oedit_val_menu(sock, olc, olc_state+1);
    }
    break;


    /*************/
    /* OBJ VAL 1 */
    /*************/
  case OEDIT_VAL_1:
    switch(type) {
    case ITEM_CONTAINER:
      send_to_socket(sock, "What is the key vnum (-1 for none) : ");
      break;
    default: 
      return oedit_val_menu(sock, olc, olc_state+1);
    }
    break;


    /*************/
    /* OBJ VAL 2 */
    /*************/
  case OEDIT_VAL_2:
    switch(type) {
    case ITEM_CONTAINER:
      send_to_socket(sock, "What is the lock picking difficulty : ");
      break;
    default: 
      return oedit_val_menu(sock, olc, olc_state+1);
    }
    break;


    /*************/
    /* OBJ VAL 3 */
    /*************/
  case OEDIT_VAL_3:
    switch(type) {
    default: 
      return oedit_val_menu(sock, olc, olc_state+1);
    }
    break;

  default: 
    oedit_menu(sock, olc);
    return OEDIT_MAIN;
  }

  // this is reached when we've actually printed a prompt
  return olc_state;
}



void oedit_main_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  int next_substate = OEDIT_MAIN;

  switch(toupper(*arg)) {
  case 'Q':
    send_to_socket(sock, "Save changes (Y/N) : ");
    next_substate = OEDIT_CONFIRM_SAVE;
    break;

  case '1':
    send_to_socket(sock, "Enter name : ");
    next_substate = OEDIT_NAME;
    break;

  case '2':
    send_to_socket(sock, "Enter name for multiple occurances : ");
    next_substate = OEDIT_MULTI_NAME;
    break;

  case '3':
    send_to_socket(sock, "Enter keywords : ");
    next_substate = OEDIT_KEYWORDS;
    break;

  case '4':
    send_to_socket(sock, "Enter room description : ");
    next_substate = OEDIT_RDESC;
    break;

  case '5':
    send_to_socket(sock, "Enter room description for multiple occurances : ");
    next_substate = OEDIT_MULTI_RDESC;
    break;

  case '6':
    send_to_socket(sock, "Enter description\r\n");
    start_text_editor(sock, 
		      objGetDescPtr((OBJ_DATA *)olcGetData(olc)),
		      MAX_BUFFER, EDITOR_MODE_NORMAL);
    next_substate = OEDIT_MAIN;
    break;

  case 'W':
    send_to_socket(sock, "Enter new weight : ");
    next_substate = OEDIT_WEIGHT;
    break;

  case 'C':
    send_to_socket(sock, "Enter new carrying capacity : ");
    next_substate = OEDIT_CAPACITY;
    break;

  case 'B':
    show_obj_bits(sock, olc);
    next_substate = OEDIT_BITS;
    break;

  case 'X':
    olcSetNext(olc, newOLC(OLC_EDSEDIT, EDSEDIT_MAIN, 
			   copyEdescSet(objGetEdescs((OBJ_DATA *)olcGetData(olc))), NULL));
    next_substate = OEDIT_EDESCS;
    break;

#ifdef MODULE_SCRIPTS
  case 'S':
    olcSetNext(olc, newOLC(OLC_SSEDIT, SSEDIT_MAIN,
			   copyScriptSet(objGetScripts((OBJ_DATA *)olcGetData(olc))), objGetName((OBJ_DATA *)olcGetData(olc))));
    next_substate = OEDIT_SCRIPTS;
    break;
#endif

  case 'T':
    show_item_type_menu(sock);
    next_substate = OEDIT_TYPE;
    break;

  default:
    oedit_menu(sock, olc);
    break;
  }

  olcSetSubstate(olc, next_substate);
}



void oedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg) {
  OBJ_DATA *obj = (OBJ_DATA *)olcGetData(olc);
  int next_substate = OEDIT_MAIN;

  switch(olcGetSubstate(olc)) {
    /******************************************************/
    /*                     MAIN MENU                      */
    /******************************************************/
  case OEDIT_MAIN:
    oedit_main_loop(sock, olc, arg);
    return;


    /******************************************************/
    /*                    CONFIRM SAVE                    */
    /******************************************************/
  case OEDIT_CONFIRM_SAVE:
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
      next_substate = OEDIT_CONFIRM_SAVE;
      break;
    }
    break;


    /******************************************************/
    /*                      SET VALUES                    */
    /******************************************************/
  case OEDIT_NAME:
    objSetName(obj, arg);
    break;

  case OEDIT_MULTI_NAME:
    objSetMultiName(obj, arg);
    break;

  case OEDIT_KEYWORDS:
    objSetKeywords(obj, arg);
    break;

  case OEDIT_RDESC:
    objSetRdesc(obj, arg);
    break;

  case OEDIT_MULTI_RDESC:
    objSetMultiRdesc(obj, arg);
    break;

  case OEDIT_WEIGHT:
    objSetWeightRaw(obj, atof(arg));
    break;

  case OEDIT_CAPACITY:
    objSetCapacity(obj, atof(arg));
    break;

  case OEDIT_EDESCS:
    // save the changes we made
    if(olcGetSave(olcGetNext(olc))) {
      EDESC_SET *edescs = (EDESC_SET *)olcGetData(olcGetNext(olc));
      objSetEdescs(obj, (edescs ? copyEdescSet(edescs) : newEdescSet()));
    }

    olcSetNext(olc, NULL);
    next_substate = OEDIT_MAIN;
    break;

#ifdef MODULE_SCRIPTS
  case OEDIT_SCRIPTS:
    // save the changes we made
    if(olcGetSave(olcGetNext(olc))) {
      SCRIPT_SET *scripts = (SCRIPT_SET *)olcGetData(olcGetNext(olc));
      objSetScripts(obj, (scripts ? copyScriptSet(scripts) : newScriptSet()));
    }
    olcSetNext(olc, NULL);
    next_substate = OEDIT_MAIN;
    break;
#endif

  case OEDIT_BITS:
    if(toupper(*arg) == 'Q')
      next_substate = OEDIT_MAIN;
    else {
      int bit = atoi(arg);
      if(bit >= 0 && bit < NUM_OBJ_BITS)
	objToggleBit(obj, BITFIELD_OBJ, bit);
      next_substate = OEDIT_BITS;
    }
    break;

  case OEDIT_TYPE: {
    int num = atoi(arg);
    if(num < 0 || num >= NUM_ITEM_TYPES) {
      show_item_type_menu(sock);
      next_substate = OEDIT_TYPE;
    }
    else {
      // if we're not of the same type, reset all of our values
      if(num != objGetType(obj)) {
	int i;
	for(i = 0; i < NUM_OBJ_VALUES; i++)
	  objSetVal(obj, i, 0);
      }

      objSetType(obj, num);
      if(numItemSubtypes(num) > 0) {
	show_item_subtype_menu(sock, num);
	next_substate = OEDIT_SUBTYPE;
      }
      else
	next_substate = OEDIT_VAL_0;
    }
    break;
  }

  case OEDIT_SUBTYPE: {
    int num = atoi(arg);
    if(num < 0 || num >= numItemSubtypes(objGetType(obj))) {
      show_item_subtype_menu(sock, objGetType(obj));
      next_substate = OEDIT_SUBTYPE;
    }
    else {
      objSetSubtype(obj, num);
      next_substate = OEDIT_VAL_0;
    }
    break;
  }

  case OEDIT_VAL_0:
    switch(objGetType(obj)) {
    case ITEM_PORTAL:    ASSERT_VAL_RANGE(atoi(arg), -1, 1000000);  break;
    case ITEM_FURNITURE: ASSERT_VAL_RANGE(atoi(arg), -1, 100);      break;
    case ITEM_CONTAINER: ASSERT_VAL_RANGE(atoi(arg),  0, 1);        break;
    default: break;
    }
    next_substate = OEDIT_VAL_1;
    break;

  case OEDIT_VAL_1:
    switch(objGetType(obj)) {
    case ITEM_CONTAINER: ASSERT_VAL_RANGE(atoi(arg), -1, 1000000); break;
    default: break;
    }
    next_substate = OEDIT_VAL_2;
    break;

  case OEDIT_VAL_2:
    switch(objGetType(obj)) {
    case ITEM_CONTAINER: ASSERT_VAL_RANGE(atoi(arg), 0, 1000000); break;
    default: break;
    }
    next_substate = OEDIT_VAL_3;
    break;

  case OEDIT_VAL_3:
    switch(objGetType(obj)) {
    default: break;
    }
    next_substate = OEDIT_MAIN;
    break;


    /******************************************************/
    /*                        DEFAULT                     */
    /******************************************************/
  default:
    log_string("ERROR: Performing oedit with invalid substate.");
    send_to_socket(sock, "An error occured while you were in OLC.\r\n");
    socketSetOLC(sock, NULL);
    socketSetState(sock, STATE_PLAYING);
    return;
  }


  /********************************************************/
  /*                 DISPLAY NEXT MENU HERE               */
  /********************************************************/
  if(next_substate == OEDIT_MAIN)
    oedit_menu(sock, olc);
  else if(next_substate >= OEDIT_VAL_0 && 
	  next_substate <  OEDIT_VAL_0 + NUM_OBJ_VALUES)
    next_substate = oedit_val_menu(sock, olc, next_substate);
  else if(next_substate == OEDIT_BITS)
    show_obj_bits(sock, olc);

  olcSetSubstate(olc, next_substate);
}
