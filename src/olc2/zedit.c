//*****************************************************************************
//
// zedit.c
//
// zedit (zone edit) is a utility to allow builders to edit zone data within the
// game. Contains all the functions for editing zones.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../character.h"
#include "../room.h"
#include "../world.h"
#include "../zone.h"
#include "../editor/editor.h"

#include "olc.h"



//*****************************************************************************
// room editing functions
//*****************************************************************************
// the different fields of a room we can edit
#define ZEDIT_NAME       1
#define ZEDIT_EDITORS    2
#define ZEDIT_TIMER      3

void zedit_menu(SOCKET_DATA *sock, ZONE_DATA *zone) {
  send_to_socket(sock,
		 "{y[{c%d{y]\r\n"
		 "{g1) Name\r\n{c%s\r\n"
		 "{g2) Editors\r\n{c%s\r\n"
		 "{g3) Reset timer: {c%d{g min%s\r\n"
		 "{g4) Description\r\n{c%s\r\n"
		 ,
		 zoneGetVnum(zone), zoneGetName(zone), zoneGetEditors(zone),
		 zoneGetPulseTimer(zone), (zoneGetPulseTimer(zone)==1 ? "":"s"),
		 zoneGetDesc(zone));
}


int zedit_chooser(SOCKET_DATA *sock, ZONE_DATA *zone, char option) {
  switch(toupper(option)) {
  case '1':
    text_to_buffer(sock, "Enter a new zone name: ");
    return ZEDIT_NAME;
  case '2':
    text_to_buffer(sock, "Enter a new list of editors: ");
    return ZEDIT_EDITORS;
  case '3':
    text_to_buffer(sock, "Enter a new reset timer: ");
    return ZEDIT_TIMER;
  case '4':
    text_to_buffer(sock, "Enter a new description:\r\n");
    socketStartEditor(sock, text_editor, zoneGetDescBuffer(zone));
    return MENU_NOCHOICE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool zedit_parser(SOCKET_DATA *sock, ZONE_DATA *zone, int choice, 
		  const char *arg) {
  switch(choice) {
  case ZEDIT_NAME:
    zoneSetName(zone, arg);
    return TRUE;
  case ZEDIT_EDITORS:
    zoneSetEditors(zone, arg);
    return TRUE;
  case ZEDIT_TIMER:
    zoneSetPulseTimer(zone, MAX(-1, atoi(arg)));
    return TRUE;
  default:
    return FALSE;
  }
}


COMMAND(cmd_zedit) {
  // we want to create a new zone?
  if(!strncasecmp(arg, "new ", 4)) {
    zone_vnum vnum = 0;
    room_vnum min = 0, max = 0;

    // scan for the parameters
    sscanf(arg+4, "%d %d %d", &vnum, &min, &max);

    if(worldGetZone(gameworld, vnum))
      send_to_char(ch, "A zone already exists with that vnum.\r\n");
    else if(worldZoneBounding(gameworld, min) || worldZoneBounding(gameworld, max))
      send_to_char(ch, "There is already a zone bounding that vnum range.\r\n");
    else {
      ZONE_DATA *zone = newZone(vnum, min, max);
      char buf[MAX_BUFFER];
      sprintf(buf, "%s's zone", charGetName(ch));
      zoneSetName(zone, buf);
      sprintf(buf, "A new zone created by %s\r\n", charGetName(ch));
      zoneSetDesc(zone, buf);
      zoneSetEditors(zone, charGetName(ch));

      worldPutZone(gameworld, zone);
      send_to_char(ch, "You create a new zone (vnum %d).\r\n", vnum);

      // save the changes... this will get costly as our world gets bigger.
      // But that should be alright once we make zone saving a bit smarter
      worldSave(gameworld, WORLD_PATH);
    }
  }

  // we want to edit a preexisting zone
  else {
    ZONE_DATA *zone = NULL;
    zone_vnum vnum   = (!*arg ? 
			zoneGetVnum(worldZoneBounding(gameworld, roomGetVnum(charGetRoom(ch)))) : atoi(arg));
 
    // make sure there is a corresponding zone ...
    if((zone = worldGetZone(gameworld, vnum)) == NULL)
      send_to_char(ch, 
		   "No such zone exists. To create a new one, use "
		   "zedit new <vnum> <min> <max>\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    else {
      do_olc(charGetSocket(ch), zedit_menu, zedit_chooser, zedit_parser,
	     zoneCopy, zoneCopyTo, deleteZone, save_world,
	     worldZoneBounding(gameworld, roomGetVnum(charGetRoom(ch))));
    }
  }
}
