//*****************************************************************************
//
// portal.c
//
// handles all of the functioning of the portal item type. Stores data about 
// the destination vnums of a portal, and does all the legwork for retreiving 
// destinations when someone wants to interact with a portal.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../storage.h"
#include "../character.h"
#include "../socket.h"
#include "../room.h"
#include "../world.h"
#include "../movement.h"
#include "../inform.h"
#include "../handler.h"
#include "items.h"

#include "../olc2/olc.h"
#include "iedit.h"


//*****************************************************************************
// item data for portals
//*****************************************************************************
typedef struct portal_data {
  int dest;
} PORTAL_DATA;

PORTAL_DATA *newPortalData() {
  PORTAL_DATA *data = malloc(sizeof(PORTAL_DATA));
  data->dest = NOWHERE;
  return data;
}

void deletePortalData(PORTAL_DATA *data) {
  free(data);
}

void portalDataCopyTo(PORTAL_DATA *from, PORTAL_DATA *to) {
  to->dest = from->dest;
}

PORTAL_DATA *portalDataCopy(PORTAL_DATA *data) {
  PORTAL_DATA *new_data = newPortalData();
  portalDataCopyTo(data, new_data);
  return new_data;
}

STORAGE_SET *portalDataStore(PORTAL_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_int(set, "dest", data->dest);
  return set;
}

PORTAL_DATA *portalDataRead(STORAGE_SET *set) {
  PORTAL_DATA *data = newPortalData();
  data->dest = read_int(set, "dest");
  return data;
}



//*****************************************************************************
// functions for interacting with portals
//*****************************************************************************
int portalGetDest(OBJ_DATA *obj) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  return data->dest;
}

void portalSetDest(OBJ_DATA *obj, int dest) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  data->dest = dest;
}


//
// cmd_enter is used to go through portals
//   usage: enter [object]
//
//   examples:
//     enter portal         enter the thing called "portal" in your room
COMMAND(cmd_enter) {
  if(!arg || !*arg)
    send_to_char(ch, "What did you want to enter?\r\n");
  else {
    int found_type = FOUND_NONE;
    void *found = generic_find(ch, arg,
			       FIND_TYPE_OBJ | FIND_TYPE_EXIT,
			       FIND_SCOPE_IMMEDIATE,
			       FALSE, &found_type);


    // we're trying to enter an exit
    if(found && found_type == FOUND_EXIT) {
      ROOM_DATA *to = try_exit(ch, found, DIR_NONE);
      if(to != NULL) look_at_room(ch, to);
    }

    // we're trying to enter a portal
    else if(found && found_type == FOUND_OBJ) {
      if(!objIsType(found, "portal"))
	send_to_char(ch, "You cannot seem to find an enterance.\r\n");
      else {
	ROOM_DATA *dest = worldGetRoom(gameworld, portalGetDest(found));
	if(!dest)
	  send_to_char(ch, 
		       "You go to enter the portal, "
		       "but dark forces prevent you!\r\n");
	else {
	  send_to_char(ch, "You step through %s.\r\n", see_obj_as(ch, found));
	  message(ch, NULL, found, NULL, TRUE, TO_ROOM,
		  "$n steps through $o.");
	  char_from_room(ch);
	  char_to_room(ch, dest);
	  look_at_room(ch, dest);
	  message(ch, NULL, found, NULL, TRUE, TO_ROOM,
		  "$n arrives after travelling through $o.");
	}
      }
    }
    else
      send_to_char(ch, "What were you trying to enter?\r\n");
  }
}



//*****************************************************************************
// portal olc
//*****************************************************************************
#define IEDIT_PORTAL_DEST      1

// the resedit olc needs these declared
void iedit_portal_menu   (SOCKET_DATA *sock, PORTAL_DATA *data) {
  ROOM_DATA *dest = worldGetRoom(gameworld, data->dest);
  send_to_socket(sock, "{g1) Destination: {c%d (%s)\r\n", data->dest,
		 (dest ? roomGetName(dest) : "nowhere"));
}

int  iedit_portal_chooser(SOCKET_DATA *sock, PORTAL_DATA *data, 
			  const char *option) {
  switch(toupper(*option)) {
  case '1': 
    text_to_buffer(sock, "Enter new destination (-1 for none): ");
    return IEDIT_PORTAL_DEST;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool iedit_portal_parser (SOCKET_DATA *sock, PORTAL_DATA *data, int choice, 
			  const char *arg) {
  switch(choice) {
  case IEDIT_PORTAL_DEST: {
    int dest = atoi(arg);
    // ugh... ugly logic. Clean this up one day?
    //   Make sure what we're getting is a positive number or make sure it is
    //   the NOWHERE number (-1). Also make sure that, if it's not NOWHERE, it
    //   corresponds to a vnum of a room already created.
    if((dest != NOWHERE && !worldGetRoom(gameworld, dest)) ||
       (dest != NOWHERE && !isdigit(*arg)))
      return FALSE;
    data->dest = dest;
    return TRUE;
  }
  default:
    return FALSE;
  }
}



//*****************************************************************************
// install the portal item type
//*****************************************************************************

//
// this will need to be called by init_items() in items/items.c
void init_portal(void) {
  item_add_type("portal", 
		newPortalData, deletePortalData,
		portalDataCopyTo, portalDataCopy, 
		portalDataStore, portalDataRead);

  // set up the portal OLC too
  item_add_olc("portal", iedit_portal_menu, iedit_portal_chooser, 
	       iedit_portal_parser);

  add_cmd("enter", NULL, cmd_enter, 0, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE);
}
