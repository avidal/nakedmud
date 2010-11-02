#ifndef __ITEMS_H
#define __ITEMS_H
//*****************************************************************************
//
// items.h
//
// contains numbers of all of the different item types and subtypes. 
//
//*****************************************************************************
#define NUM_OBJ_VALUES          5
#define VAL_FURNITURE_CAPACITY  0

#define VAL_PORTAL_DESTINATION  0  // what room does the portal lead to?

#define VAL_CONTAINER_CLOSABLE  0  // are we closable?
#define VAL_CONTAINER_KEY       1  // what is the vnum of our key?
#define VAL_CONTAINER_PICK_DIFF 2  // how hard is it to pick our lock?
#define VAL_CONTAINER_CLOSED    3  // are we closed currently?
#define VAL_CONTAINER_LOCKED    4  // are we locked currently?

#define ITEM_OTHER              0  // anything not specified
#define ITEM_PORTAL             1  // items for moving around the world
#define ITEM_WORN               2  // clothes and other protective gear
#define ITEM_WEAPON             3  // things that hurt people
#define ITEM_FURNITURE          4  // something people can sit at/on
#define ITEM_CONTAINER          5  // something for storing other items
#define NUM_ITEM_TYPES          6

/* a placeholder for misc items (baubles, trinkets, quest items, etc...) */
#define NUM_OTHERS            0

/* a placeholder for all portals */
#define NUM_PORTALS           0

/* a placeholder for containers */
#define NUM_CONTAINERS        0

/* two main furniture types: those we can sit ON and those we can sit AT */
#define FURNITURE_AT          0
#define FURNITURE_ON          1
#define NUM_FURNITURES        2

/* worn items: clothes and protective gear */
#define WORN_SHIRT            0
#define WORN_GLOVES           1
#define WORN_LEFT_GLOVE       2
#define WORN_RIGHT_GLOVE      3
#define WORN_EARRINGS         4
#define WORN_EARRING          5
#define WORN_RING             6
#define NUM_WORN              7

/* weapons: stuff that's made to hurt people */
#define WEAPON_DAGGER         0
#define NUM_WEAPONS           1


const char *itemGetType(int item_class);
const char *itemGetSubtype(int item_class, int subtype);

int numItemTypes();
int numItemSubtypes(int item_class);

const char *wornGetPositions(int worntype);


//
// functions for interacting with special item types. Nothing really OLCish,
// but just stuff to get information about the object.
//
room_vnum portalGetDestination(OBJ_DATA *portal);
int       furnitureGetCapacity(OBJ_DATA *furniture);

bool      containerIsClosable(OBJ_DATA *container);
bool      containerIsClosed  (OBJ_DATA *container);
bool      containerIsLocked  (OBJ_DATA *container);
obj_vnum  containerGetKey    (OBJ_DATA *container);
void      containerSetClosed (OBJ_DATA *container, bool closed);
void      containerSetLocked (OBJ_DATA *container, bool locked);

#endif // __ ITEMS_H
