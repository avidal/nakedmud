//*****************************************************************************
//
// oedit.c
//
// The functions needed for doing online editing of objects
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../character.h"
#include "../handler.h"
#include "../object.h"
#include "../world.h"
#include "../zone.h"
#include "../bitvector.h"

#include "olc.h"
#include "olc_submenus.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/script.h"
#include "../editor/editor.h"
#include "../items/iedit.h"
#include "../items/items.h"



//*****************************************************************************
// object editing
//*****************************************************************************
#define OEDIT_NAME          1
#define OEDIT_MULTI_NAME    2
#define OEDIT_KEYWORDS      3
#define OEDIT_RDESC         4
#define OEDIT_MULTI_RDESC   5
#define OEDIT_WEIGHT        6

void oedit_menu(SOCKET_DATA *sock, OBJ_DATA *obj) {
  send_to_socket(sock,
		 "{g[{c%d{g]\r\n"
		 "{g1) Name\r\n"
		 "{c%s\r\n"
		 "{g2) Name for multiple occurences\r\n"
		 "{c%s\r\n"
		 "{g3) Keywords\r\n"
		 "{c%s\r\n"
		 "{g4) Room description\r\n"
		 "{c%s\r\n"
		 "{g5) Room description for multiple occurences\r\n"
		 "{c%s\r\n"
		 "{g6) Description\r\n"
		 "{c%s\r\n"
		 "{gW) Weight:    {c%1.3lf\r\n"
		 "{gT) Edit item types: {c%s\r\n"
		 "{gB) Edit bitvector : {c%s\r\n"
		 "{gS) Script menu\r\n"
		 "{gX) Extra Descriptions menu\r\n",
		 objGetVnum(obj),
		 objGetName(obj),
		 objGetMultiName(obj),
		 objGetKeywords(obj),
		 objGetRdesc(obj),
		 objGetMultiRdesc(obj),
		 objGetDesc(obj),
		 objGetWeightRaw(obj),
		 objGetTypes(obj),
		 bitvectorGetBits(objGetBits(obj))
		 );
}

int  oedit_chooser(SOCKET_DATA *sock, OBJ_DATA *obj, const char *option) {
  switch(toupper(*option)) {
  case '1':
    text_to_buffer(sock, "Enter name: ");
    return OEDIT_NAME;
  case '2':
    text_to_buffer(sock, "Enter name for multiple occurences: ");
    return OEDIT_MULTI_NAME;
  case '3':
    text_to_buffer(sock, "Enter keywords: ");
    return OEDIT_KEYWORDS;
  case '4':
    text_to_buffer(sock, "Enter room description: ");
    return OEDIT_RDESC;
  case '5':
    text_to_buffer(sock, "Enter room description for multiple occurences: ");
    return OEDIT_MULTI_RDESC;
  case '6':
    text_to_buffer(sock, "Enter description\r\n");
    socketStartEditor(sock, text_editor, objGetDescBuffer(obj));
    return MENU_NOCHOICE;
  case 'W':
    text_to_buffer(sock, "Enter new weight: ");
    return OEDIT_WEIGHT;
  case 'X':
    do_olc(sock, edesc_set_menu, edesc_set_chooser, edesc_set_parser, NULL,NULL,
	   NULL, NULL, objGetEdescs(obj));
    return MENU_NOCHOICE;
  case 'T':
    do_olc(sock, iedit_menu, iedit_chooser, iedit_parser, NULL, NULL, NULL,
	   NULL, obj);
    return MENU_NOCHOICE;
  case 'B':
    do_olc(sock, bedit_menu, bedit_chooser, bedit_parser, NULL, NULL, NULL,
	   NULL, objGetBits(obj));
    return MENU_NOCHOICE;
  case 'S':
    do_olc(sock, ssedit_menu, ssedit_chooser, ssedit_parser,
	   NULL, NULL, NULL, NULL, objGetScripts(obj));
    return MENU_NOCHOICE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool oedit_parser(SOCKET_DATA *sock, OBJ_DATA *obj, int choice, 
		  const char *arg){
  switch(choice) {
  case OEDIT_NAME:
    objSetName(obj, arg);
    return TRUE;
  case OEDIT_MULTI_NAME:
    objSetMultiName(obj, arg);
    return TRUE;
  case OEDIT_KEYWORDS:
    objSetKeywords(obj, arg);
    return TRUE;
  case OEDIT_RDESC:
    objSetRdesc(obj, arg);
    return TRUE;
  case OEDIT_MULTI_RDESC:
    objSetMultiRdesc(obj, arg);
    return TRUE;
  case OEDIT_WEIGHT: {
    double val = atof(arg);
    if(val < 0) return FALSE;
    objSetWeightRaw(obj, val);
    return TRUE;
  }
  default: return FALSE;
  }
}

// saves an object to disk
void save_object(OBJ_DATA *obj) {
  worldSaveObj(gameworld, obj);
}

COMMAND(cmd_oedit) {
  OBJ_DATA *obj;

  // if no argument is supplied, default to the current obj
  if(!arg || !*arg)
    send_to_char(ch, "Please supply the vnum of a obj you wish to edit.\r\n");

  // we're trying to edit an object by name... must be something in the world
  else if(!isdigit(*arg)) {
    obj = generic_find(ch, arg, FIND_TYPE_OBJ, FIND_SCOPE_INV | FIND_SCOPE_ROOM,
		       FALSE, NULL);
    if(obj == NULL)
      send_to_char(ch, "What were you trying to edit?\r\n");
    else {
      do_olc(charGetSocket(ch), oedit_menu, oedit_chooser, oedit_parser,
	     NULL, NULL, NULL, NULL, obj);
    }
  }

  // we're editing an object by vnum... edit the prototype
  else {
    ZONE_DATA *zone = NULL;
    int   vnum = atoi(arg);

    // make sure there is a corresponding zone ...
    if((zone = worldZoneBounding(gameworld, vnum)) == NULL)
      send_to_char(ch, "No zone exists that contains the given vnum.\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    else {
      // find the obj
      obj = zoneGetObj(zone, vnum);

      // make our obj
      if(obj == NULL) {
	obj = newObj(vnum);
	objSetVnum(obj, vnum);
	zoneAddObj(zone, obj);
	objSetName      (obj, "an unfinished object");
	objSetKeywords  (obj, "object, unfinshed");
	objSetRdesc     (obj, "an unfinished object is lying here.");
	objSetDesc      (obj, "it looks unfinished.\r\n");
	objSetMultiName (obj, "a group of %d unfinished objects");
	objSetMultiRdesc(obj, "%d objects lay here, all unfinished.");
      }

      do_olc(charGetSocket(ch), oedit_menu, oedit_chooser, oedit_parser,
	     objCopy, objCopyTo, deleteObj, save_object, obj);
    }
  }
}
