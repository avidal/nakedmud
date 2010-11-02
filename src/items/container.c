//*****************************************************************************
//
// container.c
//
// handles all of the functioning of the container item type. Stores data about 
// the contents and capacity of a container, and all the functions for 
// interacting with container types.
//
//*****************************************************************************

#include "../mud.h"
#include "../storage.h"
#include "../object.h"
#include "../world.h"
#include "../socket.h"
#include "../olc2/olc.h"

#include "items.h"
#include "iedit.h"



//*****************************************************************************
// item data for containers
//*****************************************************************************
typedef struct container_data {
  double capacity;
  int    key;
  int    pick_diff;
  bool   closable;
  bool   closed;
  bool   locked;
} CONTAINER_DATA;

CONTAINER_DATA *newContainerData() {
  CONTAINER_DATA *data = malloc(sizeof(CONTAINER_DATA));
  data->capacity  = 0;
  data->key       = NOTHING;
  data->pick_diff = 0;
  data->closed    = FALSE;
  data->locked    = FALSE;
  return data;
}

void deleteContainerData(CONTAINER_DATA *data) {
  free(data);
}

void containerDataCopyTo(CONTAINER_DATA *from, CONTAINER_DATA *to) {
  *to = *from;
}

CONTAINER_DATA *containerDataCopy(CONTAINER_DATA *data) {
  CONTAINER_DATA *new_data = newContainerData();
  containerDataCopyTo(data, new_data);
  return new_data;
}

STORAGE_SET *containerDataStore(CONTAINER_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_double(set, "capacity", data->capacity);
  store_int   (set, "key",      data->key);
  store_int   (set, "pick_diff",data->pick_diff);
  store_int   (set, "closable", data->closable);
  store_int   (set, "closed",   data->closed);
  store_int   (set, "locked",   data->locked);
  return set;
}

CONTAINER_DATA *containerDataRead(STORAGE_SET *set) {
  CONTAINER_DATA *data = newContainerData();
  data->capacity = read_double(set, "capacity");
  data->key      = read_int   (set, "key");
  data->pick_diff= read_int   (set, "pick_diff");
  data->closable = read_int   (set, "closable");
  data->closed   = read_int   (set, "closed");
  data->locked   = read_int   (set, "locked");
  return data;
}



//*****************************************************************************
// functions for interacting with containers
//*****************************************************************************
double containerGetCapacity(OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->capacity;
}

void containerSetCapacity(OBJ_DATA *obj, double capacity) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->capacity = capacity;
}

bool containerIsClosable (OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->closable;
}

bool containerIsClosed   (OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->closed;
}

bool containerIsLocked   (OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->locked;
}

int  containerGetKey     (OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->key;
}

int  containerGetPicKDiff(OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->pick_diff;
}

void containerSetClosed  (OBJ_DATA *obj, bool closed) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->closed = closed;
}

void   containerSetClosable(OBJ_DATA *obj, bool closable) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->closable = closable;
}

void containerSetLocked  (OBJ_DATA *obj, bool locked) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->locked = locked;
}

void containerSetKey     (OBJ_DATA *obj, int  vnum) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->key = vnum;
}

void containerSetPickDiff(OBJ_DATA *obj, int  diff) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->pick_diff = diff;
}



//*****************************************************************************
// container olc
//*****************************************************************************
#define IEDIT_CONTAINER_CAPACITY      1
#define IEDIT_CONTAINER_KEY           2
#define IEDIT_CONTAINER_PICK_DIFF     3

// the resedit olc needs these declared
void iedit_container_menu   (SOCKET_DATA *sock, CONTAINER_DATA *data) {
  send_to_socket(sock, 
		 "{g1) Capacity : {c%1.2lf\r\n"
		 "{g2) Key      : {c%d (%s)\r\n"
		 "{g3) Closable : {c%s\r\n"
		 "{g4) Pick diff: {c%d\r\n",
		 data->capacity,
		 data->key, 
		 (worldGetObj(gameworld, data->key) ? 
		  objGetName(worldGetObj(gameworld, data->key)) : "nothing"),
		 (data->closable ? "yes" : "no"),
		 data->pick_diff);
}

int  iedit_container_chooser(SOCKET_DATA *sock, CONTAINER_DATA *data, char option) {
  switch(toupper(option)) {
  case '1':
    text_to_buffer(sock, "Enter a new weight capacity for the container: ");
    return IEDIT_CONTAINER_CAPACITY;
  case '2':
    text_to_buffer(sock, "Enter akey vnum for the container (-1 for none): ");
    return IEDIT_CONTAINER_KEY;
  case '3':
    data->closable = (data->closable + 1) % 2;
    return MENU_NOCHOICE;
  case '4':
    text_to_buffer(sock, "How difficult is the lock to pick: ");
    return IEDIT_CONTAINER_PICK_DIFF;
  default: return MENU_CHOICE_INVALID;
  }
}

bool iedit_container_parser (SOCKET_DATA *sock, CONTAINER_DATA *data, int choice, 
			  const char *arg) {
  switch(choice) {
  case IEDIT_CONTAINER_CAPACITY: {
    double capacity = atof(arg);
    if(capacity <= 0)
      return FALSE;
    data->capacity = capacity;
    return TRUE;
  }
  case IEDIT_CONTAINER_KEY: {
    int key = atoi(arg);
    // ugh... ugly logic. Clean this up one day?
    //   Make sure what we're getting is a positive number or make sure it is
    //   the NOTHING number (-1). Also make sure that, if it's not NOTHING, it
    //   corresponds to a vnum of an object already created.
    if((key != NOTHING && !worldGetObj(gameworld, key)) ||
       (key != NOTHING && !isdigit(*arg)))
      return FALSE;
    data->key = key;
    return TRUE;
  }
  case IEDIT_CONTAINER_PICK_DIFF: {
    int diff = atoi(arg);
    if(diff < 0 || !isdigit(*arg))
      return FALSE;
    data->pick_diff = diff;
    return TRUE;
  }
  default: return FALSE;
  }
}



//*****************************************************************************
// install the container item type
//*****************************************************************************

//
// this will need to be called by init_items() in items/items.c
void init_container(void) {
  item_add_type("container", 
  		newContainerData, deleteContainerData,
  		containerDataCopyTo, containerDataCopy, 
  		containerDataStore, containerDataRead);

  // set up the container OLC too
  item_add_olc("container", iedit_container_menu, iedit_container_chooser, 
  	       iedit_container_parser);
}
