//*****************************************************************************
//
// items.c
//
// contains definitional information for various item types. Some of the more
// elaborate item types (e.g. worn, and weapons) may do well to have their
// information stored in separate files.
//
//*****************************************************************************

#include "mud.h"
#include "object.h"
#include "items.h"


const char *item_type_names[NUM_ITEM_TYPES] = {
  "other",
  "portal",
  "worn",
  "weapon",
  "furniture",
  "container"
};

struct worn_data {
  char *name;
  char *positions;
};

struct worn_data worn_info[NUM_WORN] = {
  { "shirt",        "torso",                  },
  { "gloves",       "left hand, right hand",  },
  { "left glove",   "left hand",              },
  { "right glove",  "right hand",             },
  { "earrings",     "ear, ear",               },
  { "earring",      "ear",                    },
  { "ring",         "finger",                 }
};

const char *weapon_names[NUM_WEAPONS] = {
  "dagger",
};

const char *furniture_names[NUM_FURNITURES] = {
  "table furniture",
  "sitting furniture"
};

const char *itemGetType(int item_class) {
  return item_type_names[item_class];
}

const char *itemGetSubtype(int item_class, int subtype) {
  switch(item_class) {
  case ITEM_WORN:      return worn_info[subtype].name;
  case ITEM_WEAPON:    return weapon_names[subtype];
  case ITEM_FURNITURE: return furniture_names[subtype];
  // all types with no subtypes
  default:  return NULL;
  }
}

int numItemTypes() {
  return NUM_ITEM_TYPES;
}

int numItemSubtypes(int item_class) {
  switch(item_class) {
  case ITEM_WORN:      return NUM_WORN;
  case ITEM_WEAPON:    return NUM_WEAPONS;
  case ITEM_FURNITURE: return NUM_FURNITURES;
  // all types with no subtypes
  default:            return 0;
  }
}

const char *wornGetPositions(int worntype) {
  return worn_info[worntype].positions;
}

room_vnum portalGetDestination(OBJ_DATA *portal) {
  return objGetVal(portal, VAL_PORTAL_DESTINATION);
}

int       furnitureGetCapacity(OBJ_DATA *furniture) {
  return objGetVal(furniture, VAL_FURNITURE_CAPACITY);
}

bool      containerIsClosable(OBJ_DATA *container) {
  return objGetVal(container, VAL_CONTAINER_CLOSABLE);
}

bool      containerIsClosed(OBJ_DATA *container) {
  return objGetVal(container, VAL_CONTAINER_CLOSED);
}

bool      containerIsLocked(OBJ_DATA *container) {
  return objGetVal(container, VAL_CONTAINER_LOCKED);
}

obj_vnum  containerGetKey  (OBJ_DATA *container) {
  return objGetVal(container, VAL_CONTAINER_KEY);
}

void      containerSetClosed(OBJ_DATA *container, bool closed) {
  objSetVal(container, VAL_CONTAINER_CLOSED, closed);
}

void      containerSetLocked(OBJ_DATA *container, bool locked) {
  objSetVal(container, VAL_CONTAINER_LOCKED, locked);
}
