//*****************************************************************************
//
// dedit.c
//
// Dialogs are sets of reactions to speech that NPCs can have tagged onto them.
// This is these are the utilities for editing dialogs in OLC.
//
//*****************************************************************************

#include "../mud.h"
#include "../dialog.h"
#include "../world.h"
#include "../zone.h"
#include "../socket.h"
#include "../character.h"

#include "../editor/editor.h"

#include "olc.h"



//*****************************************************************************
// olc for response data
//*****************************************************************************
#define RESPEDIT_KEYWORDS 1

void respedit_menu(SOCKET_DATA *sock, RESPONSE_DATA *response) {
  send_to_socket(sock,
		 "{g1) Keywords\r\n"
		 "{c%s\r\n"
		 "{g2) Message\r\n"
		 "{c%s\r\n",
		 responseGetKeywords(response), 
		 responseGetMessage(response)
		 );
}

int respedit_chooser(SOCKET_DATA *sock, RESPONSE_DATA *response, 
		     const char *option) {
  switch(toupper(*option)) {
  case '1':
    text_to_buffer(sock, "Enter a new list of keywords: ");
    return RESPEDIT_KEYWORDS;
  case '2':
    socketStartEditor(sock, dialog_editor, responseGetMessageBuffer(response));
    return MENU_NOCHOICE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool respedit_parser(SOCKET_DATA *sock, RESPONSE_DATA *response, int choice, 
		     const char *arg) {
  switch(choice) {
  case RESPEDIT_KEYWORDS:
    responseSetKeywords(response, arg);
    return TRUE;
  default: return FALSE;
  }
}



//*****************************************************************************
// olc for dialog data
//*****************************************************************************
#define DEDIT_TITLE      1
#define DEDIT_EDIT       2
#define DEDIT_DELETE     3

void dedit_menu(SOCKET_DATA *sock, DIALOG_DATA *dialog) {
  int i, entries = dialogGetSize(dialog);
  int half_entries = entries/2 + (entries % 2 == 1);
  // entries are printed side by side... these are used to hold 'em
  char left_buf[SMALL_BUFFER];
  char right_buf[SMALL_BUFFER];

  send_to_socket(sock,
		 "{gT) Dialog title\r\n"
		 "{c%s\r\n"
		 "{gG) Greet/Approach message\r\n"
		 "{c%s\r\n"
		 "\r\n"
		 "{gResponse Entries: {c%d\r\n", 
		 dialogGetName(dialog), 
		 (*dialogGetGreet(dialog) ? dialogGetGreet(dialog) : "<NONE>"), 
		 entries);

  // print out responses, two to a line
  for(i = 0; i < half_entries; i++) {
    RESPONSE_DATA *left_entry = dialogGetNum(dialog, i);
    sprintf(left_buf, "  {g%2d) {c%s", i, responseGetKeywords(left_entry));
    if(i + half_entries >= entries)
      *right_buf = '\0';
    else {
      RESPONSE_DATA *right_entry = dialogGetNum(dialog, i + half_entries);
      sprintf(right_buf, "  {g%2d) {c%s", 
	      i + half_entries, responseGetKeywords(right_entry));
    }
    send_to_socket(sock, "%-35s %s\r\n", left_buf, right_buf);
  }

  send_to_socket(sock, "\r\n"
		 "  {gE) edit entry\r\n"
		 "  {gN) new entry\r\n"
		 "  {gD) delete entry\r\n");
}


int dedit_chooser(SOCKET_DATA *sock, DIALOG_DATA *dialog, const char *option) {
  switch(toupper(*option)) {
  case 'T':
    text_to_buffer(sock, "Enter a new title for the dialog: ");
    return DEDIT_TITLE;
  case 'G':
    socketStartEditor(sock, dialog_editor, dialogGetGreetBuffer(dialog));
    return MENU_NOCHOICE;
  case 'E':
    text_to_buffer(sock, "Which response would you like to edit: ");
    return DEDIT_EDIT;
  case 'N': {
    RESPONSE_DATA *response = newResponse();
    dialogPut(dialog, response);
    do_olc(sock, respedit_menu, respedit_chooser, respedit_parser, NULL, NULL,
	   NULL, NULL, response);
    return MENU_NOCHOICE;
  case 'D':
    text_to_buffer(sock, "Which response would you like to delete: ");
    return DEDIT_DELETE;
  default: return MENU_CHOICE_INVALID;
  }
  }
}


bool dedit_parser(SOCKET_DATA *sock, DIALOG_DATA *dialog, int choice, 
		  const char *arg) {
  switch(choice) {
  case DEDIT_TITLE:
    dialogSetName(dialog, arg);
    return TRUE;

  case DEDIT_EDIT: {
    if(!isdigit(*arg))
      return FALSE;
    RESPONSE_DATA *response = dialogGetNum(dialog, atoi(arg));
    if(response) {
      do_olc(sock, respedit_menu, respedit_chooser, respedit_parser, NULL, NULL,
	     NULL, NULL, response);
    }
    return TRUE;
  }

  case DEDIT_DELETE: {
    if(!isdigit(*arg))
      return FALSE;
    RESPONSE_DATA *response = dialogGetNum(dialog, atoi(arg));
    if(response) {
      dialogRemove(dialog, response);
      deleteResponse(response);
    }
    return TRUE;
  }

  default: return FALSE;
  }
}

// saves a dialog
void save_dialog(DIALOG_DATA *dialog) {
  worldSaveDialog(gameworld, dialog);
}

COMMAND(cmd_dedit) {
  ZONE_DATA *zone;
  DIALOG_DATA *dialog;
  int vnum;

  // we need to know which dialog...
  if(!arg || !*arg || !isdigit(*arg))
    send_to_char(ch, "Which dialog did you want to edit?\r\n");
  else {
    vnum = atoi(arg);

    // make sure there is a corresponding zone ...
    if((zone = worldZoneBounding(gameworld, vnum)) == NULL)
      send_to_char(ch, "No zone exists that contains the given vnum.\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    else {
      // find the dialog
      dialog = zoneGetDialog(zone, vnum);

      // make our dialog
      if(dialog == NULL) {
	dialog = newDialog();
	dialogSetVnum (dialog, vnum);
	dialogSetName (dialog, "An Unfinished Dialog");
	dialogSetGreet(dialog, "Hi! This dialog is unfinished.");
	zoneAddDialog(zone, dialog);
      }
      
      do_olc(charGetSocket(ch), dedit_menu, dedit_chooser, dedit_parser,
	     dialogCopy, dialogCopyTo, deleteDialog, save_dialog, dialog);
    }
  }
}
