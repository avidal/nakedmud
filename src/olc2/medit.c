//*****************************************************************************
//
// medit.c
//
// The functions needed for doing online editing of mobiles.
//
//*****************************************************************************
#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../character.h"
#include "../races.h"
#include "../world.h"
#include "../dialog.h"
#include "../zone.h"

#include "olc.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/script.h"
#include "../editor/editor.h"



//*****************************************************************************
// mobile editing
//*****************************************************************************
#define MEDIT_NAME          1
#define MEDIT_MULTI_NAME    2
#define MEDIT_KEYWORDS      3
#define MEDIT_RDESC         4
#define MEDIT_MULTI_RDESC   5
#define MEDIT_RACE          6
#define MEDIT_SEX           7
#define MEDIT_DIALOG        8

void medit_menu(SOCKET_DATA *sock, CHAR_DATA *mob) {
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
		 "{gS) Script menu\r\n"
		 "{gR) Change race   {y[{c%8s{y]\r\n"
		 "{gG) Change Gender {y[{c%8s{y]\r\n"
		 "{gD) Dialog        {y[{c%8d{y]  {w%s\r\n",
		 charGetVnum(mob),
		 charGetName(mob),
		 charGetMultiName(mob),
		 charGetKeywords(mob),
		 charGetRdesc(mob),
		 charGetMultiRdesc(mob),
		 charGetDesc(mob),
		 charGetRace(mob),
		 sexGetName(charGetSex(mob)),
		 charGetDialog(mob),
		 (worldGetDialog(gameworld, charGetDialog(mob)) ?
		  dialogGetName(worldGetDialog(gameworld, 
					       charGetDialog(mob))) : "")
		 );
}

int  medit_chooser(SOCKET_DATA *sock, CHAR_DATA *mob, char option) {
  switch(toupper(option)) {
  case '1':
    text_to_buffer(sock, "Enter name: ");
    return MEDIT_NAME;
  case '2':
    text_to_buffer(sock, "Enter name for multiple occurences: ");
    return MEDIT_MULTI_NAME;
  case '3':
    text_to_buffer(sock, "Enter keywords: ");
    return MEDIT_KEYWORDS;
  case '4':
    text_to_buffer(sock, "Enter room description: ");
    return MEDIT_RDESC;
  case '5':
    text_to_buffer(sock, "Enter room description for multiple occurences: ");
    return MEDIT_MULTI_RDESC;
  case '6':
    text_to_buffer(sock, "Enter description\r\n");
    socketStartEditor(sock, text_editor, charGetDescBuffer(mob));
    return MENU_NOCHOICE;
  case 'R':
    send_to_socket(sock, "%s\r\n\r\n", raceGetList(FALSE));
    send_to_socket(sock, "Please select a race: ");
    return MEDIT_RACE;
  case 'G':
    olc_display_table(sock, sexGetName, NUM_SEXES, 1);
    return MEDIT_SEX;
  case 'D':
    text_to_buffer(sock, "Enter the dialog vnum (-1 for none): ");
    return MEDIT_DIALOG;
  case 'S':
    do_olc(sock, ssedit_menu, ssedit_chooser, ssedit_parser,
	   NULL, NULL, NULL, NULL, charGetScripts(mob));
    return MENU_NOCHOICE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool medit_parser(SOCKET_DATA *sock, CHAR_DATA *mob, int choice, 
		  const char *arg) {
  switch(choice) {
  case MEDIT_NAME:
    charSetName(mob, arg);
    return TRUE;
  case MEDIT_MULTI_NAME:
    charSetMultiName(mob, arg);
    return TRUE;
  case MEDIT_KEYWORDS:
    charSetKeywords(mob, arg);
    return TRUE;
  case MEDIT_RDESC:
    charSetRdesc(mob, arg);
    return TRUE;
  case MEDIT_MULTI_RDESC:
    charSetMultiRdesc(mob, arg);
    return TRUE;
  case MEDIT_RACE:
    if(!isRace(arg))
      return FALSE;
    charSetRace(mob, arg);
    return TRUE;
  case MEDIT_SEX: {
    int val = atoi(arg);
    if(!isdigit(*arg) || val < 0 || val >= NUM_SEXES)
      return FALSE;
    charSetSex(mob, val);
    return TRUE;
  }
  case MEDIT_DIALOG: {
    int vnum = (isdigit(*arg) ? atoi(arg) : NOTHING);
    DIALOG_DATA *dialog = worldGetDialog(gameworld, vnum);
    if(!dialog && vnum != NOTHING)
      return FALSE;
    else {
      charSetDialog(mob, vnum);    
      return TRUE;
    }
  }
  default: return FALSE;
  }
}

COMMAND(cmd_medit) {
  ZONE_DATA *zone;
  CHAR_DATA *mob;
  mob_vnum vnum;

  if(!arg || !*arg || !isdigit(*arg))
    send_to_char(ch, "Invalid vnum! Try again.\r\n");
  else {
    vnum = atoi(arg);

    // make sure there is a corresponding zone ...
    if((zone = worldZoneBounding(gameworld, vnum)) == NULL)
      send_to_char(ch, "No zone exists that contains the given vnum.\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    else {
      // find the mobile
      mob = zoneGetMob(zone, vnum);

      // make our mobile
      if(mob == NULL) {
	mob = newMobile(vnum);
	zoneAddMob(zone, mob);
	charSetName(mob, "an unfinished mobile");
	charSetKeywords(mob, "mobile, unfinshed");
	charSetRdesc(mob, "an unfinished mobile is standing here.");
	charSetDesc(mob, "it looks unfinished.\r\n");
	charSetMultiName(mob, "%d unfinished mobiles");
	charSetMultiRdesc(mob, "A group of %d mobiles are here, looking unfinished.");
      }

      do_olc(charGetSocket(ch), medit_menu, medit_chooser, medit_parser,
	     charCopy, charCopyTo, deleteChar, save_world, mob);
    }
  }
}
