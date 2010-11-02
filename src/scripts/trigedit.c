//*****************************************************************************
//
// trigedit.c
//
// contains the functions neccessary for using OLC on triggers.
//
//*****************************************************************************
#include "../mud.h"
#include "../socket.h"
#include "../character.h"
#include "../utils.h"
#include "../world.h"
#include "scripts.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../editor/editor.h"
#include "script_editor.h"

#ifdef MODULE_OLC2
#include "../olc2/olc.h"



//*****************************************************************************
// trigger lists
//*****************************************************************************
#define TRIGLIST_NEW     1
#define TRIGLIST_DELETE  2

void trigger_list_menu(SOCKET_DATA *sock, LIST *triggers) {
  if(listSize(triggers) > 0) {
    send_to_socket(sock, "{wCurrent triggers:\r\n");
    LIST_ITERATOR *trig_i = newListIterator(triggers);
    char            *trig = NULL;
    ITERATE_LIST(trig, trig_i) {
      send_to_socket(sock, "  {g%s\r\n", trig);
    } deleteListIterator(trig_i);
  }

  send_to_socket(sock, 
		 "\r\n"
		 "  {gN) Add new trigger\r\n"
		 "  {gD) Delete trigger\r\n");
}

int trigger_list_chooser(SOCKET_DATA *sock, LIST *triggers, const char *option){
  switch(toupper(*option)) {
  case 'N':
    send_to_socket(sock, "Enter key for trigger to attach: ");
    return TRIGLIST_NEW;
  case 'D':
    send_to_socket(sock, "Enter key for trigger to delete: ");
    return TRIGLIST_DELETE;
  default: 
    return MENU_CHOICE_INVALID;
  }
}

bool trigger_list_parser(SOCKET_DATA *sock, LIST *triggers, int choice,
		    const char *arg) {
  switch(choice) {
  case TRIGLIST_NEW:
    if(!listGetWith(triggers, arg, strcasecmp))
      listPutWith(triggers, strdup(arg), strcasecmp);
    return TRUE;
  case TRIGLIST_DELETE: {
    char *found = listRemoveWith(triggers, arg, strcasecmp);
    if(found) free(found);
    return TRUE;
  }
  default: return FALSE;
  }
}



//*****************************************************************************
// triggers
//*****************************************************************************
#define TEDIT_NAME       1
#define TEDIT_TYPE       2

struct trigger_type_usable_list {
  char    *type;
  char *used_by;
};

// a table of allowable trigger types
struct trigger_type_usable_list trigger_types[] = {
  { "speech",         "mob, room" },
  { "greet",          "mob"       },
  { "enter",          "mob, room" },
  { "exit",           "mob, room" },
  { "self_enter",     "mob"       },
  { "self_exit",      "mob"       },
  { "drop",           "obj, room" },
  { "get",            "obj, room" },
  { "give",           "obj, mob"  },
  { "receive",        "mob"       },
  { "wear",           "obj, mob"  },
  { "remove",         "obj, mob"  },
  { "reset",          "room"      },
  { "look",           "obj, mob, room" },
  { "open",           "obj, room" },
  { "close",          "obj, room" },
  { "to_game",        "obj, mob, room" },
  { "", "" },
};

int num_trig_types(void) {
  static int num = -1;
  // we need to calculate...
  if(num == -1)
    do { num++; } while(*trigger_types[num].type);
  return num;
}

// returns the trigger type name, for the given number
const char *triggerTypeGetName(int num) {
  return trigger_types[num].type;
}

// returns the types of things the trigger can be attached to
const char *triggerTypeGetUsedBy(int num) {
  return trigger_types[num].used_by;
}

// returns a buffer that contains the name and used-by list for a trigger
const char *triggerTypeGetNameAndUsedBy(int num) {
  static char buf[100];
  sprintf(buf, "%-20s %s", triggerTypeGetName(num), triggerTypeGetUsedBy(num));
  return buf;
}

void tedit_menu(SOCKET_DATA *sock, TRIGGER_DATA *trigger) {
  send_to_socket(sock,
		 "{g[{c%s{g]\r\n"
		 "{g1) Name        : {c%s\r\n"
		 "{g2) Trigger type: {c%s\r\n"
		 "{g3) Script Code\r\n",
		 triggerGetKey(trigger),
		 triggerGetName(trigger),
		 (*triggerGetType(trigger) ? triggerGetType(trigger):"<NONE>"));
  script_display(sock, triggerGetCode(trigger), FALSE);
}

int tedit_chooser(SOCKET_DATA *sock, TRIGGER_DATA *trigger, const char *option){
  switch(toupper(*option)) {
  case '1':
    send_to_socket(sock, "Enter trigger name: ");
    return TEDIT_NAME;
  case '2':
    send_to_socket(sock, "      {wType                 Usable By\r\n");
    send_to_socket(sock, "      {y-----------------------------------\r\n");
    olc_display_table(sock, triggerTypeGetNameAndUsedBy, num_trig_types(), 1);
    send_to_socket(sock, "\r\nEnter trigger type: ");
    return TEDIT_TYPE;
  case '3':
    socketStartEditor(sock, script_editor, triggerGetCodeBuffer(trigger));
    return MENU_NOCHOICE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool tedit_parser(SOCKET_DATA *sock, TRIGGER_DATA *trigger, int choice,
		  const char *arg) {
  switch(choice) {
  case TEDIT_NAME:
    triggerSetName(trigger, arg);
    return TRUE;

  case TEDIT_TYPE: {
    int num = atoi(arg);
    if(num < 0 || num >= num_trig_types())
      return FALSE;
    else {
      triggerSetType(trigger, triggerTypeGetName(num));
      return TRUE;
    }
  }

  default: 
    return FALSE;
  }
}

void save_trigger(TRIGGER_DATA *trigger) {
  format_script_buffer(triggerGetCodeBuffer(trigger));
  worldSaveType(gameworld, "trigger", triggerGetKey(trigger));
}

COMMAND(cmd_tedit) {
  ZONE_DATA       *zone = NULL;
  TRIGGER_DATA *trigger = NULL;

  // we need a key
  if(!arg || !*arg)
    send_to_char(ch,"Please supply the key of a trigger you wish to edit.\r\n");
  else if(key_malformed(arg))
    send_to_char(ch, "You entered a malformed trigger key.\r\n");
  else {
    char name[SMALL_BUFFER], locale[SMALL_BUFFER];
    if(!parse_worldkey_relative(ch, arg, name, locale))
      send_to_char(ch, "Which trigger are you trying to edit?\r\n");
    else if( (zone = worldGetZone(gameworld, locale)) == NULL)
      send_to_char(ch, "No such zone exists.\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit that zone.\r\n");
    else {
      // pull up the script
      trigger = worldGetType(gameworld, "trigger", get_fullkey(name, locale));
      if(trigger == NULL) {
	trigger = newTrigger();
	triggerSetName(trigger, "An Unfinished Trigger");
	triggerSetCode(trigger, "# trigger code goes here\n"
		                "# make sure to comment it with pounds (#)\n");
	worldPutType(gameworld, "trigger", get_fullkey(name, locale), trigger);
      }

      do_olc(charGetSocket(ch), tedit_menu, tedit_chooser, tedit_parser,
	     triggerCopy, triggerCopyTo, deleteTrigger, save_trigger, trigger);
    }
  }
}

#endif // MODULE_OLC2
