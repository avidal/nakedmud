//*****************************************************************************
//
// script_edit.c
//
// This is a set of functions that come with scripts to allow them to be edited
// online in an OLC style. The functions for editing both scripts and script
// sets are included in here.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../character.h"
#include "../world.h"
#include "../zone.h"

#include "../olc2/olc.h"
#include "script.h"
#include "script_set.h"
#include "../editor/editor.h"
#include "script_editor.h"



//*****************************************************************************
// script set editing
//*****************************************************************************
#define SSEDIT_NEW      1
#define SSEDIT_DELETE   2


void ssedit_menu   (SOCKET_DATA *sock, SCRIPT_SET *set) {
  LIST *scripts = scriptSetList(set, SCRIPT_TYPE_ANY);
  SCRIPT_DATA *script = NULL;

  // show all the scripts attached
  text_to_buffer(sock, "Attached scripts:\r\n");
  while((script = listPop(scripts)) != NULL)
    send_to_socket(sock,
		   "  {y[{c%4d{y]{w %-20s {c%s\r\n",
		   scriptGetVnum(script), 
		   scriptTypeName(scriptGetType(script)),
		   scriptGetName(script));
  deleteList(scripts);

  send_to_socket(sock,
		 "\r\n"
		 "{cN{g) Attach new script\r\n"
		 "{cD{g) Delete script\r\n"
		 );
}

int  ssedit_chooser(SOCKET_DATA *sock, SCRIPT_SET *set, const char *option) {
  switch(toupper(*option)) {
  case 'N':
    text_to_buffer(sock, "Which script would you like to add (-1 for none): ");
    return SSEDIT_NEW;
  case 'D':
    text_to_buffer(sock, "Which script would you like to delete (-1 for none): ");
    return SSEDIT_DELETE;
  default: return FALSE;
  }
}

bool ssedit_parser (SOCKET_DATA *sock, SCRIPT_SET *set, int choice, 
		    const char *arg) {
  switch(choice) {
  case SSEDIT_NEW: {
    SCRIPT_DATA *script = worldGetScript(gameworld, atoi(arg));
    if(script == NULL)
      return FALSE;
    scriptSetAdd(set, scriptGetVnum(script)); 
    return TRUE;
  }
  case SSEDIT_DELETE:
    scriptSetRemove(set, atoi(arg));
    return TRUE;
  default: return FALSE;
  }
}



//*****************************************************************************
// script editing
//*****************************************************************************
#define SCEDIT_NAME        1
#define SCEDIT_TYPE        2
#define SCEDIT_ARGS        3
#define SCEDIT_NARG        4

void scedit_menu   (SOCKET_DATA *sock, SCRIPT_DATA *script) {
  send_to_socket(sock,
		 "{g[{c%d{g]\r\n"
		 "{g1) Name         : {c%s\r\n"
		 "{g2) Script type  : {c%s\r\n"
		 "{g3) Arguments    : {c%s\r\n"
		 "{g4) Num. Argument: {c%d\r\n"
		 "{g5) Script Code\r\n",
		 scriptGetVnum(script),
		 scriptGetName(script),
		 scriptTypeName(scriptGetType(script)),
		 (*scriptGetArgs(script) ? scriptGetArgs(script) : "<NONE>"),
		 scriptGetNumArg(script));
  script_display(sock, scriptGetCode(script), FALSE);
}

int  scedit_chooser(SOCKET_DATA *sock, SCRIPT_DATA *script, const char *option){
  switch(toupper(*option)) {
  case '1':
    text_to_buffer(sock, "Enter a new name for the script: ");
    return SCEDIT_NAME;
  case '2':
    olc_display_table(sock, scriptTypeName, NUM_SCRIPTS, 1);
    text_to_buffer(sock, "Pick a script type: ");
    return SCEDIT_TYPE;
  case '3':
    text_to_buffer(sock, "Enter new arguments: ");
    return SCEDIT_ARGS;
  case '5':
    socketStartEditor(sock, script_editor,scriptGetCodeBuffer(script));
    return MENU_NOCHOICE;
  case '4':
    switch(scriptGetType(script)) {
      // 0 = triggers always
      // 1 = triggers if the scriptor can see the char
    case SCRIPT_TYPE_GIVE:
    case SCRIPT_TYPE_ENTER:
      send_to_socket(sock,
		     "If the scriptor is a mob:\r\n"
		     "  0 = always triggers\r\n"
		     "  1 = triggers if the scriptor can see the char\r\n"
		     "\r\n"
		     "Enter choice : ");
      return SCEDIT_NARG;

    case SCRIPT_TYPE_RUNNABLE:
      send_to_socket(sock,
		     "Enter the minimum level that can run this script: ");
      return SCEDIT_NARG;

    case SCRIPT_TYPE_COMMAND:
      send_to_socket(sock,
		     "Control for the actual MUD command:\r\n"
		     "  0 = follow through with the MUD command\r\n"
		     "  1 = cancel the MUD command.\r\n"
		     "\r\n"
		     "Enter choice : ");
      return SCEDIT_NARG;

    default:
      send_to_socket(sock, 
		     "This script type does not use numeric arguments.\r\n"
		     "Enter choice (Q to quit) : ");
      return MENU_NOCHOICE;
    }

  default:
    return MENU_CHOICE_INVALID;
  }
}

bool scedit_parser (SOCKET_DATA *sock, SCRIPT_DATA *script, int choice, 
		    const char *arg) {
  switch(choice) {
  case SCEDIT_NAME:
    scriptSetName(script, arg);
    return TRUE;

  case SCEDIT_ARGS:
    scriptSetArgs(script, arg);
    return TRUE;

  case SCEDIT_NARG:
    switch(scriptGetType(script)) {
      // 0 = triggers always
      // 1 = triggers if the scriptor can see the char
    case SCRIPT_TYPE_GIVE:
    case SCRIPT_TYPE_ENTER:
    case SCRIPT_TYPE_EXIT:
      scriptSetNumArg(script, MIN(1, MAX(0, atoi(arg))));
      break;
      // 0 = follow through with normal command
      // 1 = cancel normal command
    case SCRIPT_TYPE_COMMAND:
      scriptSetNumArg(script, MIN(1, MAX(0, atoi(arg))));
      break;
      // narg = minimum level that can run this script
    case SCRIPT_TYPE_RUNNABLE:
      scriptSetNumArg(script, MIN(MAX_LEVEL, MAX(0, atoi(arg))));
      break;
    }
    return TRUE;

  case SCEDIT_TYPE: {
    int num = atoi(arg);
    if(num < 0 || num >= NUM_SCRIPTS)
      return FALSE;
    else {
      scriptSetType(script, num);
      // reset arguments
      if(scriptGetArgs(script))
	scriptSetArgs(script, "");
      scriptSetNumArg(script, 0);
      return TRUE;
    }
  }

  default: 
    return FALSE;
  }
}


COMMAND(cmd_scedit) {
  ZONE_DATA *zone;
  SCRIPT_DATA *script;
  script_vnum vnum;

  // we need a vnum
  if(!arg || !*arg)
    send_to_char(ch, "Please supply the vnum of a script you wish to edit.\r\n");
  else {
    vnum = atoi(arg);

    // make sure there is a corresponding zone ...
    if((zone = worldZoneBounding(gameworld, vnum)) == NULL)
      send_to_char(ch, "No zone exists that contains the given vnum.\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    else {
      // find the script
      script = zoneGetScript(zone, vnum);

      // make our script
      if(script == NULL) {
	script = newScript();
	scriptSetVnum(script, vnum);
	scriptSetName(script, "An Unfinished Script");
	scriptSetCode(script, "# script code goes here\n"
		              "# make sure to comment it with pounds (#)\n");
	zoneAddScript(zone, script);
      }

      do_olc(charGetSocket(ch), scedit_menu, scedit_chooser, scedit_parser,
	     scriptCopy, scriptCopyTo, deleteScript, save_world, script);
    }
  }
}
