#ifndef IEDIT_H
#define IEDIT_H
//*****************************************************************************
//
// iedit.h
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

void item_add_olc(const char *type, void *menu, void *chooser, void *parser);

//
// these functions will be needed by oedit
void iedit_menu(SOCKET_DATA *sock, OBJ_DATA *obj);
int  iedit_chooser(SOCKET_DATA *sock, OBJ_DATA *obj, char option);
bool iedit_parser(SOCKET_DATA *sock, OBJ_DATA *obj,int choice, const char *arg);

#endif // IEDIT_H
