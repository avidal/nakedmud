//*****************************************************************************
//
// furniture.c
//
// handles all of the functioning of the furniture item type. Stores data about 
// the capacity of the furniture, and what type of furniture it is. 
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
#include "furniture.h"



//*****************************************************************************
// item data for furnitures
//*****************************************************************************
typedef struct furniture_data {
  int capacity;
  int type;
} FURNITURE_DATA;

FURNITURE_DATA *newFurnitureData() {
  FURNITURE_DATA *data = malloc(sizeof(FURNITURE_DATA));
  data->capacity  = 0;
  data->type      = FURNITURE_AT;
  return data;
}

void deleteFurnitureData(FURNITURE_DATA *data) {
  free(data);
}

void furnitureDataCopyTo(FURNITURE_DATA *from, FURNITURE_DATA *to) {
  *to = *from;
}

FURNITURE_DATA *furnitureDataCopy(FURNITURE_DATA *data) {
  FURNITURE_DATA *new_data = newFurnitureData();
  furnitureDataCopyTo(data, new_data);
  return new_data;
}

STORAGE_SET *furnitureDataStore(FURNITURE_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_int(set, "capacity", data->capacity);
  store_int(set, "type",     data->type);
  return set;
}

FURNITURE_DATA *furnitureDataRead(STORAGE_SET *set) {
  FURNITURE_DATA *data = newFurnitureData();
  data->capacity = read_int(set, "capacity");
  data->type     = read_int(set, "type");
  return data;
}



//*****************************************************************************
// functions for interacting with furnitures
//*****************************************************************************
int furnitureGetCapacity(OBJ_DATA *obj) {
  FURNITURE_DATA *data = objGetTypeData(obj, "furniture");
  return data->capacity;
}

void furnitureSetCapacity(OBJ_DATA *obj, int capacity) {
  FURNITURE_DATA *data = objGetTypeData(obj, "furniture");
  data->capacity = capacity;
}

int furnitureGetType(OBJ_DATA *obj) {
  FURNITURE_DATA *data = objGetTypeData(obj, "furniture");
  return data->type;
}  

void furnitureSetType(OBJ_DATA *obj, int type) {
  FURNITURE_DATA *data = objGetTypeData(obj, "furniture");
  data->type = type;
}  

const char *furniture_names[NUM_FURNITURES] = {
  "table furniture",
  "sitting furniture"
};

const char *furnitureTypeGetName(int type) {
  return furniture_names[type];
}

int furnitureTypeGetNum(const char *type) {
  int i;
  for(i = 0; i < NUM_FURNITURES; i++)
    if(!strcasecmp(furniture_names[i], type))
      return i;
  return FURNITURE_NONE;
}



//*****************************************************************************
// furniture olc
//*****************************************************************************
#define IEDIT_FURNITURE_CAPACITY      1
#define IEDIT_FURNITURE_TYPE          2

// the resedit olc needs these declared
void iedit_furniture_menu(SOCKET_DATA *sock, FURNITURE_DATA *data) {
  send_to_socket(sock, 
		 "{g1) Capacity : {c%d\r\n"
		 "{g2) Type     : {y[{c%s{y]\r\n",
		 data->capacity,
		 furnitureTypeGetName(data->type));
}

int  iedit_furniture_chooser(SOCKET_DATA *sock, FURNITURE_DATA *data, char option) {
  switch(toupper(option)) {
  case '1':
    text_to_buffer(sock, "Enter a new weight capacity for the furniture: ");
    return IEDIT_FURNITURE_CAPACITY;
  case '2':
    olc_display_table(sock, furnitureTypeGetName, NUM_FURNITURES, 1);
    return IEDIT_FURNITURE_TYPE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool iedit_furniture_parser (SOCKET_DATA *sock, FURNITURE_DATA *data, int choice, 
			  const char *arg) {
  switch(choice) {
  case IEDIT_FURNITURE_CAPACITY: {
    int capacity = atoi(arg);
    if(capacity <= 0)
      return FALSE;
    data->capacity = capacity;
    return TRUE;
  }
  case IEDIT_FURNITURE_TYPE: {
    int type = atoi(arg);
    if(type < 0 || type >= NUM_FURNITURES || !isdigit(*arg))
      return FALSE;
    data->type = type;
    return TRUE;
  }
  default: return FALSE;
  }
}



//*****************************************************************************
// install the furniture item type
//*****************************************************************************

//
// this will need to be called by init_items() in items/items.c
void init_furniture(void) {
  item_add_type("furniture", 
  		newFurnitureData, deleteFurnitureData,
  		furnitureDataCopyTo, furnitureDataCopy, 
  		furnitureDataStore, furnitureDataRead);

  // set up the furniture OLC too
  item_add_olc("furniture", iedit_furniture_menu, iedit_furniture_chooser, 
  	       iedit_furniture_parser);
}
