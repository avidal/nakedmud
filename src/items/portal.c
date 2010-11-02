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
#include "../object.h"
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
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pyobj.h"
#include "../scripts/pyroom.h"



//*****************************************************************************
// item data for portals
//*****************************************************************************
typedef struct portal_data {
  char *dest;
} PORTAL_DATA;

PORTAL_DATA *newPortalData() {
  PORTAL_DATA *data = malloc(sizeof(PORTAL_DATA));
  data->dest = strdup("");
  return data;
}

void deletePortalData(PORTAL_DATA *data) {
  if(data->dest) free(data->dest);
  free(data);
}

void portalDataCopyTo(PORTAL_DATA *from, PORTAL_DATA *to) {
  if(to->dest) free(to->dest);
  to->dest = strdupsafe(from->dest);
}

PORTAL_DATA *portalDataCopy(PORTAL_DATA *data) {
  PORTAL_DATA *new_data = newPortalData();
  portalDataCopyTo(data, new_data);
  return new_data;
}

STORAGE_SET *portalDataStore(PORTAL_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "dest", data->dest);
  return set;
}

PORTAL_DATA *portalDataRead(STORAGE_SET *set) {
  PORTAL_DATA *data = newPortalData();
  data->dest = strdup(read_string(set, "dest"));
  return data;
}



//*****************************************************************************
// functions for interacting with portals
//*****************************************************************************
const char *portalGetDest(OBJ_DATA *obj) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  return data->dest;
}

void portalSetDest(OBJ_DATA *obj, const char *dest) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  if(data->dest) free(data->dest);
  data->dest = strdupsafe(dest);
}


//
// cmd_enter is used to go through portals
//   usage: enter <object>
//
//   examples:
//     enter portal         enter the thing called "portal" in your room
COMMAND(cmd_enter) {
  void    *found = NULL;
  int found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, "{ obj.room exit } ", &found, &found_type))
    return;

  // we're trying to enter an exit
  if(found_type == PARSE_EXIT) {
    if(try_move_mssg(ch, roomGetExitDir(charGetRoom(ch), found)))
      look_at_room(ch, charGetRoom(ch));
  }

  // we're trying to enter a portal
  else {
    if(!objIsType(found, "portal"))
      send_to_char(ch, "You cannot seem to find an enterance.\r\n");
    else {
      ROOM_DATA *dest = worldGetRoom(gameworld, portalGetDest(found));
      if(dest == NULL)
	send_to_char(ch, "There is nothing on the other side...\r\n");
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
}



//*****************************************************************************
// portal olc
//*****************************************************************************
#define IEDIT_PORTAL_DEST      1

// the resedit olc needs these declared
void iedit_portal_menu   (SOCKET_DATA *sock, PORTAL_DATA *data) {
  send_to_socket(sock, "{g1) Destination: {c%s\r\n", data->dest);
}

int  iedit_portal_chooser(SOCKET_DATA *sock, PORTAL_DATA *data, 
			  const char *option) {
  switch(toupper(*option)) {
  case '1': 
    text_to_buffer(sock, "Enter new destination (return for none): ");
    return IEDIT_PORTAL_DEST;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool iedit_portal_parser (SOCKET_DATA *sock, PORTAL_DATA *data, int choice, 
			  const char *arg) {
  switch(choice) {
  case IEDIT_PORTAL_DEST: {
    if(data->dest) free(data->dest);
    data->dest   = strdupsafe(arg);
    return TRUE;
  }
  default:
    return FALSE;
  }
}

void portal_from_proto(PORTAL_DATA *data, BUFFER *buf) {
  if(*bufferString(buf)) {
    char dest[SMALL_BUFFER];
    sscanf(bufferString(buf), "me.portal_dest = \"%s", dest);
    dest[next_letter_in(dest, '\"')] = '\0'; // kill closing "
    if(data->dest) free(data->dest);
    data->dest = strdupsafe(dest);
  }
}

void portal_to_proto(PORTAL_DATA *data, BUFFER *buf) {
  if(*data->dest)
    bprintf(buf, "me.portal_dest = \"%s\"\n", data->dest);
}



//*****************************************************************************
// pyobj getters and setters
//*****************************************************************************
PyObject *PyObj_getportaldest(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL)
    return NULL;
  else if(objIsType(obj, "portal"))
    return Py_BuildValue("s", portalGetDest(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only get destination for portals.");
    return NULL;
  }
}

int PyObj_setportaldest(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to set destination for "
		 "nonexistent portal, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "portal")) {
    PyErr_Format(PyExc_TypeError, "Tried to set destination for non-portal, %s",
		 objGetClass(obj));
    return -1;
  }

  if(PyString_Check(value))
    portalSetDest(obj, PyString_AsString(value));
  else if(PyRoom_Check(value))
    portalSetDest(obj, roomGetClass(PyRoom_AsRoom(value)));
  else {
    PyErr_Format(PyExc_TypeError, "portal dest must be a room or string.");
    return -1;
  }

  return 0;
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
	       iedit_portal_parser, portal_from_proto, portal_to_proto);

  // make it so we can set portal destinations in scripts
  PyObj_addGetSetter("portal_dest", PyObj_getportaldest, PyObj_setportaldest,
		     "the database key of the room we're going to.");

  add_cmd("enter", NULL, cmd_enter, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE);
}
