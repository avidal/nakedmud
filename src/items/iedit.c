//*****************************************************************************
//
// iedit.c
//
// Although it is not core to the functioning of items, we will more likely
// than not want to be able to edit their type information in OLC. This will
// require a parser, a chooser, and a menu for editing the type information.
// Item types can add in their OLC functions via this framework. iedit uses
// the OLC2 framework. For more detailed instructions on how to set up an OLC,
// users should refer to the OLC2 source headers.
//
// NOTE: the menu, chooser, and parser should only take the item-specific data,
//       and not the WHOLE object! It has been set up this way to encourage
//       people to localize all of the data associated with one item type.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "items.h"
#include "iedit.h"

#include "../olc2/olc.h"



//*****************************************************************************
// local functions, variables, and datastructures
//*****************************************************************************
// used to store the different sub-olc functions for different item types
HASHTABLE *item_olc_table = NULL;

typedef struct item_olc_data {
  void    (* menu)(SOCKET_DATA *sock, void *data);
  int  (* chooser)(SOCKET_DATA *sock, void *data, const char *option);
  bool  (* parser)(SOCKET_DATA *sock, void *data, int choice, const char *arg);
} ITEM_OLC_DATA;

ITEM_OLC_DATA *newItemOLC(void *menu, void *chooser, void *parser) {
  ITEM_OLC_DATA *data = malloc(sizeof(ITEM_OLC_DATA));
  data->menu    = menu;
  data->chooser = chooser;
  data->parser  = parser;
  return data;
}

void deleteItemOLC(ITEM_OLC_DATA *data) {
  free(data);
}



//*****************************************************************************
// implementation of iedit.h
//*****************************************************************************
#define IEDIT_EDIT        1
#define IEDIT_DELETE      2


//
// list off all the item types available for editing. Print the type in green
// if the item currently is not of that type. Print the type in yellow if the
// item is of that type.
void iedit_show_types(SOCKET_DATA *sock, OBJ_DATA *obj) {
  LIST *list = itemTypeList();
  LIST_ITERATOR *list_i = newListIterator(list);
  char *type = NULL;
  int col = 0;

  text_to_buffer(sock, "{wEditable item types:\r\n");
  ITERATE_LIST(type, list_i) {
    send_to_socket(sock, "  %s%-14s%s", (objIsType(obj, type) ? "{y" : "{g"),
		   type, ((col != 0 && col % 4 == 0) ? "\r\n": "   "));
    col++;
  }
  deleteListIterator(list_i);
  deleteListWith(list, free);

  if(col % 4 != 0) text_to_buffer(sock, "\r\n");
}

void item_add_olc(const char *type, void *menu, void *chooser, void *parser) {
  ITEM_OLC_DATA *data = NULL;
  // check to see if we already have functions for this item type installed
  if( (data = hashRemove(item_olc_table, type)) != NULL)
    deleteItemOLC(data);
  data = newItemOLC(menu, chooser, parser);
  hashPut(item_olc_table, type, data);
}

void iedit_menu(SOCKET_DATA *sock, OBJ_DATA *obj) {
  iedit_show_types(sock, obj);
  text_to_buffer(sock, 
		 "\r\n"
		 "    {gE) edit type\r\n"
		 "    D) delete type\r\n");
}

int  iedit_chooser(SOCKET_DATA *sock, OBJ_DATA *obj, const char *option) {
  switch(toupper(*option)) {
  case 'E':
    text_to_buffer(sock, "Which item type would you like to edit: ");
    return IEDIT_EDIT;
  case 'D':
    text_to_buffer(sock, "Which item type would you like to delete: ");
    return IEDIT_DELETE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool iedit_parser(SOCKET_DATA *sock, OBJ_DATA *obj,int choice, const char *arg){
  switch(choice) {
  case IEDIT_EDIT: {
    ITEM_OLC_DATA *funcs = NULL;
    if(!objIsType(obj, arg))
      objSetType(obj, arg);
    // pull up the OLC functions and see if we can edit the type
    if((funcs = hashGet(item_olc_table, arg)) != NULL) {
      void *item_data = objGetTypeData(obj, arg);
      do_olc(sock, funcs->menu, funcs->chooser, funcs->parser,
	     NULL, NULL, NULL, NULL, item_data);
    }
    return TRUE;
  }
  case IEDIT_DELETE:
    objDeleteType(obj, arg);
    return TRUE;
  default: return FALSE;
  }
}



//*****************************************************************************
// initialization of item olc
//*****************************************************************************
void init_item_olc() {
  item_olc_table = newHashtable();
}
