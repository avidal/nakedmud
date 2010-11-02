//*****************************************************************************
//
// redit.c
//
// redit (room edit) is a utility to allow builders to edit rooms within the
// game. Contains the functions for editing datastructures specific to rooms
// (rooms, exits, room resets).
//
//*****************************************************************************
#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../character.h"
#include "../world.h"
#include "../zone.h"
#include "../room.h"
#include "../exit.h"
#include "../room_reset.h"
#include "../object.h"

#include "olc.h"
#include "olc_submenus.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/script.h"
#include "../editor/editor.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_TIME
#include "../time/mudtime.h"
#endif



//*****************************************************************************
// functions for printing room reset data
//*****************************************************************************

// the resedit olc needs these declared
void rrledit_menu(SOCKET_DATA *sock, LIST *list);
int  rrledit_chooser(SOCKET_DATA *sock, LIST *list, const char *option);
bool rrledit_parser(SOCKET_DATA *sock, LIST *list, int choice, const char *arg);

const char *write_reset_arg(int type, const char *arg) {
  static char buf[SMALL_BUFFER];
  OBJ_DATA *obj  = worldGetObj(gameworld, atoi(arg));
  CHAR_DATA *mob = worldGetMob(gameworld, atoi(arg));
  int pos        = atoi(arg);
  switch(type) { 
  case RESET_LOAD_OBJECT:
    sprintf(buf, "load %s", (obj ? objGetName(obj) : "{RNOTHING{c"));
    break;
  case RESET_LOAD_MOBILE:
    sprintf(buf, "load %s", (mob ? charGetName(mob) : "{RNOBODY{c"));
    break;
  case RESET_POSITION:
    sprintf(buf, "change position to %s", 
	    (pos < 0 || pos >= NUM_POSITIONS ? "{RNOTHING{c":posGetName(pos)));
    break;
  case RESET_FIND_MOBILE:
    sprintf(buf, "find %s", (mob ? charGetName(mob) : "{RNOBODY{c"));
    break;
  case RESET_FIND_OBJECT:
    sprintf(buf, "find %s", (obj ? objGetName(obj) : "{RNOTHING{c"));
    break;
  case RESET_PURGE_MOBILE:
    sprintf(buf, "purge %s", (mob ? charGetName(mob) : "{RNOBODY{c"));
    break;
  case RESET_PURGE_OBJECT:
    sprintf(buf, "purge %s", (obj ? objGetName(obj) : "{RNOTHING{c"));
    break;
  case RESET_OPEN:
    sprintf(buf, "open/unlock dir %s or container", arg);
    break;
  case RESET_CLOSE:
    sprintf(buf, "close/unlock dir %s or container", arg);
    break;
  case RESET_LOCK:
    sprintf(buf, "close/lock dir %s or container", arg);
    break;
  default:
    sprintf(buf, "UNFINISHED OLC");
    break;
  }
  return buf;
}

int indent_line(char *buf, int buflen, int indent) {
  if(indent > 0) {
    char fmt[20];
    sprintf(fmt, "%%-%ds", indent);
    return snprintf(buf, buflen, fmt, " ");
  }
  return 0;
}

int write_reset_buf(RESET_DATA *reset, char *buf, int buflen, int indent,
		    bool indent_first) {
  int i = 0;

  if(indent_first)
    i += indent_line(buf, buflen, indent);
  i += snprintf(buf+i, buflen-i,
		"{c%s with {w%d%% {cchance {w%d {ctime%s (max {w%d{c, rm. {w%d{c)\r\n",
		write_reset_arg(resetGetType(reset), resetGetArg(reset)),
		resetGetChance(reset),
		resetGetTimes(reset),
		(resetGetTimes(reset) == 1 ? "" : "s"),
		resetGetMax(reset),
		resetGetRoomMax(reset));

  // if we've got ONs, then print 'em all out as well
  if(listSize(resetGetOn(reset)) > 0) {
    i += indent_line(buf+i, buflen-i, indent);
    i += snprintf(buf+i, buflen-i, "{yon it: \r\n");
    LIST_ITERATOR *list_i = newListIterator(resetGetOn(reset));
    RESET_DATA     *next  = NULL;
    ITERATE_LIST(next, list_i)
      i += write_reset_buf(next, buf+i, buflen-i, indent+2, TRUE);
    deleteListIterator(list_i);
  }

  // if we've got INs, then print 'em all out as well
  if(listSize(resetGetIn(reset)) > 0) {
    i += indent_line(buf+i, buflen-i, indent);
    i += snprintf(buf+i, buflen-i, "{yin it: \r\n");
    LIST_ITERATOR *list_i = newListIterator(resetGetIn(reset));
    RESET_DATA     *next  = NULL;
    ITERATE_LIST(next, list_i)
      i += write_reset_buf(next, buf+i, buflen-i, indent+2, TRUE);
    deleteListIterator(list_i);
  }

  // if we've got THENs, print 'em all out as well
  if(listSize(resetGetThen(reset)) > 0) {
    i += indent_line(buf+i, buflen-i, indent);
    i += snprintf(buf+i, buflen-i, "{ywhen successful, also: \r\n");
    LIST_ITERATOR *list_i = newListIterator(resetGetThen(reset));
    RESET_DATA     *next  = NULL;
    ITERATE_LIST(next, list_i)
      i += write_reset_buf(next, buf+i, buflen-i, indent+2, TRUE);
    deleteListIterator(list_i);
  }
  return i;
}

const char *write_reset(RESET_DATA *reset, int indent, bool indent_first) {
  static char buf[MAX_BUFFER];
  write_reset_buf(reset, buf, MAX_BUFFER, indent, indent_first);
  return buf;
}



//*****************************************************************************
// room reset editing
//*****************************************************************************
#define RESEDIT_TYPE       1
#define RESEDIT_TIMES      2
#define RESEDIT_CHANCE     3
#define RESEDIT_MAX        4
#define RESEDIT_ROOM_MAX   5
#define RESEDIT_ARGUMENT   6

void resedit_menu(SOCKET_DATA *sock, RESET_DATA *data) {
  send_to_socket(sock,
		 "{g1) Type:       {c%s\r\n"
		 "{g2) Times:      {c%d\r\n"
		 "{g3) Chance:     {c%d\r\n"
		 "{g4) Max:        {c%d\r\n"
		 "{g5) Room Max:   {c%d\r\n"
		 "{g6) Argument:   {c%s\r\n"
		 "{g7) Load on menu\r\n"
		 "{g8) Load in menu\r\n"
		 "{g9) Success menu\r\n"
		 "---------------------------------------------------------\r\n"
		 "%s",
		 resetTypeGetName(resetGetType(data)),
		 resetGetTimes(data),
		 resetGetChance(data),
		 resetGetMax(data),
		 resetGetRoomMax(data),
		 resetGetArg(data),
		 write_reset(data, 0, FALSE)
		 );
}

int resedit_chooser(SOCKET_DATA *sock, RESET_DATA *data, const char *option) {
  switch(toupper(*option)) {
  case '1':
    olc_display_table(sock, resetTypeGetName, NUM_RESETS, 1);
    text_to_buffer(sock, "Pick a reset type: ");
    return RESEDIT_TYPE;
  case '2':
    text_to_buffer(sock, "How many times should the reset execute: ");
    return RESEDIT_TIMES;
  case '3':
    text_to_buffer(sock, "What is the success chance of the reset: ");
    return RESEDIT_CHANCE;
  case '4':
    text_to_buffer(sock, "What is the max that can exit in game (0 = no limit): ");
    return RESEDIT_MAX;
  case '5':
    text_to_buffer(sock, "What is the max that can exit in room (0 = no limit): ");
    return RESEDIT_ROOM_MAX;
  case '6':
    text_to_buffer(sock, "What is the reset argument (i.e. obj vnum, direction, etc...): ");
    return RESEDIT_ARGUMENT;
  case '7':
    do_olc(sock, rrledit_menu, rrledit_chooser, rrledit_parser, 
	   NULL, NULL, NULL, NULL, resetGetOn(data));
    return MENU_NOCHOICE;
  case '8':
    do_olc(sock, rrledit_menu, rrledit_chooser, rrledit_parser, 
	   NULL, NULL, NULL, NULL, resetGetIn(data));
    return MENU_NOCHOICE;
  case '9':
    do_olc(sock, rrledit_menu, rrledit_chooser, rrledit_parser, 
	   NULL, NULL, NULL, NULL, resetGetThen(data));
    return MENU_NOCHOICE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool resedit_parser(SOCKET_DATA *sock, RESET_DATA *data, int choice, 
		    const char *arg){
  switch(choice) {
  case RESEDIT_TYPE: {
    int type = atoi(arg);
    if(type < 0 || type >= NUM_RESETS)
      return FALSE;
    resetSetType(data, type);
    // set all of the data to defaults
    resetSetArg(data, "");
    resetSetChance(data, 100);
    resetSetMax(data, 0);
    resetSetRoomMax(data, 0);
    resetSetTimes(data, 1);
    return TRUE;
  }

  case RESEDIT_TIMES: {
    int times = atoi(arg);
    if(times < 1)
      return FALSE;
    resetSetTimes(data, times);
    return TRUE;
  }

  case RESEDIT_CHANCE: {
    int chance = atoi(arg);
    if(chance < 1 || chance > 100)
      return FALSE;
    resetSetChance(data, chance);
    return TRUE;
  }

  case RESEDIT_MAX: {
    int max = atoi(arg);
    if(max < 0)
      return FALSE;
    resetSetMax(data, max);
    return TRUE;
  }

  case RESEDIT_ROOM_MAX: {
    int rmax = atoi(arg);
    if(rmax < 0)
      return FALSE;
    resetSetRoomMax(data, rmax);
    return TRUE;
  }

  case RESEDIT_ARGUMENT:
    resetSetArg(data, arg);
    return TRUE;

  default: return FALSE;
  }
}



//*****************************************************************************
// room reset list editing
//*****************************************************************************
#define RRLEDIT_EDIT       1
#define RRLEDIT_DELETE     2


void rrledit_menu(SOCKET_DATA *sock, LIST *list) {
  LIST_ITERATOR *res_i = newListIterator(list);
  RESET_DATA    *reset = NULL;
  int count = 0;

  send_to_socket(sock,
		 "{wCurrent resets:\r\n");
  ITERATE_LIST(reset, res_i) {
    send_to_socket(sock, " {g%2d) %s", count, write_reset(reset, 5, FALSE));
    count++;
  }
  deleteListIterator(res_i);

  send_to_socket(sock, "\r\n"
		 "  {gE) edit entry\r\n"
		 "  {gN) new entry\r\n"
		 "  {gD) delete entry\r\n"
		 );
}


int rrledit_chooser(SOCKET_DATA *sock, LIST *list, const char *option) {
  switch(toupper(*option)) {
  case 'N': {
    RESET_DATA *data = newReset();
    listQueue(list, data);
    do_olc(sock, resedit_menu, resedit_chooser, resedit_parser, 
	   NULL, NULL, NULL, NULL, data);
    return MENU_NOCHOICE;
  }
  case 'E':
    if(listSize(list) == 0)
      return MENU_CHOICE_INVALID;
    text_to_buffer(sock, "Which entry do you want to edit (-1 for none): ");
    return RRLEDIT_EDIT;
  case 'D':
    text_to_buffer(sock, "Which entry do you want to delete: ");
    return RRLEDIT_DELETE;
  default:
    return MENU_CHOICE_INVALID;
  }
}


bool rrledit_parser(SOCKET_DATA *sock, LIST *list, int choice, const char *arg){
  switch(choice) {
  case RRLEDIT_EDIT: {
    RESET_DATA *reset = NULL;
    if(atoi(arg) == NOTHING)
      return TRUE;
    if(!isdigit(*arg) || (reset = listGet(list, atoi(arg))) == NULL)
      return FALSE;
    do_olc(sock, resedit_menu, resedit_chooser, resedit_parser, 
	   NULL, NULL, NULL, NULL, reset);
    return TRUE;
  }
  case RRLEDIT_DELETE: {
    RESET_DATA *reset = NULL;
    if(!isdigit(*arg) || (reset = listGet(list, atoi(arg))) == NULL)
      return FALSE;
    listRemove(list, reset);
    deleteReset(reset);
    return TRUE;
  }
  default: return FALSE;
  }
}



//*****************************************************************************
// exit editing functions
//*****************************************************************************
#define EXEDIT_KEYWORDS    1
#define EXEDIT_LEAVE       2
#define EXEDIT_ENTER       3
#define EXEDIT_TO          4
#define EXEDIT_KEY         5
#define EXEDIT_PICK        6
#define EXEDIT_SPOT        7
#define EXEDIT_NAME        8
#define EXEDIT_OPPOSITE    9

void exedit_menu(SOCKET_DATA *sock, EXIT_DATA *exit) {
  send_to_socket(sock,
		 "{g1) Door name\r\n"
		 "{c%s\r\n"
		 "{g2) Door keywords\r\n"
		 "{c%s\r\n"
		 "{g3) Leave message\r\n"
		 "{c%s\r\n"
		 "{g4) Enter message\r\n"
		 "{c%s\r\n"
		 "{g5) Description\r\n"
		 "{c%s\r\n"
		 "{g6) Exits to    : {y[{c%6d{y]\r\n"
		 "{g7) Closable    : {y[{c%6s{y]\r\n"
		 "{g8) Key vnum    : {y[{c%6d{y]\r\n"
		 "{g9) Pick diff   : {y[{c%6d{y]\r\n"
		 "{g0) Spot diff   : {y[{c%6d{y]\r\n"
		 "{gO) Opposite dir: {c%s{n\r\n",
		 (*exitGetName(exit) ? exitGetName(exit) : "<NONE>"),
		 (*exitGetKeywords(exit) ? exitGetKeywords(exit) : "<NONE>"),
		 (*exitGetSpecLeave(exit) ? exitGetSpecLeave(exit):"<DEFAULT>"),
		 (*exitGetSpecEnter(exit) ? exitGetSpecEnter(exit):"<DEFAULT>"),
		 exitGetDesc(exit),
		 exitGetTo(exit),
		 (exitIsClosable(exit) ? "Yes" : "No" ),
		 exitGetKey(exit),
		 exitGetPickLev(exit),
		 exitGetHidden(exit),
		 (*exitGetOpposite(exit) ? exitGetOpposite(exit) : "<DEFAULT>")
		 );
}

int exedit_chooser(SOCKET_DATA *sock, EXIT_DATA *exit, const char *option) {
  switch(toupper(*option)) {
  case '1':
    text_to_buffer(sock, "Enter a new name: ");
    return EXEDIT_NAME;
  case '2':
    text_to_buffer(sock, "Enter a new list of keywords: ");
    return EXEDIT_KEYWORDS;
  case '3':
    text_to_buffer(sock, "Enter a new leave message: ");
    return EXEDIT_LEAVE;
  case '4':
    text_to_buffer(sock, "Enter a new enterance message: ");
    return EXEDIT_ENTER;
  case '5':
    socketStartEditor(sock, text_editor, exitGetDescBuffer(exit));
    return MENU_NOCHOICE;
  case '6':
    text_to_buffer(sock, "Enter a new destination: ");
    return EXEDIT_TO;
  case '7':
    exitSetClosable(exit, (exitIsClosable(exit) ? FALSE : TRUE));
    return MENU_NOCHOICE;
  case '8':
    text_to_buffer(sock, "Enter a new key vnum: ");
    return EXEDIT_KEY;
  case '9':
    text_to_buffer(sock, "Enter a new lock difficulty: ");
    return EXEDIT_PICK;
  case '0':
    text_to_buffer(sock, "Enter a new spot difficulty: ");
    return EXEDIT_SPOT;
  case 'O':
    text_to_buffer(sock, "What is this exit's opposite direction: ");
    return EXEDIT_OPPOSITE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool exedit_parser(SOCKET_DATA *sock, EXIT_DATA *exit, int choice, 
		   const char *arg) {
  switch(choice) {
  case EXEDIT_NAME:
    exitSetName(exit, arg);
    return TRUE;
  case EXEDIT_KEYWORDS:
    exitSetKeywords(exit, arg);
    return TRUE;
  case EXEDIT_OPPOSITE:
    exitSetOpposite(exit, arg);
    return TRUE;
  case EXEDIT_LEAVE:
    exitSetSpecLeave(exit, arg);
    return TRUE;
  case EXEDIT_ENTER:
    exitSetSpecEnter(exit, arg);
    return TRUE;
  case EXEDIT_TO:
    exitSetTo(exit, MAX(NOWHERE, atoi(arg)));
    return TRUE;
  case EXEDIT_KEY:
    exitSetKey(exit, MAX(NOTHING, atoi(arg)));
    return TRUE;
  case EXEDIT_PICK:
    exitSetPickLev(exit, MAX(0, atoi(arg)));
    return TRUE;
  case EXEDIT_SPOT:
    exitSetHidden(exit, MAX(0, atoi(arg)));
    return TRUE;
  default:
    return FALSE;
  }
}



//*****************************************************************************
// room editing functions
//*****************************************************************************

// the different fields of a room we can edit
#define REDIT_NAME       1
#define REDIT_TERRAIN    2
#define REDIT_EXIT       3
#define REDIT_FILL_EXIT  4


//
// Display the exits the socket can edit
//
void redit_exit_menu(SOCKET_DATA *sock, ROOM_DATA *room) {
  int i;

  // normal exits first
  for(i = 0; i < NUM_DIRS; i++) {
    EXIT_DATA *exit = roomGetExit(room, i);
    send_to_socket(sock, "   {g%-10s : {y[%s%6d{y]%s",
		   dirGetName(i), 
		   (exit ? "{c" : "{y" ),
		   (exit ? exitGetTo(exit) : -1),
		   (!(i % 2) ? "   " : "\r\n"));
  }

  // now special exits
  int num_spec_exits = 0;
  const char **room_names = roomGetExitNames(room, &num_spec_exits);
  for(i = 0; i < num_spec_exits; i++) {
    EXIT_DATA *exit = roomGetExitSpecial(room, room_names[i]);
    send_to_socket(sock, "   {g%-10s : {y[{c%6d{y]%s",
		   room_names[i],
		   exitGetTo(exit),
		   (!(i % 2) ? "   " : "\r\n"));
  }

  // make sure we've printed the last newline if needed
  if(i % 2 == 1)
    send_to_socket(sock, "\r\n");

  // clean up our mess
  free(room_names);
}


void redit_menu(SOCKET_DATA *sock, ROOM_DATA *room) {
  send_to_socket(sock,
		 "{y[{c%d{y]\r\n"
		 "{g1) Name\r\n{c%s\r\n"
		 "{g2) Description\r\n{c%s\r\n"
#ifdef MODULE_TIME
		 "{g3) Night description (optional)\r\n{c%s\r\n"
#endif
		 "{gT) Terrain type {y[{c%s{y]\r\n"
		 "{gX) Extra descriptions menu\r\n"
		 "{gR) Room reset menu\r\n"
		 "{gS) Script menu\r\n"
		 "{gE) Edit exit\r\n"
		 "{gF) Fill exit\r\n"
		 ,
		 roomGetVnum(room), roomGetName(room), roomGetDesc(room),
#ifdef MODULE_TIME
		 roomGetNightDesc(room),
#endif
		 terrainGetName(roomGetTerrain(room)));
  redit_exit_menu(sock, room);
}


int redit_chooser(SOCKET_DATA *sock, ROOM_DATA *room, const char *option) {
  switch(toupper(*option)) {
  case '1':
    text_to_buffer(sock, "Enter a new room name: ");
    return REDIT_NAME;
  case '2':
    text_to_buffer(sock, "Enter a new room description:\r\n");
    socketStartEditor(sock, text_editor, roomGetDescBuffer(room));
    return MENU_NOCHOICE;
#ifdef MODULE_TIME
  case '3':
    text_to_buffer(sock, "Enter a new night description:\r\n");
    socketStartEditor(sock, text_editor, roomGetNightDescBuffer(room));
    return MENU_NOCHOICE;
#endif
  case 'T':
    olc_display_table(sock, terrainGetName, NUM_TERRAINS, 3);
    text_to_buffer(sock, "Pick a terrain type: ");
    return REDIT_TERRAIN;
  case 'F':
    text_to_buffer(sock, "What is the name of the exit you wish to fill: ");
    return REDIT_FILL_EXIT;
  case 'E':
    text_to_buffer(sock, "What is the name of the exit you wish to edit: ");
    return REDIT_EXIT;
  case 'X':
    do_olc(sock, edesc_set_menu, edesc_set_chooser, edesc_set_parser, NULL,NULL,
	   NULL, NULL, roomGetEdescs(room));
    return MENU_NOCHOICE;
  case 'R':
    do_olc(sock, rrledit_menu, rrledit_chooser, rrledit_parser, 
	   NULL, NULL, NULL, NULL, roomGetResets(room));
    return MENU_NOCHOICE;
  case 'S':
    do_olc(sock, ssedit_menu, ssedit_chooser, ssedit_parser,
	   NULL, NULL, NULL, NULL, roomGetScripts(room));
    return MENU_NOCHOICE;
    
  default:
    return MENU_CHOICE_INVALID;
  }
}


bool redit_parser(SOCKET_DATA *sock, ROOM_DATA *room, int choice, 
		  const char *arg) {
  switch(choice) {
  case REDIT_NAME:
    roomSetName(room, arg);
    return TRUE;
  case REDIT_TERRAIN: {
    int val = atoi(arg);
    if(val < 0 || val >= NUM_TERRAINS)
      return FALSE;
    roomSetTerrain(room, val);
    return TRUE;
  }
  case REDIT_FILL_EXIT: {
    int dir = dirGetNum(arg);
    if(dir != DIR_NONE)
      roomSetExit(room, dir, NULL);
    else
      roomSetExitSpecial(room, arg, NULL);
    return TRUE;
  }

  case REDIT_EXIT: {
    EXIT_DATA *exit = NULL;
    int dir = dirGetNum(arg);
    // did we supply an arg?
    if(!*arg)
      return TRUE;
    // find the exit. Create a new one if none exists
    if(dir != DIR_NONE) {
      if((exit = roomGetExit(room, dir)) == NULL)
	roomSetExit(room, dir, (exit = newExit()));
    }
    else if((exit = roomGetExitSpecial(room, arg)) == NULL)
      roomSetExitSpecial(room, arg, (exit = newExit()));

    // enter the exit editor
    do_olc(sock, exedit_menu, exedit_chooser, exedit_parser, NULL, NULL,
	   NULL, NULL, exit);
    return TRUE;
  }
    
  default:
    return FALSE;
  }
}


COMMAND(cmd_redit) {
  ZONE_DATA *zone;
  ROOM_DATA *room;
  int vnum;

  // if no argument is supplied, default to the current room
  if(!arg || !*arg)
    vnum = roomGetVnum(charGetRoom(ch));
  else
    vnum = atoi(arg);

  // make sure there is a corresponding zone ...
  if((zone = worldZoneBounding(gameworld, vnum)) == NULL)
    send_to_char(ch, "No zone exists that contains the given vnum.\r\n");
  else if(!canEditZone(zone, ch))
    send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
  else {
    // find the room
    room = zoneGetRoom(zone, vnum);

    // make our room
    if(room == NULL) {
      room = newRoom();
      roomSetVnum(room, vnum);
      roomSetName(room, "An Unfinished Room");
      roomSetDesc(room, "   You are in an unfinished room.\r\n");
      zoneAddRoom(zone, room);
    }

    do_olc(charGetSocket(ch), redit_menu, redit_chooser, redit_parser,
	   roomCopy, roomCopyTo, deleteRoom, save_world, room);
  }
}
