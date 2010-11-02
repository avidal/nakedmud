//*****************************************************************************
//
// scripts.c
//
// NakedMud makes extensive use of scripting. It uses scripting to generate
// objects, mobiles, and rooms when they are loaded into the game. There are
// also scripting hooks for these things (commonly referred to as triggers), 
// which allow them to be a bit more dynamic and flavorful in the game. For
// instance, greetings when someone enters a room, repsonses to questions, 
// actions when items are received.. you know... that sort of stuff. 
//
//*****************************************************************************
#include "../mud.h"
#include "../utils.h"
#include "../world.h"
#include "../character.h"
#include "../room.h"
#include "../object.h"
#include "../socket.h"
#include "../auxiliary.h"
#include "../storage.h"
#include "../handler.h"
#include "../hooks.h"

#include "scripts.h"
#include "pyplugs.h"
#include "pychar.h"
#include "pyroom.h"
#include "pyexit.h"
#include "pyobj.h"
#include "pymud.h"
#include "pyevent.h"
#include "pystorage.h"
#include "pyauxiliary.h"
#include "trighooks.h"

// online editor stuff
#include "../editor/editor.h"
#include "script_editor.h"



//*****************************************************************************
// auxiliary data
//*****************************************************************************
typedef struct {
  LIST *triggers;
} TRIGGER_AUX_DATA;

TRIGGER_AUX_DATA *newTriggerAuxData(void) {
  TRIGGER_AUX_DATA *data = malloc(sizeof(TRIGGER_AUX_DATA));
  data->triggers         = newList();
  return data;
}

void deleteTriggerAuxData(TRIGGER_AUX_DATA *data) {
  deleteListWith(data->triggers, free);
  free(data);
}

void triggerAuxDataCopyTo(TRIGGER_AUX_DATA *from, TRIGGER_AUX_DATA *to) {
  deleteListWith(to->triggers, free);
  to->triggers = listCopyWith(from->triggers, strdup);
}

TRIGGER_AUX_DATA *triggerAuxDataCopy(TRIGGER_AUX_DATA *data) {
  TRIGGER_AUX_DATA *newdata = malloc(sizeof(TRIGGER_AUX_DATA));
  newdata->triggers = listCopyWith(data->triggers, strdup);
  return newdata;
}

char *read_one_trigger(STORAGE_SET *set) {
  return strdup(read_string(set, "trigger"));
}

TRIGGER_AUX_DATA *triggerAuxDataRead(STORAGE_SET *set) {
  TRIGGER_AUX_DATA *data = malloc(sizeof(TRIGGER_AUX_DATA));
  data->triggers = gen_read_list(read_list(set, "triggers"), read_one_trigger);
  return data;
}

STORAGE_SET *store_one_trigger(char *key) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "trigger", key);
  return set;
}

STORAGE_SET *triggerAuxDataStore(TRIGGER_AUX_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_list(set, "triggers", gen_store_list(data->triggers,store_one_trigger));
  return set;
}



//*****************************************************************************
// local datastructures, functions, and defines
//*****************************************************************************

//
// a stack that keeps track of the locale scripts are running in
LIST *locale_stack = NULL;

//
// Many thanks to Xanthros who pointed out that, if two scripts trigger
// eachother, we could get tossed into an infinite loop. He suggested a
// check for loop depth before each script is run. If we are deeper than
// some maximum depth, do not run the script.
#define MAX_LOOP_DEPTH   30
int script_loop_depth   = 0;

// a local variable used for storing whether or not the last script ran fine
bool script_ok = TRUE;


//
// looks for dynamic descs and expands them out as needed. Dynamic descs are
// bits of code that are embedded within descriptions, and surrounded by [ and
// ]. They can be anything that returns a  (string or numeric) value. Code must
// be a single statement. To perform conditional output, it is common to use 
// the ite() (if, then, else) function, which takes 2 arguments and an optional
// third.Here would be some valid dynamic descriptions (assuming that the 
// variables I made up exist in your mud):
//   You see [me.getvar("flowers")] flowers blooming here.
//   [ite(ch.perception >10, "There is a large bird's nest on the east cliff.")]
//   [ite(ch.name=="Bob", "You are in your house.", "You are in Bob's house.")]
//   You are in [ite(ch.name == "Bob", "your", ch.name + "'s")] house.
void expand_dynamic_descs(BUFFER *desc, PyObject *me, CHAR_DATA *ch) {
  // make a new temp buffer to hold all of the expanded data
  BUFFER *new_desc = newBuffer(bufferLength(desc)*2);
  char code[SMALL_BUFFER];
  PyObject   *dict = NULL;

  int start, end, i, size = bufferLength(desc);
  for(i = 0; i < size; i++) {
    // figure out when our next dynamic desc is.
    start = next_letter_in(bufferString(desc) + i, '[');

    // no more
    if(start == -1) {
      // copy the rest and skip to the end of the buffer
      bprintf(new_desc, "%s", bufferString(desc) + i);
      i = size - 1;
    }
    // we have another desc
    else {
      // copy everything up to start
      while(start > 0) {
	bprintf(new_desc, "%c", *(bufferString(desc) + i));
	start--;
	i++;
      }

      // skip the start marker
      i++;

      // find our end
      end = next_letter_in(bufferString(desc) + i, ']');

      // make sure we have it
      if(end == -1)
	break;

      // copy everything between start and end
      strncpy(code, bufferString(desc) + i, end);
      code[end] = '\0';

      // skip i up to the end
      i = i + end;

      // if we haven't already created a dict, do it now
      if(dict == NULL) {
	PyObject *pych = newPyChar(ch);
	dict = restricted_script_dict();
	PyDict_SetItemString(dict, "me", me);
	PyDict_SetItemString(dict, "ch", pych);
	Py_DECREF(pych);
      }

      // evaluate the code
      PyObject *retval = PyRun_String(code, Py_eval_input, dict, dict);

      // did we encounter an error?
      if(retval == NULL) {
	char *tb = getPythonTraceback();
	log_string("Dynamic desc terminated with an error:\r\n%s\r\n"
		   "\r\nTraceback is:\r\n%s\r\n", code, tb);
	free(tb);
	break;
      }
      // append the output
      else if(PyString_Check(retval))
	bprintf(new_desc, "%s", PyString_AsString(retval));
      else if(PyInt_Check(retval))
	bprintf(new_desc, "%ld", PyInt_AsLong(retval));
      else if(PyFloat_Check(retval))
	bprintf(new_desc, "%lf", PyFloat_AsDouble(retval));
      // invalid return type...
      else if(retval != Py_None)
	log_string("dynamic desc had invalid evaluation: %s", code);

      Py_XDECREF(retval);
    }
  }

  // copy over the changes, and free our buffer
  bufferCopyTo(new_desc, desc);
  deleteBuffer(new_desc);

  // free up our dictionary
  Py_XDECREF(dict);
}

void expand_char_dynamic_descs(BUFFER *desc, CHAR_DATA *me, CHAR_DATA *ch) {
  PyObject *pyme = newPyChar(me);
  expand_dynamic_descs(desc, pyme, ch);
  Py_DECREF(pyme);
}

void  expand_obj_dynamic_descs(BUFFER *desc, OBJ_DATA *me,  CHAR_DATA *ch) {
  PyObject *pyme = newPyObj(me);
  expand_dynamic_descs(desc, pyme, ch);
  Py_DECREF(pyme);
}

void expand_room_dynamic_descs(BUFFER *desc, ROOM_DATA *me, CHAR_DATA *ch) {
  PyObject *pyme = newPyRoom(me);
  expand_dynamic_descs(desc, pyme, ch);
  Py_DECREF(pyme);
}

void finalize_scripts(void *none1, void *none2, void *none3) {
  Py_Finalize();
}



//*****************************************************************************
// player commands
//*****************************************************************************
//
// displays info on a script to a person
COMMAND(cmd_tstat) {
  if(!charGetSocket(ch))
    return;
  else {
    TRIGGER_DATA *trig = 
      worldGetType(gameworld, "trigger", get_fullkey_relative(arg, 
			      get_key_locale(roomGetClass(charGetRoom(ch)))));
    if(trig == NULL)
      send_to_char(ch, "No trigger exists with that key.\r\n");
    else {
      send_to_socket(charGetSocket(ch),
		     "--------------------------------------------------------------------------------\r\n"
		     "Name         : %s\r\n"
		     "Trigger type : %s\r\n"
		     "--------------------------------------------------------------------------------\r\n",
		     triggerGetName(trig), 
		     triggerGetType(trig));
      script_display(charGetSocket(ch), triggerGetCode(trig), FALSE);
    }
  }
}


//
// attach a new trigger to the given instanced object/mobile/room
COMMAND(cmd_attach) {
  TRIGGER_DATA *trig = NULL;
  char          *key = NULL;
  void          *tgt = NULL;
  int     found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, 
		 "word [to] { ch.room obj.room.inv.eq room }",
		 &key, &tgt, &found_type))
    return;

  // check to make sure our key is OK
  if((trig = worldGetType(gameworld, "trigger", 
          get_fullkey_relative(key, 
	      get_key_locale(roomGetClass(charGetRoom(ch)))))) == NULL)
    send_to_char(ch, "No trigger exists with the key, %s.\r\n", key);
  else {
    // what are we trying to attach it to?
    if(found_type == PARSE_CHAR) {
      send_to_char(ch, "Trigger %s attached to %s.\r\n", key, charGetName(tgt));
      triggerListAdd(charGetTriggers(tgt), triggerGetKey(trig));
    }
    else if(found_type == PARSE_ROOM) {
      send_to_char(ch, "Trigger %s attached to %s.\r\n", key, roomGetName(tgt));
      triggerListAdd(roomGetTriggers(tgt), triggerGetKey(trig));
    }
    else {
      send_to_char(ch, "Trigger %s attached to %s.\r\n", key, objGetName(tgt));
      triggerListAdd(objGetTriggers(tgt), triggerGetKey(trig));
    }
  }  
}


//
// detach a trigger from to the given instanced object/mobile/room
COMMAND(cmd_detach) {
  TRIGGER_DATA *trig = NULL;
  char          *key = NULL;
  void          *tgt = NULL;
  int     found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, 
		 "word [from] { ch.room obj.room.inv.eq room }",
		 &key, &tgt, &found_type))
    return;

  // check to make sure our key is OK
  if((trig = worldGetType(gameworld, "trigger", 
          get_fullkey_relative(key, 
	      get_key_locale(roomGetClass(charGetRoom(ch)))))) == NULL)
    send_to_char(ch, "which trigger did you want to detach?\r\n");
  else {
    // what are we trying to detach the trigger from?
    if(found_type == PARSE_CHAR) {
      send_to_char(ch, "Trigger %s detached from %s.\r\n", key,
		   charGetName(tgt));
      triggerListRemove(charGetTriggers(tgt), triggerGetKey(trig));
    }
    else if(found_type == PARSE_ROOM) {
      send_to_char(ch, "Trigger %s detached from %s.\r\n", key,
		   roomGetName(tgt));
      triggerListRemove(roomGetTriggers(tgt), triggerGetKey(trig));
    }
    else {
      send_to_char(ch, "Trigger %s detached to %s.\r\n", key,
		   objGetName(tgt));
      triggerListRemove(objGetTriggers(tgt), triggerGetKey(trig));
    }
  }
}

const char *triggerGetListType(TRIGGER_DATA *trigger) {
  static char buf[SMALL_BUFFER];
  sprintf(buf, "%-40s %13s", triggerGetName(trigger), triggerGetType(trigger));
  return buf;
}

// this is used for the header when printing out zone trigger info
#define TRIGGER_LIST_HEADER \
"Name                                              Type"

COMMAND(cmd_tlist) {
  do_list(ch, (arg&&*arg?arg:get_key_locale(roomGetClass(charGetRoom(ch)))),
	  "trigger", TRIGGER_LIST_HEADER, triggerGetListType);
}

COMMAND(cmd_tdelete) {
  do_delete(ch, "trigger", deleteTrigger, arg);
}

COMMAND(cmd_trename) {
  char from[SMALL_BUFFER];
  arg = one_arg(arg, from);
  do_rename(ch, "trigger", from, arg);
}




//*****************************************************************************
// implementation of scripts.h - triggers portion in triggers.c
//*****************************************************************************
void init_scripts(void) {
  // create our locale stack
  locale_stack = newList();

  // initialize python
  Py_Initialize();

  // initialize all of our modules written in C
  init_PyAuxiliary();
  init_PyEvent();
  init_PyStorage();
  init_PyChar();
  init_PyRoom();
  init_PyExit();
  init_PyObj();
  init_PyMud();

  // initialize all of our modules written in Python
  init_pyplugs();

  // initialize the other parts to this module
  init_script_editor();
  init_trighooks();

  // so triggers can be saved to/loaded from disk
  worldAddType(gameworld, "trigger", triggerRead, triggerStore, deleteTrigger,
	       triggerSetKey);

  // deal with auxiliary data
  auxiliariesInstall("trigger_data", 
		     newAuxiliaryFuncs(AUXILIARY_TYPE_CHAR | AUXILIARY_TYPE_OBJ|
				       AUXILIARY_TYPE_ROOM,
				       newTriggerAuxData,  deleteTriggerAuxData,
				       triggerAuxDataCopyTo, triggerAuxDataCopy,
				       triggerAuxDataStore,triggerAuxDataRead));

  // add in some hooks for preprocessing scripts embedded in descs
  hookAdd("preprocess_room_desc", expand_room_dynamic_descs);
  hookAdd("preprocess_char_desc", expand_char_dynamic_descs);
  hookAdd("preprocess_obj_desc",  expand_obj_dynamic_descs);
  hookAdd("shutdown",             finalize_scripts);

  /*
  // add new player commands
  add_cmd("trun", NULL, cmd_scrun, POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
  */
  extern COMMAND(cmd_tedit); // define the command
  add_cmd("attach",  NULL, cmd_attach,  POS_UNCONCIOUS, POS_FLYING,
	  "scripter", FALSE, FALSE);
  add_cmd("detach",  NULL, cmd_detach,  POS_UNCONCIOUS, POS_FLYING,
	  "scripter", FALSE, FALSE);
  add_cmd("tedit",   NULL, cmd_tedit,   POS_UNCONCIOUS, POS_FLYING,
	  "scripter", FALSE, TRUE);
  add_cmd("tstat",   NULL, cmd_tstat,   POS_UNCONCIOUS, POS_FLYING,
	  "scripter", FALSE, FALSE);
  add_cmd("tlist",   NULL, cmd_tlist,   POS_UNCONCIOUS, POS_FLYING,
	  "scripter", FALSE, FALSE);
  add_cmd("tdelete", NULL, cmd_tdelete, POS_UNCONCIOUS, POS_FLYING,
	  "scripter",FALSE, FALSE);
  add_cmd("trename", NULL, cmd_trename, POS_UNCONCIOUS, POS_FLYING,
	  "scripter", FALSE, FALSE);
}

//
// makes a dictionary with all of the neccessary stuff in it, but without
// a __builtin__ module set
PyObject *mud_script_dict(void) {
  PyObject* dict = PyDict_New();

  // add the exit() function so people can terminate scripts
  PyObject *sys = PyImport_ImportModule("sys");
  if(sys != NULL) {
    PyObject *exit = PyDict_GetItemString(PyModule_GetDict(sys), "exit");
    if(exit != NULL) PyDict_SetItemString(dict, "exit", exit);
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
  mudmod = PyImport_ImportModule("event");
  PyDict_Update(dict, PyModule_GetDict(mudmod));
  Py_DECREF(mudmod);
  mudmod = PyImport_ImportModule("random");
  PyDict_SetItemString(dict, "random", mudmod);
  Py_DECREF(mudmod);

  return dict;
}

PyObject *restricted_script_dict(void) {
  // build up our basic dictionary
  PyObject *dict = mud_script_dict();
  
  // add restricted builtin modules
  PyObject *builtins = PyImport_ImportModule("__restricted_builtin__");
  if(builtins != NULL) {
    PyDict_SetItemString(dict, "__builtins__", builtins);
    Py_DECREF(builtins);
  }

  return dict;
}

PyObject *unrestricted_script_dict(void) {
  PyObject *dict = mud_script_dict();

  // add builtins
  PyObject *builtins = PyImport_ImportModule("__builtin__");
  if(builtins != NULL) {
    PyDict_SetItemString(dict, "__builtins__", builtins);
    Py_DECREF(builtins);
  }

  return dict;
}

void run_script(PyObject *dict, const char *script, const char *locale) {
  if(script_loop_depth >= MAX_LOOP_DEPTH)
    script_ok = FALSE;
  else {
    listPush(locale_stack, strdupsafe(locale));

    script_loop_depth++;
    PyObject* compileRetval = PyRun_String(script, Py_file_input, dict, dict);
    script_loop_depth--;
    script_ok = TRUE;
    // we threw an error and it wasn't an intentional
    // system exit error. Now print the backtrace
    if(compileRetval == NULL && PyErr_Occurred() != PyExc_SystemExit) {
      char *tb = getPythonTraceback();
      log_string("Script terminated with an error:\r\n%s\r\n"
		 "\r\nTraceback is:\r\n%s\r\n", script, tb);
      free(tb);
      script_ok = FALSE;
    }

    Py_XDECREF(compileRetval);
    free(listPop(locale_stack));
  }
}

const char *get_script_locale(void) {
  return listHead(locale_stack);
}

bool last_script_ok(void) {
  return script_ok;
}

void format_script_buffer(BUFFER *script) {
  bufferReplace(script, "\r", "", TRUE);
}

LIST *charGetTriggers(CHAR_DATA *ch) {
  TRIGGER_AUX_DATA *data = charGetAuxiliaryData(ch, "trigger_data");
  return data->triggers;
}

LIST *objGetTriggers (OBJ_DATA  *obj) {
  TRIGGER_AUX_DATA *data = objGetAuxiliaryData(obj, "trigger_data");
  return data->triggers;
}

LIST *roomGetTriggers(ROOM_DATA *room) {
  TRIGGER_AUX_DATA *data = roomGetAuxiliaryData(room, "trigger_data");
  return data->triggers;
}

void triggerListAdd(LIST *list, const char *trigger) {
  if(!listGetWith(list, trigger, strcasecmp))
    listPut(list, strdup(trigger));
}

void triggerListRemove(LIST *list, const char *trigger) {
  char *val = listRemoveWith(list, trigger, strcasecmp);
  if(val) free(val);
}


//
// statements we need to highlight when showing a script
const char *control_table[] = {
  "import",
  "return",
  "except",
  "while",
  "from",
  "elif",
  "else",
  "pass",
  "try",
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
// returns which control string we found. returns -1 if none were found
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
	send_to_socket(sock, "%s{g%s{n\r\n", line_num_info, line);
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
    send_to_socket(sock, "{c%2d]{g  %s{n\r\n", line_num, line);

  // and kill any color that is leaking
  //  send_to_socket(sock, "{n");

  // we don't end in a newline, but we have code
  if(line_num != 1 && ptr[strlen(ptr)-1] != '\n')
    send_to_socket(sock, "{RBuffer does not end in newline!{n\r\n");
}
