//*****************************************************************************
//
// script.c
//
// All of the information regarding scripts, from the functions to run them
// and all of the different script types.
//
//*****************************************************************************

// script stuff
#include <Python.h>
#include <structmember.h>

// mud stuff
#include <mud.h>
#include <utils.h>
#include <socket.h>
#include <character.h>
#include <room.h>
#include <object.h>
#include <storage.h>
#include <auxiliary.h>

#include "script.h"
#include "script_set.h"
#include "pychar.h"
#include "pyroom.h"
#include "pyobj.h"
#include "pymud.h"



//*****************************************************************************
//
// Auxiliary script data that we need to install into players, objects and
// rooms. Essentially, this just allows these datastructures to actually have
// scripts installed on them.
//
//*****************************************************************************
typedef struct script_aux_data {
  SCRIPT_SET *scripts;    // the set of scripts that we have
} SCRIPT_AUX_DATA;

SCRIPT_AUX_DATA *
newScriptAuxData() {
  SCRIPT_AUX_DATA *data = malloc(sizeof(SCRIPT_AUX_DATA));
  data->scripts = newScriptSet();
  return data;
}

void
deleteScriptAuxData(SCRIPT_AUX_DATA *data) {
  deleteScriptSet(data->scripts);
  free(data);
}

void
scriptAuxDataCopyTo(SCRIPT_AUX_DATA *from, SCRIPT_AUX_DATA *to) {
  copyScriptSetTo(from->scripts, to->scripts);
}

SCRIPT_AUX_DATA *
scriptAuxDataCopy(SCRIPT_AUX_DATA *data) {
  SCRIPT_AUX_DATA *newdata = newScriptAuxData();
  scriptAuxDataCopyTo(data, newdata);
  return newdata;
}

SCRIPT_AUX_DATA *
scriptAuxDataRead(STORAGE_SET *set) {
  SCRIPT_AUX_DATA  *data = newScriptAuxData();
  STORAGE_SET_LIST *list = read_list(set, "scripts");
  STORAGE_SET    *script = NULL;
  while( (script = storage_list_next(list)) != NULL)
    scriptSetAdd(data->scripts, read_int(script, "vnum"));
  return data;
}

STORAGE_SET *
scriptAuxDataStore(SCRIPT_AUX_DATA *data) {
  STORAGE_SET       *set = new_storage_set();
  STORAGE_SET_LIST *list = new_storage_list();
  LIST          *scripts = scriptSetList(data->scripts, SCRIPT_TYPE_ANY);
  SCRIPT_DATA    *script = NULL;
  store_list(set, "scripts", list, NULL);
  while((script = listPop(scripts)) != NULL) {
    STORAGE_SET *scriptset = new_storage_set();
    store_int(scriptset, "vnum", scriptGetVnum(script), NULL);
    storage_list_put(list, scriptset);
  }
  deleteList(scripts);
  return set;
}


//*****************************************************************************
//
// functions for getting script data from various datastructures
//
//*****************************************************************************
SCRIPT_SET *roomGetScripts(const ROOM_DATA *room) {
  SCRIPT_AUX_DATA *data = roomGetAuxiliaryData(room, "script_aux_data");
  return data->scripts;
}

void roomSetScripts(ROOM_DATA *room, SCRIPT_SET *scripts) {
  SCRIPT_AUX_DATA *data = roomGetAuxiliaryData(room, "script_aux_data");
  if(data->scripts) deleteScriptSet(data->scripts);
  data->scripts = scripts;
}

SCRIPT_SET *objGetScripts(const OBJ_DATA *obj) {
  SCRIPT_AUX_DATA *data = objGetAuxiliaryData(obj, "script_aux_data");
  return data->scripts;
}

void objSetScripts(OBJ_DATA *obj, SCRIPT_SET *scripts) {
  SCRIPT_AUX_DATA *data = objGetAuxiliaryData(obj, "script_aux_data");
  if(data->scripts) deleteScriptSet(data->scripts);
  data->scripts = scripts;
}

SCRIPT_SET *charGetScripts(const CHAR_DATA *ch) {
  SCRIPT_AUX_DATA *data = charGetAuxiliaryData(ch, "script_aux_data");
  return data->scripts;
}

void charSetScripts(CHAR_DATA *ch, SCRIPT_SET *scripts) {
  SCRIPT_AUX_DATA *data = charGetAuxiliaryData(ch, "script_aux_data");
  if(data->scripts) deleteScriptSet(data->scripts);
  data->scripts = scripts;
}



//*****************************************************************************
//
// functions 'n such for the script object
//
//*****************************************************************************
struct script_data {
  script_vnum vnum;
  int         type;
  char       *name;
  char       *args;
  int      num_arg;
  char       *code;
};

const char *script_type_info[NUM_SCRIPTS] = {
  "Initialization",
  "Speech",
  "Drop",
  "Give/Receive",
  "Enter",
  "Exit",
  "Command"
};

const char *scriptTypeName(int num) {
  return script_type_info[num];
}

SCRIPT_DATA *newScript() {
  SCRIPT_DATA *script = malloc(sizeof(SCRIPT_DATA));
  bzero(script, sizeof(*script));
  
  script->vnum = NOTHING;
  script->type = SCRIPT_TYPE_INIT;

  script->num_arg = 0;
  script->name    = strdup("");
  script->args    = strdup("");
  script->code    = strdup("");
  return script;
}

void         deleteScript(SCRIPT_DATA *script) {
  if(script->name) free(script->name);
  if(script->args) free(script->args);
  if(script->code) free(script->code);
  free(script);
}

SCRIPT_DATA *scriptRead(STORAGE_SET *set) {
  SCRIPT_DATA *script = malloc(sizeof(SCRIPT_DATA));
  script->vnum    = read_int(set, "vnum");
  script->type    = read_int(set, "type");
  script->num_arg = read_int(set, "narg");
  script->name    = strdup(read_string(set, "name"));
  script->args    = strdup(read_string(set, "args"));
  script->code    = strdup(read_string(set, "code"));
  // python chokes on carraige returns. Strip 'em
  format_script(&script->code, MAX_SCRIPT);
  return script;
}

STORAGE_SET *scriptStore(SCRIPT_DATA *script) {
  STORAGE_SET *set = new_storage_set();
  store_int   (set, "vnum", script->vnum, NULL);
  store_int   (set, "type", script->type, NULL);
  store_int   (set, "narg", script->num_arg, NULL);
  store_string(set, "name", script->name, NULL);
  store_string(set, "args", script->args, NULL);
  store_string(set, "code", script->code, NULL);
  return set;
}


SCRIPT_DATA *scriptCopy(SCRIPT_DATA *script) {
  SCRIPT_DATA *newscript = newScript();
  scriptCopyTo(script, newscript);
  return newscript;
}

void         scriptCopyTo(SCRIPT_DATA *from, SCRIPT_DATA *to) {
  if(to->name) free(to->name);
  if(to->args) free(to->args);
  if(to->code) free(to->code);

  to->name = strdup(from->name ? from->name : "");
  to->args = strdup(from->args ? from->args : "");
  to->code = strdup(from->code ? from->code : "");
  
  to->vnum    = from->vnum;
  to->type    = from->type;
  to->num_arg = from->num_arg;
}

script_vnum scriptGetVnum(SCRIPT_DATA *script) {
  return script->vnum;
}

int         scriptGetType(SCRIPT_DATA *script) {
  return script->type;
}

int         scriptGetNumArg(SCRIPT_DATA *script) {
  return script->num_arg;
}

const char *scriptGetArgs(SCRIPT_DATA *script) {
  return script->args;
}

const char *scriptGetName(SCRIPT_DATA *script) {
  return script->name;
}

const char *scriptGetCode(SCRIPT_DATA *script) {
  return script->code;
}

char      **scriptGetCodePtr(SCRIPT_DATA *script) {
  return &(script->code);
}

void scriptSetVnum(SCRIPT_DATA *script, script_vnum vnum) {
  script->vnum = vnum;
}

void scriptSetType(SCRIPT_DATA *script, int type) {
  script->type = type;
}

void scriptSetNumArg(SCRIPT_DATA *script, int num_arg) {
  script->num_arg = num_arg;
}

void scriptSetArgs(SCRIPT_DATA *script, const char *args) {
  if(script->args) free(script->args);
  script->args = strdup(args ? args : "");
}

void scriptSetName(SCRIPT_DATA *script, const char *name) {
  if(script->name) free(script->name);
  script->name = strdup(name ? name : "");
}

void scriptSetCode(SCRIPT_DATA *script, const char *code) {
  if(script->code) free(script->code);
  script->code = strdup(code ? code : "");
}



//*****************************************************************************
//
// stuff we need for trying to run scripts
//
//*****************************************************************************

//
// Toss a script into Python and let it run
//
void start_script(char *script) {
  // we exited with an error! Log it
  if(PyRun_SimpleString(script) < 0) {
    // hmmmm... there's gotta be a way we can get 
    // the termination messages generated from running
    log_string("script terminated with an error:\r\n"
	       "%s\r\n",
	       script);
  }
}


void init_scripts() {
  /* initialize python */
  Py_Initialize();

  /* initialize modules */
  init_PyChar();
  init_PyRoom();
  init_PyObj();
  init_PyMud();


  /* initialize our auxiliary data */
  auxiliariesInstall("script_aux_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_ROOM | AUXILIARY_TYPE_OBJ|
				       AUXILIARY_TYPE_CHAR,
				       newScriptAuxData, deleteScriptAuxData,
				       scriptAuxDataCopyTo, scriptAuxDataCopy,
				       scriptAuxDataStore, scriptAuxDataRead));
}


void finalize_scripts() {
  Py_Finalize();
}


void run_script(const char *script, void *me, int me_type,
		CHAR_DATA *ch, OBJ_DATA *obj, ROOM_DATA *room, EXIT_DATA *exit, 
		const char *cmd, const char *arg, int narg) {
  static char buf[MAX_SCRIPT];
  *buf = '\0';
  int i = 0;

  // cat all of the headers
  i += snprintf(buf+i, MAX_SCRIPT - i - 1, "from mud import *\n");
  i += snprintf(buf+i, MAX_SCRIPT - i - 1, "from char import *\n");
  i += snprintf(buf+i, MAX_SCRIPT - i - 1, "from room import *\n");
  i += snprintf(buf+i, MAX_SCRIPT - i - 1, "from obj import *\n");
  //    i += snprintf(buf+i, MAX_SCRIPT - i - 1, "from exit import *\n");

  // print the different variables
  i += snprintf(buf+i, MAX_SCRIPT - i - 1, "cmd = '%s'\n", (cmd ? cmd : ""));
  i += snprintf(buf+i, MAX_SCRIPT - i - 1, "arg = '%s'\n", (arg ? arg : ""));
  i += snprintf(buf+i, MAX_SCRIPT - i - 1, "narg = %d\n", narg);

  // print me
  if(me_type == SCRIPTOR_CHAR)
    i += snprintf(buf+i, MAX_SCRIPT - i - 1, "me = Char(%d)\n", charGetUID(me));
  else if(me_type == SCRIPTOR_OBJ)
    i += snprintf(buf+i, MAX_SCRIPT - i - 1, "me = Obj(%d)\n", objGetUID(me));
  else if(me_type == SCRIPTOR_ROOM)
    i += snprintf(buf+i, MAX_SCRIPT - i - 1, "me = Room(%d)\n",roomGetVnum(me));
  else if(me_type == SCRIPTOR_EXIT)
    ;

  // print all of the other things involved
  if(ch)
    i += snprintf(buf+i, MAX_SCRIPT - i - 1, "ch = Char(%d)\n", charGetUID(ch));
  if(obj)
    i += snprintf(buf+i, MAX_SCRIPT - i - 1, "obj = Obj(%d)\n", objGetUID(obj));
  if(room)
    i += snprintf(buf+i, MAX_SCRIPT - i - 1, "room = Room(%d)\n", roomGetVnum(room));
  if(exit)
    ;

  // cat the code
  i += snprintf(buf+i, MAX_SCRIPT - i - 1, "%s", script);

  // start the script
  start_script(buf);
}


void format_script(char **script, int max_len) {
  // python chokes on carriage returns
  replace_string(script, "\r", "", TRUE);
}


//
// Control we need to highlight
//
const char *control_table[] = {
  "while",
  "elif",
  "else",
  "def",
  "for",
  "if",
  "in",
  "is",
  "and",
  "or",
  "not",
  NULL
};


//
// returns which control string we found. returns
// -1 if none were found
//
int check_for_control(const char *ptr, int i) {
  int syn_i;
  for(syn_i = 0; control_table[syn_i] != NULL; syn_i++) {
    int len = strlen(control_table[syn_i]);
    // not enough characters for it to exist
    if(i - len + 1 < 0)
      continue;
    // we found it might have found it. Check to make
    // sure that we are surrounded by spaces or colons
    if(!strncasecmp(ptr+i-len+1, control_table[syn_i], len)) {
      // check the left side first
      if(!(i - len < 0 || isspace(ptr[i-len]) || ptr[i-len] == ':'))
	continue;
      //  and now the right side
      if(!(ptr+i+1 == '\0' || isspace(ptr[i+1]) || ptr[i+1] == ':'))
	continue;

      return syn_i;
    }
  }

  // didn't find any
  return -1;
}


void script_display(SOCKET_DATA *sock, const char *script, bool show_line_nums){
  const char *ptr = script;//buffer_string(sock->text_editor);
  char line[SMALL_BUFFER] = "\0";
  int  line_num = 1;
  int  line_i = 0, i = 0;
  bool in_line_comment = FALSE; // are we displaying a comment?
  bool in_digit = FALSE;        // are we displaying a digit?
  bool in_string  = FALSE;      // how about a string?
  char string_type = '"';       // what kinda string marker is it? ' or " ?
  int  syn_to_color = -1;       // if we're coloring flow control, which one?

  for(i = 0; ptr[i] != '\0'; i++) {
    // take off the color for digits
    if(in_digit && !isdigit(ptr[i])) {
      sprintf(line+line_i, "{g");
      line_i += 2;
      in_digit = FALSE;
    } // NO ELSE ... we might need to color something else

    // transfer over the character
    line[line_i] = ptr[i];

    // if the character is a #, color the comment red
    if(ptr[i] == '#') {
      sprintf(line+line_i, "{r#");
      line_i += 3;
      in_line_comment = TRUE;
    }

    // we've found a digit that we have to color in
    else if(isdigit(ptr[i]) && !in_digit && !in_line_comment && !in_string) {
      sprintf(line+line_i, "{y%c", ptr[i]);
      line_i += 3;
      in_digit = TRUE;
    }

    // if we've found a string marker, color/uncolor it
    else if((ptr[i] == '"' || ptr[i] == '\'') && !in_line_comment &&
	    // if we're already coloring a string and the marker
	    // types don't match up, then don't worry about it
	    !(in_string && string_type != ptr[i])) {

      if(in_string && ptr[i] == string_type)
	sprintf(line+line_i, "\%c{g", string_type);
      else
	sprintf(line+line_i, "{w%c", ptr[i]);
      
      line_i += 3;
      in_string = (in_string + 1) % 2;
      string_type = ptr[i];
    }

    // we've hit a new line
    else if(ptr[i] == '\n') {

	// do we need to show line numbers
	char line_num_info[20];
	if(show_line_nums)
	  sprintf(line_num_info, "{c%2d]  ", line_num);
	else
	  *line_num_info = '\0';

	line[line_i] = '\0';
	send_to_socket(sock, "%s{g%s\r\n", line_num_info, line);
	*line = '\0';
	line_i = 0;
	line_num++;
	in_line_comment = in_string = FALSE; // reset on newline
    }

    // checking while, for, if, else, elif, etc...
    // this is kinda tricky. We have to backtrack and check some stuff
    else if(!(in_line_comment || in_digit || in_string) &&
	    (syn_to_color = check_for_control(ptr, i)) != -1) {
      sprintf(line+line_i-strlen(control_table[syn_to_color])+1,
	      "{p%s{g", control_table[syn_to_color]);
      line_i += 5; // the two markers for the color, and one for new character
    }

    // didn't find anything of interest
    else
	line_i++;
  }

  line[line_i] = '\0';
  // send the last line
  if(*line)
    send_to_socket(sock, "{c%2d]{g  %s\r\n", line_num, line);
  // there was nothing on the first line
  else if(line_num == 1)
    send_to_socket(sock, "The buffer is empty.\r\n");

  if(ptr[strlen(ptr)-1] != '\n')
    send_to_socket(sock, "Buffer does not end in newline!\r\n");
}


//*****************************************************************************
//
// tries for various speech triggers
//
//*****************************************************************************
void try_speech_script_with(CHAR_DATA *ch, CHAR_DATA *listener, char *speech) {
  LIST *speech_scripts = scriptSetList(charGetScripts(listener), 
				       SCRIPT_TYPE_SPEECH);
  SCRIPT_DATA *script = NULL;

  while( (script = listPop(speech_scripts)) != NULL) {
    if(is_keyword(scriptGetArgs(script), speech, FALSE)) {
      run_script(scriptGetCode(script),
		 listener, SCRIPTOR_CHAR,
		 ch, NULL, charGetRoom(listener), NULL, NULL, speech, 0);
    }
  }
  deleteList(speech_scripts);
}

void try_speech_script(CHAR_DATA *ch, CHAR_DATA *listener, char *speech) {
  if(listener != NULL)
    try_speech_script_with(ch, listener, speech);
  else {
    LIST_ITERATOR *char_i = newListIterator(roomGetCharacters(charGetRoom(ch)));
    ITERATE_LIST(listener, char_i)
      try_speech_script_with(ch, listener, speech);
    deleteListIterator(char_i);
  }
}


void try_enterance_script(CHAR_DATA *ch, ROOM_DATA *room, 
			  EXIT_DATA *exit, const char *dirname) {
  // check the room
  try_scripts(SCRIPT_TYPE_ENTER,
	      room, SCRIPTOR_ROOM,
	      ch, NULL, room, exit, dirname, NULL, 0);

  // check everyone in the room
  LIST_ITERATOR *char_i = newListIterator(roomGetCharacters(room));
  CHAR_DATA *greeter = NULL;
  ITERATE_LIST(greeter, char_i) {
    if(greeter == ch) 
      continue;
    try_scripts(SCRIPT_TYPE_ENTER,
		greeter, SCRIPTOR_CHAR,
		ch, NULL, room, exit, dirname, NULL, 0);
  }
  deleteListIterator(char_i);
}


void try_exit_script(CHAR_DATA *ch, ROOM_DATA *room, 
		     EXIT_DATA *exit, const char *dirname) {
  // check the room
  try_scripts(SCRIPT_TYPE_EXIT,
	      room, SCRIPTOR_ROOM,
	      ch, NULL, room, exit, dirname, NULL, 0);

  // check everyone in the room
  LIST_ITERATOR *char_i = newListIterator(roomGetCharacters(room));
  CHAR_DATA *watcher = NULL;
  while( (watcher = listIteratorCurrent(char_i)) != NULL) {
    listIteratorNext(char_i);
    if(watcher == ch) 
      continue;
    try_scripts(SCRIPT_TYPE_EXIT,
		watcher, SCRIPTOR_CHAR,
		ch, NULL, room, exit, dirname, NULL, 0);
  }
  deleteListIterator(char_i);
}


int try_command_script(CHAR_DATA *ch, const char *cmd, const char *arg) {
  int retval = 0;

  // the room had a command script, and we have
  // to halt the normal command from going through
  if(try_scripts(SCRIPT_TYPE_COMMAND,
		 charGetRoom(ch), SCRIPTOR_ROOM,
		 ch, NULL, charGetRoom(ch), NULL, cmd, arg, 0))
    retval = 1;

  // check everyone in the room
  LIST_ITERATOR *char_i = newListIterator(roomGetCharacters(charGetRoom(ch)));
  CHAR_DATA *scriptor = NULL;
  while( (scriptor = listIteratorCurrent(char_i)) != NULL) {
    listIteratorNext(char_i);
    if(scriptor == ch) 
      continue;
    if(try_scripts(SCRIPT_TYPE_COMMAND,
		   scriptor, SCRIPTOR_CHAR,
		   ch, NULL, charGetRoom(ch), NULL, cmd, arg, 0))
      retval = 1;
  }
  deleteListIterator(char_i);
  return retval;
}


int try_scripts(int script_type,
		void *me, int me_type,
		CHAR_DATA *ch, OBJ_DATA *obj, ROOM_DATA *room, EXIT_DATA *exit,
		const char *cmd, const char *arg, int narg) {
  int retval = 0;
  LIST *scripts = NULL;
  SCRIPT_DATA *script = NULL;

  if(me_type == SCRIPTOR_CHAR)
    scripts = scriptSetList(charGetScripts(me), script_type);
  if(me_type == SCRIPTOR_ROOM)
    scripts = scriptSetList(roomGetScripts(me), script_type);
  if(me_type == SCRIPTOR_OBJ)
    scripts = scriptSetList(objGetScripts(me), script_type);
  /*
  if(me_type == SCRIPTOR_EXIT)
    scripts = scriptSetGetList(exitGetScripts(me), script_type);
  */

  // see if we meet the script requirements
  while( (script = listPop(scripts)) != NULL) {
    // make specific checks by script type
    switch(scriptGetType(script)) {
    case SCRIPT_TYPE_GIVE:
    case SCRIPT_TYPE_ENTER:
    case SCRIPT_TYPE_EXIT:
      // if the scriptor is a char and narg is 1,
      // we have to make sure we can see the character
      // before we run the script
      if(me_type == SCRIPTOR_CHAR && 
	 scriptGetNumArg(script) == 1 &&
	 ch && !can_see_person(me, ch))
	continue;
      break;

    case SCRIPT_TYPE_COMMAND:
      // if the keyword isn't on our list, continue
      if(!is_keyword(scriptGetArgs(script), cmd, FALSE))
	continue;
      // if the numeric argument of the script is 1,
      // we need to switch our retval to 1 and return
      // it so we know not to carry on with the normal command
      if(scriptGetNumArg(script) == 1)
	retval = 1;
      break;

    default:
      break;
    }

    run_script(scriptGetCode(script), me, me_type,
	       ch, obj, room, exit, cmd, arg, narg);    
  }
  deleteList(scripts);
  return retval;
}
