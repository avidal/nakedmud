//*****************************************************************************
//
// redit.c
//
// When room prototypes became python scripts, The room OLC had to be rethought.
// Ideally, all builders would have a basic grasp on python and thus would be
// able to write scripts. Ideally. Sadly, I don't think this can be expected
// out of most builders, and we still need some sort of non-scripting interface
// for editing objs. So here it is...
//
//*****************************************************************************
#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../world.h"
#include "../zone.h"
#include "../room.h"
#include "../character.h"
#include "../object.h"
#include "../exit.h"
#include "../room_reset.h"
#include "../extra_descs.h"
#include "../prototype.h"
#include "../handler.h"

#include "olc.h"
#include "olc_submenus.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../editor/editor.h"
#include "../scripts/scripts.h"
#include "../scripts/script_editor.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_TIME
#include "../time/mudtime.h"
#endif



//*****************************************************************************
// room reset olc structs
//*****************************************************************************
typedef struct {
  RESET_DATA *reset;
  char      *locale;
} RESET_OLC;

RESET_OLC *newResetOLC(RESET_DATA *reset, const char *locale) {
  RESET_OLC *data = malloc(sizeof(RESET_OLC));
  data->reset  = reset;
  data->locale = strdupsafe(locale);
  return data;
}

void deleteResetOLC(RESET_OLC *data) {
  if(data->locale) free(data->locale);
  free(data);
}



//*****************************************************************************
// room reset list olc structs. Note these are different from RESET_LISTs. This
// is for lists of in/on/then resets that are attached onto other resets.
//*****************************************************************************
typedef struct {
  LIST *res_list;
  char   *locale;
} RESLIST_OLC;

RESLIST_OLC *newReslistOLC(LIST *list, const char *locale) {
  RESLIST_OLC *data = malloc(sizeof(RESLIST_OLC));
  data->res_list    = list;
  data->locale      = strdupsafe(locale);
  return data;
}

void deleteReslistOLC(RESLIST_OLC *data) {
  if(data->locale) free(data->locale);
  free(data);
}



//*****************************************************************************
// utility functions
//*****************************************************************************

//
// reload a room. used whenever a room is edited and saved
void reload_room(const char *key) {
  if(worldRoomLoaded(gameworld, key)) {
    PROTO_DATA   *proto = worldGetType(gameworld, "rproto", key);
    ROOM_DATA *old_room = worldGetRoom(gameworld, key);
    ROOM_DATA *new_room = protoRoomRun(proto);
    if(new_room != NULL) {
      do_mass_transfer(old_room, new_room, TRUE, TRUE, TRUE);
      extract_room(old_room);
      worldPutRoom(gameworld, key, new_room);
    }
  }
}



//*****************************************************************************
// functions for printing room reset data
//*****************************************************************************
const char *write_reset_arg(int type, const char *arg, const char *locale) {
  static char buf[SMALL_BUFFER];
  PROTO_DATA  *obj = 
    worldGetType(gameworld, "oproto", get_fullkey_relative(arg, locale));
  PROTO_DATA  *mob = 
    worldGetType(gameworld, "mproto", get_fullkey_relative(arg, locale));
  int pos          = atoi(arg);
  switch(type) { 
  case RESET_LOAD_OBJECT:
    sprintf(buf, "load %s", (obj ? protoGetKey(obj) : "{RNOTHING{c"));
    break;
  case RESET_LOAD_MOBILE:
    sprintf(buf, "load %s", (mob ? protoGetKey(mob) : "{RNOBODY{c"));
    break;
  case RESET_POSITION:
    sprintf(buf, "change position to %s",
	    (pos < 0 || pos >= NUM_POSITIONS ? "{RNOTHING{c":posGetName(pos)));
    break;
  case RESET_FIND_MOBILE:
    sprintf(buf, "find %s", (mob ? protoGetKey(mob) : "{RNOBODY{c"));
    break;
  case RESET_FIND_OBJECT:
    sprintf(buf, "find %s", (obj ? protoGetKey(obj) : "{RNOTHING{c"));
    break;
  case RESET_PURGE_MOBILE:
    sprintf(buf, "purge %s", (mob ? protoGetKey(mob) : "{RNOBODY{c"));
    break;
  case RESET_PURGE_OBJECT:
    sprintf(buf, "purge %s", (obj ? protoGetKey(obj) : "{RNOTHING{c"));
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
		    bool indent_first, const char *locale) {
  int  i = 0;

  if(indent_first)
    i += indent_line(buf, buflen, indent);
  i += snprintf(buf+i, buflen-i,
		"{c%s with {w%d%% {cchance {w%d {ctime%s (max {w%d{c, rm. {w%d{c)\r\n",
		write_reset_arg(resetGetType(reset),resetGetArg(reset), locale),
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
      i += write_reset_buf(next, buf+i, buflen-i, indent+2, TRUE, locale);
    deleteListIterator(list_i);
  }

  // if we've got INs, then print 'em all out as well
  if(listSize(resetGetIn(reset)) > 0) {
    i += indent_line(buf+i, buflen-i, indent);
    i += snprintf(buf+i, buflen-i, "{yin it: \r\n");
    LIST_ITERATOR *list_i = newListIterator(resetGetIn(reset));
    RESET_DATA     *next  = NULL;
    ITERATE_LIST(next, list_i)
      i += write_reset_buf(next, buf+i, buflen-i, indent+2, TRUE, locale);
    deleteListIterator(list_i);
  }

  // if we've got THENs, print 'em all out as well
  if(listSize(resetGetThen(reset)) > 0) {
    i += indent_line(buf+i, buflen-i, indent);
    i += snprintf(buf+i, buflen-i, "{ywhen successful, also: \r\n");
    LIST_ITERATOR *list_i = newListIterator(resetGetThen(reset));
    RESET_DATA     *next  = NULL;
    ITERATE_LIST(next, list_i)
      i += write_reset_buf(next, buf+i, buflen-i, indent+2, TRUE, locale);
    deleteListIterator(list_i);
  }
  return i;
}

const char *write_reset(RESET_DATA *reset, int indent, bool indent_first,
			const char *locale) {
  static char buf[MAX_BUFFER];
  write_reset_buf(reset, buf, MAX_BUFFER, indent, indent_first, locale);
  return buf;
}



//*****************************************************************************
// room reset editing
//*****************************************************************************

// the resedit olc needs these declared
void    reslistedit_menu(SOCKET_DATA *sock, RESLIST_OLC *data);
int  reslistedit_chooser(SOCKET_DATA *sock, RESLIST_OLC *data, 
			 const char *option);
bool  reslistedit_parser(SOCKET_DATA *sock, RESLIST_OLC *data, int choice, 
			 const char *arg);

#define RESEDIT_TYPE       1
#define RESEDIT_TIMES      2
#define RESEDIT_CHANCE     3
#define RESEDIT_MAX        4
#define RESEDIT_ROOM_MAX   5
#define RESEDIT_ARGUMENT   6

void resedit_menu(SOCKET_DATA *sock, RESET_OLC *data) {
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
		 resetTypeGetName(resetGetType(data->reset)),
		 resetGetTimes(data->reset),
		 resetGetChance(data->reset),
		 resetGetMax(data->reset),
		 resetGetRoomMax(data->reset),
		 resetGetArg(data->reset),
		 write_reset(data->reset, 0, FALSE, data->locale)
		 );
}

int resedit_chooser(SOCKET_DATA *sock, RESET_OLC *data, const char *option) {
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
    text_to_buffer(sock, "What is the reset argument (i.e. obj name, direction, etc...): ");
    return RESEDIT_ARGUMENT;
  case '7':
    do_olc(sock, reslistedit_menu, reslistedit_chooser, reslistedit_parser, 
	   NULL, NULL, deleteReslistOLC, NULL, 
	   newReslistOLC(resetGetOn(data->reset), data->locale));
    return MENU_NOCHOICE;
  case '8':
    do_olc(sock, reslistedit_menu, reslistedit_chooser, reslistedit_parser, 
	   NULL, NULL, deleteReslistOLC, NULL, 
	   newReslistOLC(resetGetIn(data->reset), data->locale));
    return MENU_NOCHOICE;
  case '9':
    do_olc(sock, reslistedit_menu, reslistedit_chooser, reslistedit_parser, 
	   NULL, NULL, deleteReslistOLC, NULL, 
	   newReslistOLC(resetGetThen(data->reset), data->locale));
    return MENU_NOCHOICE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool resedit_parser(SOCKET_DATA *sock, RESET_OLC *data, int choice, 
		    const char *arg){
  switch(choice) {
  case RESEDIT_TYPE: {
    int type = atoi(arg);
    if(type < 0 || type >= NUM_RESETS)
      return FALSE;
    resetSetType(data->reset, type);
    // set all of the data to defaults
    resetSetArg(data->reset, "");
    resetSetChance(data->reset, 100);
    resetSetMax(data->reset, 0);
    resetSetRoomMax(data->reset, 0);
    resetSetTimes(data->reset, 1);
    return TRUE;
  }

  case RESEDIT_TIMES: {
    int times = atoi(arg);
    if(times < 1)
      return FALSE;
    resetSetTimes(data->reset, times);
    return TRUE;
  }

  case RESEDIT_CHANCE: {
    int chance = atoi(arg);
    if(chance < 1 || chance > 100)
      return FALSE;
    resetSetChance(data->reset, chance);
    return TRUE;
  }

  case RESEDIT_MAX: {
    int max = atoi(arg);
    if(max < 0)
      return FALSE;
    resetSetMax(data->reset, max);
    return TRUE;
  }

  case RESEDIT_ROOM_MAX: {
    int rmax = atoi(arg);
    if(rmax < 0)
      return FALSE;
    resetSetRoomMax(data->reset, rmax);
    return TRUE;
  }

  case RESEDIT_ARGUMENT:
    resetSetArg(data->reset, arg);
    return TRUE;

  default: return FALSE;
  }
}



//*****************************************************************************
// room reset list editing
//*****************************************************************************
#define RESLISTEDIT_EDIT       1
#define RESLISTEDIT_DELETE     2


void reslistedit_menu(SOCKET_DATA *sock, RESLIST_OLC *data) {
  LIST_ITERATOR *res_i = newListIterator(data->res_list);
  RESET_DATA    *reset = NULL;
  int            count = 0;

  send_to_socket(sock, "{wCurrent resets:\r\n");
  ITERATE_LIST(reset, res_i) {
    send_to_socket(sock, " {g%2d) %s", count, 
		   write_reset(reset, 5, FALSE, data->locale));
    count++;
  } deleteListIterator(res_i);

  send_to_socket(sock, "\r\n"
		 "  {gE) edit entry\r\n"
		 "  {gN) new entry\r\n"
		 "  {gD) delete entry\r\n"
		 );
}

void rrledit_menu(SOCKET_DATA *sock, RESET_LIST *list) {
  send_to_socket(sock, "{y[{c%s{y]\r\n", resetListGetKey(list));
  RESLIST_OLC *olc = newReslistOLC(resetListGetResets(list),
				   get_key_locale(resetListGetKey(list)));
  reslistedit_menu(sock, olc);
  deleteReslistOLC(olc);
}

int reslistedit_chooser(SOCKET_DATA *sock, RESLIST_OLC *list, 
			const char *option) {
  switch(toupper(*option)) {
  case 'N': {
    RESET_DATA *data = newReset();
    listQueue(list->res_list, data);
    do_olc(sock, resedit_menu, resedit_chooser, resedit_parser, 
	   NULL, NULL, deleteResetOLC, NULL, 
	   newResetOLC(data, list->locale));
    return MENU_NOCHOICE;
  }
  case 'E':
    if(listSize(list->res_list) == 0)
      return MENU_CHOICE_INVALID;
    text_to_buffer(sock, "Which entry do you want to edit (-1 for none): ");
    return RESLISTEDIT_EDIT;
  case 'D':
    text_to_buffer(sock, "Which entry do you want to delete: ");
    return RESLISTEDIT_DELETE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

int rrledit_chooser(SOCKET_DATA *sock, RESET_LIST *list, const char *option) {
  RESLIST_OLC *olc = newReslistOLC(resetListGetResets(list),
				   get_key_locale(resetListGetKey(list)));
  int retval = reslistedit_chooser(sock, olc, option);
  deleteReslistOLC(olc);
  return retval;
}

bool reslistedit_parser(SOCKET_DATA *sock, RESLIST_OLC *list, int choice, 
			const char *arg) {
  switch(choice) {
  case RESLISTEDIT_EDIT: {
    RESET_DATA *reset = NULL;
    if(atoi(arg) == NOTHING)
      return TRUE;
    if(!isdigit(*arg) || 
       (reset = listGet(list->res_list, atoi(arg))) == NULL)
      return FALSE;
    do_olc(sock, resedit_menu, resedit_chooser, resedit_parser, 
	   NULL, NULL, deleteResetOLC, NULL, 
	   newResetOLC(reset, list->locale));
    return TRUE;
  }
  case RESLISTEDIT_DELETE: {
    RESET_DATA *reset = NULL;
    if(!isdigit(*arg) || 
       (reset = listGet(list->res_list, atoi(arg))) == NULL)
      return FALSE;
    listRemove(list->res_list, reset);
    deleteReset(reset);
    return TRUE;
  }
  default: return FALSE;
  }
}

bool rrledit_parser(SOCKET_DATA *sock, RESET_LIST *list, int choice, 
		    const char *arg) {
  RESLIST_OLC *olc = newReslistOLC(resetListGetResets(list),
				   get_key_locale(resetListGetKey(list)));
  int retval = reslistedit_parser(sock, olc, choice, arg);
  deleteReslistOLC(olc);
  return retval;
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
		 "{g6) Exits to    : {c%s\r\n"
		 "{g8) Key         : {c%s\r\n"
		 "{g7) Closable    : {y[{c%6s{y]%s\r\n"
		 "{g9) Pick diff   : {y[{c%6d{y]\r\n"
		 "{g0) Spot diff   : {y[{c%6d{y]\r\n"
		 "{gO) Opposite dir: {c%s{n\r\n",
		 (*exitGetName(exit) ? exitGetName(exit) : "<NONE>"),
		 (*exitGetKeywords(exit) ? exitGetKeywords(exit) : "<NONE>"),
		 (*exitGetSpecLeave(exit) ? exitGetSpecLeave(exit):"<DEFAULT>"),
		 (*exitGetSpecEnter(exit) ? exitGetSpecEnter(exit):"<DEFAULT>"),
		 exitGetDesc(exit),
		 exitGetTo(exit),
		 exitGetKey(exit),
		 (exitIsClosable(exit) ? "Yes" : "No" ),
		 (exitIsClosable(exit) && (!*exitGetName(exit) || !*exitGetKeywords(exit)) ? " {r* exit also needs name and keywords{n" : ""),
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
    text_to_buffer(sock, "Enter a new key name: ");
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
    exitSetTo(exit, arg);
    return TRUE;
  case EXEDIT_KEY:
    exitSetKey(exit, arg);
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
// room olc data
//*****************************************************************************
typedef struct {
  char          *key; // the key for our prototype
  char      *parents; // prototypes we inherit from
  bool      abstract; // can we be loaded into the game?
  bool    resettable; // do we reset when our zone resets?
  ROOM_DATA    *room; // our room, which holds most of our variables
  RESET_LIST *resets; // non-python reset rules
  BUFFER *extra_code; // any extra code that should go into our prototype
} ROOM_OLC;

ROOM_OLC *newRoomOLC(void) {
  ROOM_OLC *data   = malloc(sizeof(ROOM_OLC));
  data->key        = strdup("");
  data->parents    = strdup("");
  data->room       = newRoom();
  data->resets     = newResetList();
  data->extra_code = newBuffer(1);
  data->abstract   = TRUE;
  data->resettable = FALSE;
  roomSetTerrain(data->room, TERRAIN_NONE);
  return data;
}

void deleteRoomOLC(ROOM_OLC *data) {
  if(data->key)        free(data->key);
  if(data->parents)    free(data->parents);
  if(data->room)       deleteRoom(data->room);
  if(data->resets)     deleteResetList(data->resets);
  if(data->extra_code) deleteBuffer(data->extra_code);
  free(data);
}

ROOM_DATA *roomOLCGetRoom(ROOM_OLC *data) {
  return data->room;
}

RESET_LIST *roomOLCGetResets(ROOM_OLC *data) {
  return data->resets;
}

const char *roomOLCGetKey(ROOM_OLC *data) {
  return data->key;
}

const char *roomOLCGetParents(ROOM_OLC *data) {
  return data->parents;
}

BUFFER *roomOLCGetExtraCode(ROOM_OLC *data) {
  return data->extra_code;
}

bool roomOLCGetAbstract(ROOM_OLC *data) {
  return data->abstract;
}

bool roomOLCGetResettable(ROOM_OLC *data) {
  return data->resettable;
}

void roomOLCSetKey(ROOM_OLC *data, const char *key) {
  if(data->key) free(data->key);
  data->key =   strdupsafe(key);
}

void roomOLCSetParents(ROOM_OLC *data, const char *parents) {
  if(data->parents) free(data->parents);
  data->parents =   strdupsafe(parents);
}

void roomOLCSetAbstract(ROOM_OLC *data, bool abstract) {
  data->abstract = abstract;
}

void roomOLCSetResettable(ROOM_OLC *data, bool resettable) {
  data->resettable = resettable;
}


//
// sets an exits values, based on a proto script
void exit_from_proto(EXIT_DATA *exit, BUFFER *buf) {
  char line[SMALL_BUFFER];
  const char *code = bufferString(buf);

  do {
    char *lptr = line;
    code = strcpyto(line, code, '\n');

    if(!strcmp(lptr, "exit.makedoor()"))
      exitSetClosable(exit, TRUE);
    else if(!strncmp(lptr, "exit.name", 9)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[next_letter_in(lptr, '\"')] = '\0'; // kill the ending "
      exitSetName(exit, lptr);
    }
    else if(!strncmp(lptr, "exit.keywords", 13)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[next_letter_in(lptr, '\"')] = '\0'; // kill the ending "
      exitSetKeywords(exit, lptr);
    }
    else if(!strncmp(lptr, "exit.key", 8)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[next_letter_in(lptr, '\"')] = '\0'; // kill the ending "
      exitSetKey(exit, lptr);
    }
    else if(!strncmp(lptr, "exit.opposite", 13)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[next_letter_in(lptr, '\"')] = '\0'; // kill the ending "
      exitSetOpposite(exit, lptr);
    }
    else if(!strncmp(lptr, "exit.leave_mssg", 15)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[next_letter_in(lptr, '\"')] = '\0'; // kill the ending "
      exitSetSpecLeave(exit, lptr);
    }
    else if(!strncmp(lptr, "exit.enter_mssg", 15)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[next_letter_in(lptr, '\"')] = '\0'; // kill the ending "
      exitSetSpecEnter(exit, lptr);
    }
    else if(!strncmp(lptr, "exit.desc", 9)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[strlen(lptr) - 1] = '\0';           // kill the ending "
      exitSetDesc(exit, lptr);
      // replace our \"s with "
      bufferReplace(exitGetDescBuffer(exit), "\\\"", "\"", TRUE);
      bufferFormat(exitGetDescBuffer(exit), SCREEN_WIDTH, PARA_INDENT);
    }
    else if(!strncmp(lptr, "exit.pick_diff", 14)) {
      while(!isdigit(*lptr)) lptr++;
      exitSetPickLev(exit, atoi(lptr));
    }
    else if(!strncmp(lptr, "exit.spot_diff", 14)) {
      while(!isdigit(*lptr)) lptr++;
      exitSetHidden(exit, atoi(lptr));
    }
  } while(*code != '\0');
}


ROOM_OLC *roomOLCFromProto(PROTO_DATA *proto) {
  ROOM_OLC  *data = newRoomOLC();
  ROOM_DATA *room = roomOLCGetRoom(data);
  roomOLCSetKey(data, protoGetKey(proto));
  roomOLCSetParents(data, protoGetParents(proto));
  roomOLCSetAbstract(data, protoIsAbstract(proto));

  // this is a really ugly way to do the conversion, but basically let's
  // just look through every line in the buffer and if we recognize some
  // token, parse out whatever is assigned to it
  char line[MAX_BUFFER];
  const char *code = protoGetScript(proto);
  do {
    code = strcpyto(line, code, '\n');
    char *lptr = line;
    if(!strncmp(lptr, "me.name", 7)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      roomSetName(room, lptr);
    }
    else if(!strncmp(lptr, "me.desc",  7)) {
      // we have three "'s to skip by, because this lptr will take the form:
      // me.desc = me.desc + " " + "..."
      while(*lptr != '\"') lptr++; lptr++;
      while(*lptr != '\"') lptr++; lptr++;
      while(*lptr != '\"') lptr++; lptr++;
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      roomSetDesc(room, lptr);
      // replace our \"s with "
      bufferReplace(roomGetDescBuffer(room), "\\\"", "\"", TRUE);
      bufferFormat(roomGetDescBuffer(room), SCREEN_WIDTH, PARA_INDENT);
    }
#ifdef MODULE_TIME
    else if(!strncmp(lptr, "me.ndesc",  13)) {
      // we have three "'s to skip by, because this lptr will take the form:
      // me.desc = me.desc + " " + "..."
      while(*lptr != '\"') lptr++; lptr++;
      while(*lptr != '\"') lptr++; lptr++;
      while(*lptr != '\"') lptr++; lptr++;
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      roomSetDesc(room, lptr);
      // replace our \"s with "
      bufferReplace(roomGetNightDescBuffer(room), "\\\"", "\"", TRUE);
      bufferFormat(roomGetNightDescBuffer(room), SCREEN_WIDTH, PARA_INDENT);
    }
#endif
    else if(!strncmp(lptr, "me.terrain", 10)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      roomSetTerrain(room, terrainGetNum(lptr));
    }
    else if(!strncmp(lptr, "me.edesc(", 9)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                              // kill the leading "
      char *desc_start = lptr + next_letter_in(lptr, '\"') + 1;
      lptr[next_letter_in(lptr, '\"')] = '\0';             // kill the ending "
      while(*desc_start != '\"') desc_start++;
      desc_start++;                                        // kill start and end
      desc_start[strlen(desc_start)-2] = '\0';             // "s for desc too
      EDESC_DATA *edesc = newEdesc(lptr, desc_start);
      // replace our \"s with "
      bufferReplace(edescGetDescBuffer(edesc), "\\\"", "\"", TRUE);
      edescSetPut(roomGetEdescs(room), edesc);
    }
    else if(!strncmp(lptr, "me.attach(\"", 11)) {
      char trigname[SMALL_BUFFER];
      sscanf(lptr, "me.attach(\"%s", trigname);
      // kill our ending ")
      trigname[strlen(trigname)-2] = '\0';
      triggerListAdd(roomGetTriggers(room), trigname);
    }
    else if(!strncmp(lptr, "me.bits", 7)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[next_letter_in(lptr, '\"')] = '\0'; // kill the ending "
      bitSet(roomGetBits(room), lptr);
    }

    else if(!strncmp(lptr, "exit = me.dig(", 14)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                              // kill the leading "
      char *dest = lptr + next_letter_in(lptr, '\"') + 1;
      lptr[next_letter_in(lptr, '\"')] = '\0';             // kill the ending "
      while(*dest != '\"') dest++;
      dest++;                                              // kill start and end
      dest[next_letter_in(dest, '\"')] = '\0';             // "s for desc too
      EXIT_DATA *exit = newExit();
      exitSetTo(exit, dest);
      roomSetExit(room, lptr, exit);

      BUFFER *ex_buf = newBuffer(1);
      code = strcpyto(line, code, '\n');
      while(*line && strcmp(line, "### end exit") != 0) {
	bprintf(ex_buf, "%s\n", line);
	code = strcpyto(line, code, '\n');
      }

      exit_from_proto(exit, ex_buf);
      deleteBuffer(ex_buf);
    }

    else if(!strcmp(lptr, "### begin extra code")) {
      code = strcpyto(line, code, '\n');
      while(strcmp(line, "### end extra code") != 0) {
	bprintf(roomOLCGetExtraCode(data), "%s\n", line);
	if(!*code) break;
	code = strcpyto(line, code, '\n');
      }
    }
  } while(*code != '\0');

  return data;
}


//
// Makes a python script out of an exit
void exit_to_proto(EXIT_DATA *exit, BUFFER *buf) {
  if(exitIsClosable(exit))
    bprintf(buf, "exit.makedoor()\n");
  if(*exitGetName(exit))
    bprintf(buf, "exit.name       = \"%s\"\n", exitGetName(exit));
  if(*exitGetKeywords(exit))
    bprintf(buf, "exit.keywords   = \"%s\"\n", exitGetKeywords(exit));
  if(*exitGetKey(exit))
    bprintf(buf, "exit.key        = \"%s\"\n", exitGetKey(exit));
  if(*exitGetOpposite(exit))
    bprintf(buf, "exit.opposite   = \"%s\"\n", exitGetOpposite(exit));
  if(*exitGetDesc(exit)) {
    BUFFER *desc_copy = bufferCopy(exitGetDescBuffer(exit));
    bufferReplace(desc_copy, "\n", " ", TRUE);
    bufferReplace(desc_copy, "\r", "",  TRUE);
    bufferReplace(desc_copy, "\"", "\\\"", TRUE);
    bprintf(buf, "exit.desc       = \"%s\"\n", bufferString(desc_copy));
    deleteBuffer(desc_copy);
  }
  if(exitGetPickLev(exit) > 0)
    bprintf(buf, "exit.pick_diff  = %d\n", exitGetPickLev(exit));
  if(exitGetHidden(exit) > 0)
    bprintf(buf, "exit.spot_diff  = %d\n", exitGetHidden(exit));
  if(*exitGetSpecLeave(exit))
    bprintf(buf, "exit.leave_mssg = \"%s\"\n", exitGetSpecLeave(exit));
  if(*exitGetSpecEnter(exit))
    bprintf(buf, "exit.enter_mssg = \"%s\"\n", exitGetSpecEnter(exit));
}

PROTO_DATA *roomOLCToProto(ROOM_OLC *data) {
  PROTO_DATA *proto = newProto();
  ROOM_DATA   *room = roomOLCGetRoom(data);
  BUFFER       *buf = protoGetScriptBuffer(proto);
  protoSetKey(proto, roomOLCGetKey(data));
  protoSetParents(proto, roomOLCGetParents(data));
  protoSetAbstract(proto, roomOLCGetAbstract(data));

  bprintf(buf, "### The following rproto was generated by redit.\n");
  bprintf(buf, "### If you edit this script, adhere to the stylistic\n"
	       "### conventions laid out by redit, or delete the top line\n");

  bprintf(buf, "\n### string values\n");
  if(*roomGetName(room))
    bprintf(buf, "me.name       = \"%s\"\n", roomGetName(room));
  if(roomGetTerrain(room) != TERRAIN_NONE)
    bprintf(buf, "me.terrain    = \"%s\"\n", 
	    terrainGetName(roomGetTerrain(room)));
  if(*roomGetDesc(room)) {
    BUFFER *desc_copy = bufferCopy(roomGetDescBuffer(room));
    bufferReplace(desc_copy, "\n", " ", TRUE);
    bufferReplace(desc_copy, "\r", "",  TRUE);
    bufferReplace(desc_copy, "\"", "\\\"", TRUE);
    bprintf(buf, "me.desc       = me.desc + \" \" + \"%s\"\n", 
	    bufferString(desc_copy));
    deleteBuffer(desc_copy);
  }
#ifdef MODULE_TIME
  if(*roomGetNightDesc(room)) {
    BUFFER *desc_copy = bufferCopy(roomGetNightDescBuffer(room));
    bufferReplace(desc_copy, "\n", " ",    TRUE);
    bufferReplace(desc_copy, "\r", "",     TRUE);
    bufferReplace(desc_copy, "\"", "\\\"", TRUE);
    bprintf(buf, "me.ndesc = me.ndesc + \" \" + \"%s\"\n", 
	    bufferString(desc_copy));
    deleteBuffer(desc_copy);
  }
#endif

  // extra descriptions
  if(listSize(edescSetGetList(roomGetEdescs(room))) > 0) {
    bprintf(buf, "\n### extra descriptions\n");
    LIST_ITERATOR *edesc_i= 
      newListIterator(edescSetGetList(roomGetEdescs(room)));
    EDESC_DATA      *edesc= NULL;
    ITERATE_LIST(edesc, edesc_i) {
      BUFFER *desc_copy = bufferCopy(edescGetDescBuffer(edesc));
      bufferReplace(desc_copy, "\n", " ", TRUE);
      bufferReplace(desc_copy, "\r", "",  TRUE);
      bufferReplace(desc_copy, "\"", "\\\"", TRUE);
      bprintf(buf, "me.edesc(\"%s\", \"%s\")\n", 
	      edescGetKeywords(edesc), bufferString(desc_copy));
      deleteBuffer(desc_copy);
    } deleteListIterator(edesc_i);
  }

  if(*bitvectorGetBits(roomGetBits(room))) {
    bprintf(buf, "\n### room bits\n");
    bprintf(buf, "me.bits     = \"%s\" + \", \" + me.bits\n", 
	    bitvectorGetBits(roomGetBits(room)));
  }

  if(listSize(roomGetTriggers(room)) > 0) {
    bprintf(buf, "\n### room triggers\n");
    LIST_ITERATOR *trig_i = newListIterator(roomGetTriggers(room));
    char            *trig = NULL;
    ITERATE_LIST(trig, trig_i) {
      bprintf(buf, "me.attach(\"%s\")\n", trig);
    } deleteListIterator(trig_i);
  }

  // convert exits
  LIST      *ex_names = roomGetExitNames(room);
  LIST_ITERATOR *ex_i = newListIterator(ex_names);
  char       *ex_name = NULL;
  EXIT_DATA     *exit = NULL;
  ITERATE_LIST(ex_name, ex_i) {
    exit = roomGetExit(room, ex_name);
    bprintf(buf, "\n### begin exit: %s\n", ex_name);
    bprintf(buf, "exit = me.dig(\"%s\", \"%s\")\n", ex_name, exitGetTo(exit));
    exit_to_proto(exit, buf);
    bprintf(buf, "### end exit\n");
  } deleteListIterator(ex_i);
  deleteListWith(ex_names, free);

  // extra code
  if(bufferLength(roomOLCGetExtraCode(data)) > 0) {
    bprintf(buf, "\n### begin extra code\n");
    bprintf(buf, "%s", bufferString(roomOLCGetExtraCode(data)));
    bprintf(buf, "### end extra code\n");
  }

  return proto;
}



//*****************************************************************************
// room editing functions
//*****************************************************************************

// the different fields of a room we can edit
#define REDIT_PARENTS    2
#define REDIT_NAME       3
#define REDIT_TERRAIN    4
#define REDIT_EXIT       5
#define REDIT_FILL_EXIT  6
#define REDIT_BITVECTOR  7


//
// Display the exits the socket can edit
//
void redit_exit_menu(SOCKET_DATA *sock, ROOM_OLC *data) {
  LIST       *ex_list = roomGetExitNames(roomOLCGetRoom(data));
  LIST_ITERATOR *ex_i = newListIterator(ex_list);
  char           *dir = NULL;
  EXIT_DATA       *ex = NULL;
  int               i = 0;

  // first, show all of the standard directions and display an entry for
  // them, whether we have an exit or not
  for(i = 0; i < NUM_DIRS; i++) {
    ex = roomGetExit(roomOLCGetRoom(data), dirGetName(i));
    send_to_socket(sock, "   {g%-10s : %s%-20s%s",
		   dirGetName(i), 
		   (ex ? "{c" : "{y" ),
		   (ex ? exitGetTo(ex) : "nowhere"),
		   (!(i % 2) ? "   "   : "\r\n"));    
  }

  // now go through our exit list and do all of our special exits
  i = 0;
  ITERATE_LIST(dir, ex_i) {
    if(dirGetNum(dir) == DIR_NONE) {
      ex = roomGetExit(roomOLCGetRoom(data), dir);
      send_to_socket(sock, "   {g%-10s : {c%s%s",
		     dir, exitGetTo(ex),
		     (!(i % 2) ? "   " : "\r\n"));
      i++;
    }
  } deleteListIterator(ex_i);
  deleteListWith(ex_list, free);

  // make sure we've printed the last newline if needed
  if(i % 2 == 1)
    send_to_socket(sock, "\r\n");
}


void redit_menu(SOCKET_DATA *sock, ROOM_OLC *data) {
  send_to_socket(sock,
		 "{y[{c%s{y]\r\n"
		 "{g1) Abstract: {c%s\r\n"
		 "{g2) Inherits from prototypes:\r\n"
		 "{c%s\r\n"
		 "{g3) Name\r\n{c%s\r\n"
		 "{g4) Description\r\n{c%s"
#ifdef MODULE_TIME
		 "{g5) Night description (optional)\r\n{c%s"
#endif
		 "{gL) Land type {y[{c%s{y]\r\n"
		 "{gB) Set Bits: {c%s\r\n"
		 "{gZ) Room can be reset: {c%s\r\n"
		 "{gR) Room reset menu\r\n"
		 "{gX) Extra descriptions menu\r\n"
		 "{gT) Trigger menu\r\n"
		 "{gE) Edit exit\r\n"
		 "{gF) Fill exit\r\n"
		 ,
		 roomOLCGetKey(data), 
		 (roomOLCGetAbstract(data) ? "yes" : "no"),
		 roomOLCGetParents(data),
		 roomGetName(roomOLCGetRoom(data)), 
		 roomGetDesc(roomOLCGetRoom(data)),
#ifdef MODULE_TIME
		 roomGetNightDesc(roomOLCGetRoom(data)),
#endif
		 (roomGetTerrain(roomOLCGetRoom(data)) == TERRAIN_NONE ? 
		  "leave unchanged" :
		  terrainGetName(roomGetTerrain(roomOLCGetRoom(data)))),
		 bitvectorGetBits(roomGetBits(roomOLCGetRoom(data))),
		 (roomOLCGetResettable(data) ? "yes" : "no"));
  redit_exit_menu(sock, data);

  // only allow code editing for people with scripting priviledges
  send_to_socket(sock, "\n{gC) Extra code%s\r\n", 
		 ((!socketGetChar(sock) ||  
		   !bitIsOneSet(charGetUserGroups(socketGetChar(sock)),
				"scripter")) ? "    {y({cuneditable{y){g":""));
  script_display(sock, bufferString(roomOLCGetExtraCode(data)), FALSE);
}


int redit_chooser(SOCKET_DATA *sock, ROOM_OLC *data, const char *option) {
  switch(toupper(*option)) {
  case '1':
    roomOLCSetAbstract(data, (roomOLCGetAbstract(data) + 1) % 2);
    return MENU_NOCHOICE;
  case '2':
    text_to_buffer(sock,"Enter comma-separated list of rooms to inherit from: ");
    return REDIT_PARENTS;
  case '3':
    text_to_buffer(sock, "Enter a new room name: ");
    return REDIT_NAME;
  case '4':
    text_to_buffer(sock, "Enter a new room description:\r\n");
    socketStartEditor(sock, text_editor, roomGetDescBuffer(roomOLCGetRoom(data)));
    return MENU_NOCHOICE;
#ifdef MODULE_TIME
  case '5':
    text_to_buffer(sock, "Enter a new night description:\r\n");
    socketStartEditor(sock, text_editor, roomGetNightDescBuffer(roomOLCGetRoom(data)));
    return MENU_NOCHOICE;
#endif
  case 'Z':
    roomOLCSetResettable(data, (roomOLCGetResettable(data) + 1) % 2);
    return MENU_NOCHOICE;
  case 'R':
    do_olc(sock, rrledit_menu, rrledit_chooser, rrledit_parser, NULL, NULL,
	   NULL, NULL, roomOLCGetResets(data));
    return MENU_NOCHOICE;
  case 'L':
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
	   NULL, NULL, roomGetEdescs(roomOLCGetRoom(data)));
    return MENU_NOCHOICE;
  case 'T':
    do_olc(sock, trigger_list_menu, trigger_list_chooser, trigger_list_parser,
	   NULL, NULL, NULL, NULL, roomGetTriggers(roomOLCGetRoom(data)));
    return MENU_NOCHOICE;
  case 'B':
    do_olc(sock, bedit_menu, bedit_chooser, bedit_parser,
	   NULL, NULL, NULL, NULL, roomGetBits(roomOLCGetRoom(data)));
    return MENU_NOCHOICE;
  case 'C':
    // only scripters can edit extra code
    if(!socketGetChar(sock) || 
       !bitIsOneSet(charGetUserGroups(socketGetChar(sock)), "scripter"))
      return MENU_CHOICE_INVALID;
    text_to_buffer(sock, "Edit extra code\r\n");
    socketStartEditor(sock, script_editor, roomOLCGetExtraCode(data));
    return MENU_NOCHOICE;
  default:
    return MENU_CHOICE_INVALID;
  }
}


bool redit_parser(SOCKET_DATA *sock, ROOM_OLC *data, int choice, 
		  const char *arg) {
  switch(choice) {
  case REDIT_PARENTS:
    roomOLCSetParents(data, arg);
    return TRUE;    
  case REDIT_NAME:
    roomSetName(roomOLCGetRoom(data), arg);
    return TRUE;
  case REDIT_TERRAIN: {
    int val = atoi(arg);
    if(val < 0 || val >= NUM_TERRAINS)
      return FALSE;
    roomSetTerrain(roomOLCGetRoom(data), val);
    return TRUE;
  }
  case REDIT_FILL_EXIT: {
    EXIT_DATA *old_exit = NULL;
    const char     *dir = arg;
    // are we trying to reference by its abbreviation?
    if(!roomGetExit(roomOLCGetRoom(data), dir) && dirGetAbbrevNum(arg) != DIR_NONE)
      dir = dirGetName(dirGetAbbrevNum(dir));
    old_exit = roomRemoveExit(roomOLCGetRoom(data), dir);

    // delete the exit...
    if(old_exit != NULL) {
      // is our room in the game? do we need to remove the exit from game?
      if(propertyTableIn(room_table, roomGetUID(roomOLCGetRoom(data))))
	exit_from_game(old_exit);
      deleteExit(old_exit);
    }

    return TRUE;
  }

  case REDIT_EXIT: {
    if(!*arg)
      return FALSE;
    else {
      EXIT_DATA *exit = NULL;
      const char *dir = arg;
      
      // are we trying to reference by its abbreviation?
      if(!roomGetExit(roomOLCGetRoom(data), dir) && dirGetAbbrevNum(arg) != DIR_NONE)
	dir = dirGetName(dirGetAbbrevNum(dir));
      exit = roomGetExit(roomOLCGetRoom(data), dir);

      // do we need to supply a new exit?
      if(exit == NULL) {
	exit = newExit();
	// are we editing a room in game? If so, add this exit to the game
	if(propertyTableIn(room_table, roomGetUID(roomOLCGetRoom(data))))
	  exit_to_game(exit);
	roomSetExit(roomOLCGetRoom(data), dir, exit);
      }

      // enter the exit editor
      do_olc(sock, exedit_menu, exedit_chooser, exedit_parser, NULL, NULL,
	     NULL, NULL, exit);
      return TRUE;
    }
  }
    
  default:
    return FALSE;
  }
}

void save_rreset(RESET_LIST *list) {
  if(listSize(resetListGetResets(list)) > 0)
    worldSaveType(gameworld, "reset", resetListGetKey(list));
  else {
    worldRemoveType(gameworld, "reset", resetListGetKey(list));
    worldSaveType(gameworld, "reset", resetListGetKey(list));
    deleteResetList(list);
  }
}

void save_room_olc(ROOM_OLC *data) {
  PROTO_DATA *old_proto = worldGetType(gameworld, "rproto",roomOLCGetKey(data));
  PROTO_DATA *new_proto = roomOLCToProto(data);
  RESET_LIST *old_reset = worldGetType(gameworld, "reset", roomOLCGetKey(data));
  RESET_LIST *new_reset = roomOLCGetResets(data);
  ZONE_DATA       *zone = 
    worldGetZone(gameworld, get_key_locale(roomOLCGetKey(data)));

  // save our room proto
  if(old_proto == NULL)
    worldPutType(gameworld, "rproto", protoGetKey(new_proto), new_proto);
  else {
    protoCopyTo(new_proto, old_proto);
    deleteProto(new_proto);
  }

  // save our resets
  if(old_reset == NULL && listSize(resetListGetResets(new_reset)) > 0)
    worldPutType(gameworld, "reset", roomOLCGetKey(data), 
		 resetListCopy(new_reset));
  else if(old_reset != NULL)
    resetListCopyTo(new_reset, old_reset);
  else if(old_reset != NULL && listSize(resetListGetResets(new_reset)) == 0) {
    worldRemoveType(gameworld, "reset", roomOLCGetKey(data));
    deleteResetList(old_reset);
  }

  // check to see if we should reset this room even if it's not visited yet
  if(roomOLCGetResettable(data) && 
     !listGetWith(zoneGetResettable(zone),
		  get_key_name(roomOLCGetKey(data)),
		  strcasecmp))
    listPut(zoneGetResettable(zone), strdup(get_key_name(roomOLCGetKey(data))));
  else if(!roomOLCGetResettable(data)) {
    char *found = listRemoveWith(zoneGetResettable(zone),
				 get_key_name(roomOLCGetKey(data)),
				 strcasecmp);
    if(found) zoneSave(zone);
  }

  worldSaveType(gameworld, "rproto", roomOLCGetKey(data));
  worldSaveType(gameworld, "reset",  roomOLCGetKey(data));
  zoneSave(zone);

  // force-reset our room
  reload_room(roomOLCGetKey(data));
}



//*****************************************************************************
// commands
//*****************************************************************************
COMMAND(cmd_resedit) {
  ZONE_DATA    *zone = NULL;
  RESET_LIST *resets = NULL;
  const char   *rkey = arg;

  // we need a key
  if(!rkey || !*rkey)
    rkey = roomGetClass(charGetRoom(ch));

  char locale[SMALL_BUFFER];
  char   name[SMALL_BUFFER];
  if(!parse_worldkey_relative(ch, rkey, name, locale))
    send_to_char(ch, "Which room are you trying to edit?\r\n");
  // make sure we can edit the zone
  else if((zone = worldGetZone(gameworld, locale)) == NULL)
    send_to_char(ch, "No such zone exists.\r\n");
  else if(!canEditZone(zone, ch))
    send_to_char(ch, "You are not authorized to edit that zone.\r\n");
  else {
    // try to pull up the reset
    resets = worldGetType(gameworld, "reset",  get_fullkey(name, locale));
    
    // if we don't already have a reset, make one
    if(resets == NULL) {
      resets = newResetList();
      worldPutType(gameworld, "reset", get_fullkey(name, locale), resets);
    }

    do_olc(charGetSocket(ch), rrledit_menu, rrledit_chooser, rrledit_parser, 
	   resetListCopy, resetListCopyTo, deleteResetList, save_rreset, 
	   resets);
  }
}


COMMAND(cmd_redit) {
  ZONE_DATA    *zone = NULL;
  PROTO_DATA  *proto = NULL;
  RESET_LIST *resets = NULL;
  const char   *rkey = arg;

  // we need a key
  if(!rkey || !*rkey)
    rkey = roomGetClass(charGetRoom(ch));

  char locale[SMALL_BUFFER];
  char   name[SMALL_BUFFER];
  if(!parse_worldkey_relative(ch, rkey, name, locale))
    send_to_char(ch, "Which room are you trying to edit?\r\n");
  // make sure we can edit the zone
  else if((zone = worldGetZone(gameworld, locale)) == NULL)
    send_to_char(ch, "No such zone exists.\r\n");
  else if(!canEditZone(zone, ch))
    send_to_char(ch, "You are not authorized to edit that zone.\r\n");
  else {
    // try to make our OLC datastructure
    ROOM_OLC *data = NULL;
    
    // try to pull up the prototype
    proto  = worldGetType(gameworld, "rproto", get_fullkey(name, locale));
    resets = worldGetType(gameworld, "reset",  get_fullkey(name, locale));

    // if we already have proto data, try to parse an obj olc out of it
    if(proto != NULL) {
      // check to make sure the prototype was made by redit
      char line[SMALL_BUFFER];
      strcpyto(line, protoGetScript(proto), '\n');
      if(strcmp(line, "### The following rproto was generated by redit.") != 0){
	send_to_char(ch, "This room was not generated by redit and potential "
		     "formatting problems prevent redit from being used. To "
		     "edit, rpedit must be used.\r\n");
	return;
      }
      else {
	data = roomOLCFromProto(proto);
	if(resets != NULL)
	  resetListCopyTo(resets, roomOLCGetResets(data));
	else
	  resetListSetKey(roomOLCGetResets(data), get_fullkey(name, locale));
      }
    }
    // otherwise, make a new obj olc and assign its key
    else {
      data = newRoomOLC();
      roomOLCSetKey(data, get_fullkey(name, locale));
      resetListSetKey(roomOLCGetResets(data), get_fullkey(name, locale));
      roomOLCSetAbstract(data, TRUE);
      
      ROOM_DATA *room = roomOLCGetRoom(data);
      roomSetName      (room, "an unfinished room");
      roomSetDesc      (room, "it looks unfinished.\r\n");
    }
    
    if(listGetWith(zoneGetResettable(zone), 
		   get_key_name(roomOLCGetKey(data)),
		   strcasecmp) != NULL)
      roomOLCSetResettable(data, TRUE);
    
    do_olc(charGetSocket(ch), redit_menu, redit_chooser, redit_parser, 
	   NULL, NULL, deleteRoomOLC, save_room_olc, data);
  }
}
