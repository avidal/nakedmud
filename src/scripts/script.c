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
#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../world.h"
#include "../character.h"
#include "../room.h"
#include "../object.h"
#include "../storage.h"
#include "../auxiliary.h"

#include "script.h"
#include "script_set.h"
#include "pychar.h"
#include "pyroom.h"
#include "pyobj.h"
#include "pymud.h"

// online editor stuff
#include "../editor/editor.h"
#include "script_editor.h"
#include "../olc2/olc.h"



//*****************************************************************************
// local functions and commands
//*****************************************************************************

//
// Takes a script and runs it. Unlike typical scripts, this is not a trigger.
// This is like running a program, in the form of a script.
//    usage: scrun <vnum> [arguments] 
COMMAND(cmd_scrun) {
  // make sure we at least have a vnum
  if(!arg || !*arg)
    send_to_char(ch, "Which script would you like to run?\r\n");
  else {
    char buf[SMALL_BUFFER];

    // pull out the vnum
    arg = one_arg(arg, buf);

    SCRIPT_DATA *script = worldGetScript(gameworld, atoi(buf));
    if(script == NULL || !isdigit(*buf))
      send_to_char(ch, "No script with that vnum exists!\r\n");
    else if(scriptGetType(script) != SCRIPT_TYPE_RUNNABLE)
      send_to_char(ch, "That script is not runnable!\r\n");
    else if(charGetLevel(ch) < scriptGetNumArg(script))
      send_to_char(ch, "You are not high enough level to run that script!\r\n");
    else {
      send_to_char(ch, "Ok.\r\n");
      run_script(scriptGetCode(script), ch, SCRIPTOR_CHAR, NULL, NULL, NULL,
		 NULL, arg, 0);
    }
  }
}



//*****************************************************************************
// Auxiliary script data that we need to install into players, objects and
// rooms. Essentially, this just allows these datastructures to actually have
// scripts installed on them.
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
  store_list(set, "scripts", list);
  while((script = listPop(scripts)) != NULL) {
    STORAGE_SET *scriptset = new_storage_set();
    store_int(scriptset, "vnum", scriptGetVnum(script));
    storage_list_put(list, scriptset);
  }
  deleteList(scripts);
  return set;
}


//*****************************************************************************
// functions for getting script data from various datastructures
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
// functions 'n such for the script object
//*****************************************************************************
struct script_data {
  script_vnum vnum;
  int         type;
  char       *name;
  char       *args;
  int      num_arg;
  BUFFER     *code;
};

const char *script_type_info[NUM_SCRIPTS] = {
  "Initialization",
  "Speech",
  "Drop",
  "Give/Receive",
  "Enter",
  "Exit",
  "Command",
  "Runnable"
};

const char *scriptTypeName(int num) {
  return script_type_info[num];
}

SCRIPT_DATA *newScript(void) {
  SCRIPT_DATA *script = malloc(sizeof(SCRIPT_DATA));
  bzero(script, sizeof(*script));
  
  script->vnum = NOTHING;
  script->type = SCRIPT_TYPE_INIT;

  script->num_arg = 0;
  script->name    = strdup("");
  script->args    = strdup("");
  script->code    = newBuffer(1);
  return script;
}

void         deleteScript(SCRIPT_DATA *script) {
  if(script->name) free(script->name);
  if(script->args) free(script->args);
  if(script->code) deleteBuffer(script->code);
  free(script);
}

SCRIPT_DATA *scriptRead(STORAGE_SET *set) {
  SCRIPT_DATA *script = newScript();
  scriptSetVnum(script, read_int(set, "vnum"));
  scriptSetType(script, read_int(set, "type"));
  scriptSetNumArg(script, read_int(set, "narg"));
  scriptSetName(script, read_string(set, "name"));
  scriptSetArgs(script, read_string(set, "args"));
  scriptSetCode(script, read_string(set, "code"));
  // python chokes on carraige returns. Strip 'em
  bufferReplace(script->code, "\r", "", TRUE);
  return script;
}

STORAGE_SET *scriptStore(SCRIPT_DATA *script) {
  STORAGE_SET *set = new_storage_set();
  store_int   (set, "vnum", script->vnum);
  store_int   (set, "type", script->type);
  store_int   (set, "narg", script->num_arg);
  store_string(set, "name", script->name);
  store_string(set, "args", script->args);
  store_string(set, "code", bufferString(script->code));
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

  to->name = strdup(from->name ? from->name : "");
  to->args = strdup(from->args ? from->args : "");
  bufferCopyTo(from->code, to->code);
  
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
  return bufferString(script->code);
}

BUFFER *scriptGetCodeBuffer(SCRIPT_DATA *script) {
  return script->code;
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
  bufferClear(script->code);
  bufferCat(script->code, code);
}



//*****************************************************************************
// stuff we need for trying to run scripts
//*****************************************************************************

//
// Many thanks to Xanthros who pointed out that, if two scripts trigger
// eachother, we could get tossed into an infinite loop. He suggested a
// check for loop depth before each script is run. If we are deeper than
// some maximum depth, do not run the script.
#define MAX_LOOP_DEPTH   30
int script_loop_depth   = 0;


//
// Python makes it overly complicated (IMO) to get traceback information when
// running in C. I spent some time hunting around, and came across a lovely
// site that provided me with this function for getting a traceback of an error.
// The site that provided this piece of code to me is located here:
//   http://stompstompstomp.com/weblog/technical/2004-03-29
char* getPythonTraceback()
{
    // Python equivilant:
    // import traceback, sys
    // return "".join(traceback.format_exception(sys.exc_type, 
    //    sys.exc_value, sys.exc_traceback))

    PyObject *type, *value, *traceback;
    PyObject *tracebackModule;
    char *chrRetval;

    PyErr_Fetch(&type, &value, &traceback);

    tracebackModule = PyImport_ImportModule("traceback");
    if (tracebackModule != NULL)
    {
        PyObject *tbList, *emptyString, *strRetval;

        tbList = PyObject_CallMethod(
            tracebackModule, 
            "format_exception", 
            "OOO",
            type,
            value == NULL ? Py_None : value,
            traceback == NULL ? Py_None : traceback);

        emptyString = PyString_FromString("");
        strRetval = PyObject_CallMethod(emptyString, "join", 
            "O", tbList);

        chrRetval = strdup(PyString_AsString(strRetval));

        Py_DECREF(tbList);
        Py_DECREF(emptyString);
        Py_DECREF(strRetval);
        Py_DECREF(tracebackModule);
    }
    else
    {
        chrRetval = strdup("Unable to import traceback module.");
    }

    Py_DECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);

    return chrRetval;
}


//
// Return a new dictionary with all the basic modules imported
PyObject *newScriptDict() {
  PyObject* dict = PyDict_New();
  
  // Check for __builtins__...
  if (PyDict_GetItemString(dict, "__builtins__") == NULL) {
    // Hm... no __builtins__ eh?
    PyObject* builtinMod = PyImport_ImportModule("__builtin__");
    if (builtinMod == NULL || 
	PyDict_SetItemString(dict, "__builtins__", builtinMod) != 0) {
      Py_DECREF(dict);
      Py_XDECREF(dict);
      // error handling
      return NULL;
    }
    Py_DECREF(builtinMod);
  }

  PyObject *sys = PyImport_ImportModule("sys");
  if(sys != NULL) {
    PyObject *exit = PyDict_GetItemString(PyModule_GetDict(sys), "exit");
    if(exit != NULL)
      PyDict_SetItemString(dict, "exit", exit);
    Py_DECREF(sys);
  }
  
  // merge all of the mud module contents with our current dict
  PyObject *mudmod = PyImport_ImportModule("mud");
  PyDict_Update(dict, PyModule_GetDict(mudmod));
  Py_DECREF(mudmod);
  mudmod = PyImport_ImportModule("char");
  PyDict_Update(dict, PyModule_GetDict(mudmod));
  Py_DECREF(mudmod);
  mudmod = PyImport_ImportModule("room");
  PyDict_Update(dict, PyModule_GetDict(mudmod));
  Py_DECREF(mudmod);
  mudmod = PyImport_ImportModule("obj");
  PyDict_Update(dict, PyModule_GetDict(mudmod));
  Py_DECREF(mudmod);

  return dict;
}


//
// Decrement the reference count of the dictionary
void deleteScriptDict(PyObject *dict) {
  Py_DECREF(dict);
}


//
// Toss a script into Python and let it run
void start_script(PyObject *dict, const char *script) {
  if(script_loop_depth < MAX_LOOP_DEPTH) {
    script_loop_depth++;
    PyObject* compileRetval = PyRun_String(script, Py_file_input, dict, dict);
    script_loop_depth--;
    // we threw an error and it wasn't an intentional
    // system exit error. Now print the backtrace
    if(compileRetval == NULL && PyErr_Occurred() != PyExc_SystemExit) {
      char *tb = getPythonTraceback();
      log_string("script terminated with an error:\r\n%s\r\n"
		 "\r\nTraceback is:\r\n%s\r\n", script, tb);
      free(tb);
    }
  }
}


void init_scripts() {
  // initialize python
  Py_Initialize();

  // initialize modules
  init_PyChar();
  init_PyRoom();
  init_PyObj();
  init_PyMud();


  // initialize our auxiliary data
  auxiliariesInstall("script_aux_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_ROOM | AUXILIARY_TYPE_OBJ|
				       AUXILIARY_TYPE_CHAR,
				       newScriptAuxData, deleteScriptAuxData,
				       scriptAuxDataCopyTo, scriptAuxDataCopy,
				       scriptAuxDataStore, scriptAuxDataRead));

  extern COMMAND(cmd_scedit); // define the command
  add_cmd("scedit", NULL, cmd_scedit, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_SCRIPTER, FALSE, TRUE);
  add_cmd("scrun", NULL, cmd_scrun, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);

  init_script_editor();
}


void finalize_scripts() {
  Py_Finalize();
}


void run_script(const char *script, void *me, int me_type,
		CHAR_DATA *ch, OBJ_DATA *obj, ROOM_DATA *room, 
		const char *cmd, const char *arg, int narg) {
  PyObject *dict = newScriptDict();
  // now, import all of our command and argument variables
  if(cmd) {
    PyObject *pycmd = PyString_FromString(cmd);
    PyDict_SetItemString(dict, "cmd", pycmd);
    Py_DECREF(pycmd);
  }
  if(arg) {
    PyObject *pyarg = PyString_FromString(arg);
    PyDict_SetItemString(dict, "arg", pyarg);
    Py_DECREF(pyarg);
  }
  if(TRUE) {
    PyObject *pynarg = PyInt_FromLong(narg);
    PyDict_SetItemString(dict, "narg", pynarg);
    Py_DECREF(pynarg);
  }

  // now import everyone who is involved
  if(me) {
    PyObject *pyme = NULL;
    switch(me_type) {
    case SCRIPTOR_CHAR:  pyme = newPyChar(me); break;
    case SCRIPTOR_OBJ:   pyme = newPyObj(me);  break;
    case SCRIPTOR_ROOM:  pyme = newPyRoom(me); break;
    }
    PyDict_SetItemString(dict, "me", pyme);
    Py_DECREF(pyme);
  }
  if(ch) {
    PyObject *pych = newPyChar(ch);
    PyDict_SetItemString(dict, "ch", pych);
    Py_DECREF(pych);
  }
  if(room) {
    PyObject *pyroom = newPyRoom(room);
    PyDict_SetItemString(dict, "room", pyroom);
    Py_DECREF(pyroom);
  }    
  if(obj) {
    PyObject *pyobj = newPyObj(obj);
    PyDict_SetItemString(dict, "obj", pyobj);
    Py_DECREF(pyobj);
  }    

  // start the script
  start_script(dict, script);
  deleteScriptDict(dict);
}



//*****************************************************************************
// Stuff we need for formatting and coloring scripts on screen
//*****************************************************************************
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
// tries for various speech triggers
//*****************************************************************************
void try_speech_script_with(CHAR_DATA *ch, CHAR_DATA *listener, char *speech) {
  LIST *speech_scripts = scriptSetList(charGetScripts(listener), 
				       SCRIPT_TYPE_SPEECH);
  SCRIPT_DATA *script = NULL;

  while( (script = listPop(speech_scripts)) != NULL) {
    if(is_keyword(scriptGetArgs(script), speech, FALSE)) {
      run_script(scriptGetCode(script),
		 listener, SCRIPTOR_CHAR,
		 ch, NULL, charGetRoom(listener), NULL, speech, 0);
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


void try_enterance_script(CHAR_DATA *ch, ROOM_DATA *room, const char *dirname) {
  // check the room
  try_scripts(SCRIPT_TYPE_ENTER,
	      room, SCRIPTOR_ROOM,
	      ch, NULL, room, dirname, NULL, 0);

  // check everyone in the room
  LIST_ITERATOR *char_i = newListIterator(roomGetCharacters(room));
  CHAR_DATA *greeter = NULL;
  ITERATE_LIST(greeter, char_i) {
    if(greeter == ch) 
      continue;
    try_scripts(SCRIPT_TYPE_ENTER,
		greeter, SCRIPTOR_CHAR,
		ch, NULL, room, dirname, NULL, 0);
  }
  deleteListIterator(char_i);
}


void try_exit_script(CHAR_DATA *ch, ROOM_DATA *room, const char *dirname) {
  // check the room
  try_scripts(SCRIPT_TYPE_EXIT,
	      room, SCRIPTOR_ROOM,
	      ch, NULL, room, dirname, NULL, 0);

  // check everyone in the room
  LIST_ITERATOR *char_i = newListIterator(roomGetCharacters(room));
  CHAR_DATA *watcher = NULL;
  while( (watcher = listIteratorCurrent(char_i)) != NULL) {
    listIteratorNext(char_i);
    if(watcher == ch) 
      continue;
    try_scripts(SCRIPT_TYPE_EXIT,
		watcher, SCRIPTOR_CHAR,
		ch, NULL, room, dirname, NULL, 0);
  }
  deleteListIterator(char_i);
}


int try_command_script(CHAR_DATA *ch, const char *cmd, const char *arg) {
  int retval = 0;

  // the room had a command script, and we have
  // to halt the normal command from going through
  if(try_scripts(SCRIPT_TYPE_COMMAND,
		 charGetRoom(ch), SCRIPTOR_ROOM,
		 ch, NULL, charGetRoom(ch), cmd, arg, 0))
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
		   ch, NULL, charGetRoom(ch), cmd, arg, 0))
      retval = 1;
  }
  deleteListIterator(char_i);
  return retval;
}


int try_scripts(int script_type,
		void *me, int me_type,
		CHAR_DATA *ch, OBJ_DATA *obj, ROOM_DATA *room,
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
	 ch && !can_see_char(me, ch))
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

    run_script(scriptGetCode(script), me, me_type, ch, obj, room, cmd, 
	       arg, narg);    
  }
  deleteList(scripts);
  return retval;
}
