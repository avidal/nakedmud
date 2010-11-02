//*****************************************************************************
//
// pymud.h
//
// a python module that provides some useful utility functions for interacting
// with the MUD. Includes stuff like global variables, messaging functions,
// and a whole bunch of other stuff.
//
// WORK FOR FUTURE: Our use of Py_INCREF is probably creating a
//                  memory leak somewhere.
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../utils.h"
#include "../character.h"
#include "../inform.h"
#include "../handler.h"
#include "../parse.h"
#include "../races.h"

#include "scripts.h"
#include "pyroom.h"
#include "pychar.h"
#include "pyobj.h"
#include "pyplugs.h"
#include "pyexit.h"
#include "pysocket.h"



//*****************************************************************************
// local variables and functions
//*****************************************************************************
// global variables we have set.
PyObject  *globals = NULL;

// a list of methods to add to the mud module
LIST *pymud_methods = NULL;



//*****************************************************************************
//
// GLOBAL VARIABLES
//
// the following functions allow scriptors to store/access global variables.
// globals are stored in a python map, that maps two python objects together.
// the functions used to interact with globals are:
//   get_global(key)
//   set_global(key, val)
//   erase_global(key)
//
//*****************************************************************************
PyObject *mud_get_global(PyObject *self, PyObject *args) {
  PyObject *key = NULL;

  // get the key
  if (!PyArg_ParseTuple(args, "O", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not retrieve global variable - no key provided");
    return NULL;
  }

  PyObject *val = PyDict_GetItem(globals, key);
  if(val == NULL)
    val = Py_None;
  
  Py_INCREF(val);
  return val;
}

PyObject *mud_set_global(PyObject *self, PyObject *args) {
  PyObject *key = NULL, *val = NULL;

  if (!PyArg_ParseTuple(args, "OO", &key, &val)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not set global variable - need key and value");
    return NULL;
  }

  PyDict_SetItem(globals, key, val);
  return Py_BuildValue("i", 1);
}


PyObject *mud_erase_global(PyObject *self, PyObject *args) {
  PyObject *key = NULL;

  if (!PyArg_ParseTuple(args, "O", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not erase global variable - need key");
    return NULL;
  }

  PyDict_SetItem(globals, key, Py_None);
  return Py_BuildValue("i", 1);
}


//
// format a string to be into a typical description style
PyObject *mud_format_string(PyObject *self, PyObject *args) {
  char *string = NULL;
  bool indent  = TRUE;
  int  width   = SCREEN_WIDTH;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "s|bi", &string, &indent, &width)) {
    PyErr_Format(PyExc_TypeError, 
		 "Can not format non-string values.");
    return NULL;
  }

  // dup the string so we can work with it and not intrude on the PyString data
  BUFFER *buf = newBuffer(MAX_BUFFER);
  bufferCat(buf, string);
  bufferFormat(buf, width, (indent ? PARA_INDENT : 0));
  PyObject *ret = Py_BuildValue("s", bufferString(buf));
  deleteBuffer(buf);
  return ret;
}


//
// parses arguments for character commands
PyObject *mud_parse_args(PyObject *self, PyObject *args) {
  PyObject   *pych = NULL;
  bool show_errors = FALSE;
  char        *cmd = NULL;
  char     *pyargs = NULL;
  char     *syntax = NULL;
  char *parse_args = NULL;
  CHAR_DATA    *ch = NULL;

  // parse our arguments
  if(!PyArg_ParseTuple(args, "Obsss", &pych, &show_errors, 
		       &cmd, &pyargs, &syntax)) {
    PyErr_Format(PyExc_TypeError, "Invalid arguments to parse_args");
    return NULL;
  }

  // convert the character
  if(!PyChar_Check(pych) || (ch = PyChar_AsChar(pych)) == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "First argument must be an existent character!");
      return NULL;
  }

  // strdup our py args; they might be edited in the parse function
  parse_args = strdup(pyargs);

  // finish up and garbage collections
  PyObject *retval = Py_parse_args(ch, show_errors, cmd, parse_args, syntax);
  free(parse_args);
  return retval;
}


//
// a wrapper around NakedMud's generic_find() function
PyObject *mud_generic_find(PyObject *self, PyObject *args) {
  PyObject *py_looker = Py_None;     CHAR_DATA *looker = NULL;
  char      *type_str = NULL;        bitvector_t  type = 0; 
  char     *scope_str = NULL;        bitvector_t scope = 0;
  char           *arg = NULL;
  bool         all_ok = TRUE;

  // parse the arguments
  if(!PyArg_ParseTuple(args, "Osss|b", &py_looker, &arg, &type_str, &scope_str,
		       &all_ok)) {
    PyErr_Format(PyExc_TypeError,
		 "Invalid arguments supplied to mud.generic_find()");
    return NULL;
  }

  // convert the looker
  if(py_looker != Py_None) {
    if(!PyChar_Check(py_looker) || (looker = PyChar_AsChar(py_looker)) == NULL){
      PyErr_Format(PyExc_TypeError, 
		   "First argument must be an existent character or None!");
      return NULL;
    }
  }

  // convert the scope
  if(is_keyword(scope_str, "room", FALSE))
    SET_BIT(scope, FIND_SCOPE_ROOM);
  if(is_keyword(scope_str, "inv", FALSE))
    SET_BIT(scope, FIND_SCOPE_INV);
  if(is_keyword(scope_str, "worn", FALSE))
    SET_BIT(scope, FIND_SCOPE_WORN);
  if(is_keyword(scope_str, "world", FALSE))
    SET_BIT(scope, FIND_SCOPE_WORLD);
  if(is_keyword(scope_str, "visible", FALSE))
    SET_BIT(scope, FIND_SCOPE_VISIBLE);
  if(is_keyword(scope_str, "immediate", FALSE))
    SET_BIT(scope, FIND_SCOPE_IMMEDIATE);
  if(is_keyword(scope_str, "all", FALSE))
    SET_BIT(scope, FIND_SCOPE_ALL);

  // convert the types
  if(is_keyword(type_str, "obj", FALSE))
    SET_BIT(type, FIND_TYPE_OBJ);
  if(is_keyword(type_str, "char", FALSE))
    SET_BIT(type, FIND_TYPE_CHAR);
  if(is_keyword(type_str, "exit", FALSE))
    SET_BIT(type, FIND_TYPE_EXIT);
  if(is_keyword(type_str, "in", FALSE))
    SET_BIT(type, FIND_TYPE_IN_OBJ);
  if(is_keyword(type_str, "all", FALSE))
    SET_BIT(type,FIND_TYPE_OBJ  | FIND_TYPE_CHAR | 
	         FIND_TYPE_EXIT | FIND_TYPE_IN_OBJ);

  // do the search
  int found_type = FOUND_NONE;
  void    *found = generic_find(looker, arg, type, scope, all_ok, &found_type);

  if(found_type == FOUND_CHAR) {
    // were we searching for one type, or multiple types?
    if(!strcasecmp("char", type_str))
      return Py_BuildValue("O", charGetPyFormBorrowed(found));
    else
      return Py_BuildValue("Os", charGetPyFormBorrowed(found), "char");
  }
  else if(found_type == FOUND_EXIT) {
    // were we searching for one type, or multiple types?
    PyObject   *exit = newPyExit(found);
    PyObject *retval = NULL;
    if(!strcasecmp("exit", type_str))
      retval = Py_BuildValue("O", exit);
    else
      retval = Py_BuildValue("Os", exit, "obj");
    Py_DECREF(exit);
    return retval;
  }
  else if(found_type == FOUND_OBJ) {
    // were we searching for one type, or multiple types?
    if(!strcasecmp("obj", type_str))
      return Py_BuildValue("O", objGetPyFormBorrowed(found));
    else
      return Py_BuildValue("Os", objGetPyFormBorrowed(found), "obj");
  }
  else if(found_type == FOUND_IN_OBJ) {
    // were we searching for one type, or multiple types?
    if(!strcasecmp("in", type_str))
      return Py_BuildValue("O", objGetPyFormBorrowed(found));
    else
      return Py_BuildValue("Os", objGetPyFormBorrowed(found), "in");
  }
  // now it gets a bit more tricky... we have to see what other bit was set
  else if(found_type == FOUND_LIST) {
    PyObject         *list = PyList_New(0);
    LIST_ITERATOR *found_i = newListIterator(found);
    void        *one_found = NULL;
    if(IS_SET(type, FIND_TYPE_CHAR)) {
      ITERATE_LIST(one_found, found_i)
	PyList_Append(list, charGetPyFormBorrowed(one_found));
    }
    else if(IS_SET(type, FIND_TYPE_OBJ | FIND_TYPE_IN_OBJ)) {
      ITERATE_LIST(one_found, found_i)
	PyList_Append(list, objGetPyFormBorrowed(one_found));
    }
    deleteListIterator(found_i);
    deleteList(found);
    PyObject *retval = Py_BuildValue("Os", list, "list");
    Py_DECREF(list);
    return retval;
  }

  // nothing was found...
  return Py_BuildValue("Os", Py_None, Py_None);
}


//
// execute message() from inform.h
PyObject *mud_message(PyObject *self, PyObject *args) {
  // the python/C representations of the various variables that message() needs
  PyObject    *pych = NULL;     CHAR_DATA     *ch = NULL;
  PyObject  *pyvict = NULL;     CHAR_DATA   *vict = NULL;
  PyObject   *pyobj = NULL;     OBJ_DATA     *obj = NULL;
  PyObject  *pyvobj = NULL;     OBJ_DATA    *vobj = NULL;
  char     *pyrange = NULL;     bitvector_t range = 0;
  char        *mssg = NULL;
  int    hide_nosee = 0;

  // parse all of the arguments
  if(!PyArg_ParseTuple(args, "OOOObss", &pych, &pyvict, &pyobj, &pyvobj,
		       &hide_nosee, &pyrange, &mssg)) {
    PyErr_Format(PyExc_TypeError,"Invalid arguments supplied to mud.message()");
    return NULL;
  }

  // convert the character
  if(pych != Py_None) {
    if(!PyChar_Check(pych) || (ch = PyChar_AsChar(pych)) == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "First argument must be an existent character or None!");
      return NULL;
    }
  }

  // convert the victim
  if(pyvict != Py_None) {
    if(!PyChar_Check(pyvict) || (vict = PyChar_AsChar(pyvict)) == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "Second argument must be an existent character or None!");
      return NULL;
    }
  }

  // convert the object
  if(pyobj != Py_None) {
    if(!PyObj_Check(pyobj) || (obj = PyObj_AsObj(pyobj)) == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "Third argument must be an existent object or None!");
      return NULL;
    }
  }

  // convert the target object
  if(pyvobj != Py_None) {
    if(!PyObj_Check(pyvobj) || (vobj = PyObj_AsObj(pyvobj)) == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "Fourth argument must be an existent object or None!");
      return NULL;
    }
  }

  // check all of our keywords: char, vict, room
  if(is_keyword(pyrange, "to_char", FALSE))
    SET_BIT(range, TO_CHAR);
  if(is_keyword(pyrange, "to_vict", FALSE))
    SET_BIT(range, TO_VICT);
  if(is_keyword(pyrange, "to_room", FALSE))
    SET_BIT(range, TO_ROOM);
  if(is_keyword(pyrange, "to_world", FALSE))
    SET_BIT(range, TO_WORLD);

  // finally, send out the message
  message(ch, vict, obj, vobj, hide_nosee, range, mssg);
  return Py_BuildValue("i", 1);
}


//
// extracts an mob or object from the game
PyObject *mud_extract(PyObject *self, PyObject *args) {
  PyObject *thing = NULL;

  // parse the value
  if (!PyArg_ParseTuple(args, "O", &thing)) {
    PyErr_Format(PyExc_TypeError, 
		 "extract must be provided with an object or mob to extract!.");
    return NULL;
  }

  // check its type
  if(PyChar_Check(thing)) {
    CHAR_DATA *ch = PyChar_AsChar(thing);
    if(ch != NULL)
      extract_mobile(ch);
    else {
      PyErr_Format(PyExc_StandardError,
		   "Tried to extract nonexistent character!");
      return NULL;
    }
  }
  else if(PyObj_Check(thing)) {
    OBJ_DATA *obj = PyObj_AsObj(thing);
    if(obj != NULL)
      extract_obj(obj);
    else {
      PyErr_Format(PyExc_StandardError,
		   "Tried to extract nonexistent object!");
      return NULL;
    }
  }
  else if(PyRoom_Check(thing)) {
    ROOM_DATA *room = PyRoom_AsRoom(thing);
    if(room != NULL)
      extract_room(room);
    else {
      PyErr_Format(PyExc_StandardError,
		   "Tried to extract nonexistent room!");
      return NULL;
    }
  }
  
  // success
  return Py_BuildValue("i", 1);
}

//
// functional form of if/then/else
PyObject *mud_ite(PyObject *self, PyObject *args) {
  PyObject *condition = NULL;
  PyObject  *true_act = NULL;
  PyObject *false_act = Py_None;

  if (!PyArg_ParseTuple(args, "OO|O", &condition, &true_act, &false_act)) {
    PyErr_Format(PyExc_TypeError, "ite must be specified 2 and an optional 3rd "
		 "arg");
    return NULL;
  }

  // check to see if our condition is true
  if( (PyInt_Check(condition)    && PyInt_AsLong(condition) != 0) ||
      (PyString_Check(condition) && strlen(PyString_AsString(condition)) > 0))
    return true_act;
  else
    return false_act;
}

//
// returns whether or not two database keys have the same name. Locale sensitive
PyObject *mud_keys_equal(PyObject *self, PyObject *args) {
  char *key1 = NULL;
  char *key2 = NULL;
  if(!PyArg_ParseTuple(args, "ss", &key1, &key2)) {
    PyErr_Format(PyExc_TypeError, "keys_equal takes two string arguments");
    return NULL;
  }

  char *fullkey1 = strdup(get_fullkey_relative(key1, get_script_locale()));
  char *fullkey2 = strdup(get_fullkey_relative(key2, get_script_locale()));
  bool        ok = !strcasecmp(fullkey1, fullkey2);
  free(fullkey1);
  free(fullkey2);
  return Py_BuildValue("i", ok);
}

//
// returns the mud's message of the day
PyObject *mud_get_motd(PyObject *self, PyObject *args) {
  return Py_BuildValue("s", bufferString(motd));
}

//
// returns the mud's message of the day
PyObject *mud_get_greeting(PyObject *self, PyObject *args) {
  return Py_BuildValue("s", bufferString(greeting));
}

PyObject *mud_log_string(PyObject *self, PyObject *args) {
  char *mssg = NULL;
  if(!PyArg_ParseTuple(args, "s", &mssg)) {
    PyErr_Format(PyExc_TypeError, "a message must be supplied to log_string");
    return NULL;
  }

  // we have to strip all %'s out of this message
  BUFFER *buf = newBuffer(1);
  bufferCat(buf, mssg);
  bufferReplace(buf, "%", "%%", TRUE);
  log_string(bufferString(buf));
  deleteBuffer(buf);
  return Py_BuildValue("i", 1);
}

PyObject *mud_is_race(PyObject *self, PyObject *args) {
  char       *race = NULL;
  bool player_only = FALSE;
  if(!PyArg_ParseTuple(args, "s|b", &race, &player_only)) {
    PyErr_Format(PyExc_TypeError, "a string must be supplied");
    return NULL;
  }

  if(player_only)
    return Py_BuildValue("i", raceIsForPC(race));
  else
    return Py_BuildValue("i", isRace(race));
}

PyObject *mud_list_races(PyObject *self, PyObject *args) {
  bool player_only = FALSE;
  if(!PyArg_ParseTuple(args, "|b", &player_only)) {
    PyErr_Format(PyExc_TypeError, "true/false value to list only player races must be provided.");
    return NULL;
  }
  return Py_BuildValue("s", raceGetList(player_only));
}


//*****************************************************************************
// MUD module
//*****************************************************************************
void PyMud_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pymud_methods == NULL) pymud_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pymud_methods, def);
}


PyMODINIT_FUNC
init_PyMud(void) {
  // add all of our methods
  PyMud_addMethod("get_global", mud_get_global, METH_VARARGS,
		  "Get the value of a global variable.");
  PyMud_addMethod("set_global", mud_set_global, METH_VARARGS,
		  "Set the value of a global variable.");
  PyMud_addMethod("erase_global",  mud_erase_global, METH_VARARGS,
		  "Erase the value of a global variable.");
  PyMud_addMethod("message", mud_message, METH_VARARGS,
		  "plugs into the message() function from inform.h");
  PyMud_addMethod("format_string", mud_format_string, METH_VARARGS,
		  "format a string to be 80 chars wide and indented.");
  PyMud_addMethod("generic_find",  mud_generic_find, METH_VARARGS,
		  "Python wrapper around the generic_find() function");
  PyMud_addMethod("extract", mud_extract, METH_VARARGS,
		  "extracts an object or character from the game.");
  PyMud_addMethod("keys_equal", mud_keys_equal, METH_VARARGS,
		  "Returns whether or not two db keys are equal, given the ."
		  "locale that the script is running in.");
  PyMud_addMethod("ite", mud_ite, METH_VARARGS,
		  "A functional form of an if-then-else statement. Takes 2 "
		  "arguments (condition, if action) and an optional third "
		  "(else action). If no else action is specified and the "
		  "condition is false, None is returned.");
  PyMud_addMethod("parse_args", mud_parse_args, METH_VARARGS,
		  "equivalent to parse_args written in C");
  PyMud_addMethod("get_motd", mud_get_motd, METH_VARARGS,
		  "returns the mud's message of the day");
  PyMud_addMethod("get_greeting", mud_get_greeting, METH_VARARGS,
		  "returns the mud's login greeting");
  PyMud_addMethod("log_string", mud_log_string, METH_VARARGS,
		  "adds a string to the mudlog");
  PyMud_addMethod("is_race", mud_is_race, METH_VARARGS,
		  "returns whether or not the string is a valid race.");
  PyMud_addMethod("list_races", mud_list_races, METH_VARARGS,
		  "returns a list of all the races available. Can take one "
		  "argument that specifies whether or not to list player "
		  "races.");

  Py_InitModule3("mud", makePyMethods(pymud_methods),
		 "The mud module, for all MUD misc mud utils.");

  globals = PyDict_New();
  Py_INCREF(globals);
}
