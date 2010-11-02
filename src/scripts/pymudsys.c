//*****************************************************************************
//
// pymudsys.c
//
// A set of system level commands and variables that may be needed by python,
// but which are not neccessarily needed by scripts.
//
//*****************************************************************************
#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../utils.h"
#include "../character.h"
#include "../socket.h"
#include "../save.h"

#include "pymudsys.h"
#include "scripts.h"
#include "pyplugs.h"
#include "pychar.h"



//*****************************************************************************
// local variables and functions
//*****************************************************************************
// a list of methods to add to the mudsys module
LIST *pymudsys_methods = NULL;



//*****************************************************************************
// mudsys methods
//*****************************************************************************
PyObject *mudsys_set_sys_val(PyObject *self, PyObject *args) {
  char *key, *val;
  if(!PyArg_ParseTuple(args, "ss", &key, &val)) {
    PyErr_Format(PyExc_TypeError, "Provide a string key and value");
    return NULL;
  }

  mudsettingSetString(key, val);
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_get_sys_val(PyObject *self, PyObject *args) {
  char *key;
  if(!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, "Provide a string key");
    return NULL;
  }

  return Py_BuildValue("s", mudsettingGetString(key));
}

PyObject *mudsys_shutdown(PyObject *self, PyObject *args) {
  shut_down = TRUE;
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_copyover(PyObject *self, PyObject *args) {
  do_copyover();
  return Py_BuildValue("i", 1);
}

//
// saves a player to disk
PyObject *mudsys_do_save(PyObject *self, PyObject *args) {
  PyObject *pych = NULL;
  CHAR_DATA  *ch = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError, "A character to be saved must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may be saved.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to save nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }

  if(!charIsNPC(ch))
    save_player(ch);
  return Py_BuildValue("i", 1);
}


//
// quits a character from the game
PyObject *mudsys_do_quit(PyObject *self, PyObject *args) {
  PyObject *pych = NULL;
  CHAR_DATA  *ch = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError,"A character to be quitted must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may be quitted.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to quit nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }

  if(!charIsNPC(ch) && charGetSocket(ch)) {
    SOCKET_DATA *sock = charGetSocket(ch);
    charSetSocket(ch, NULL);
    socketSetChar(sock, NULL);
    socketPopInputHandler(sock);
    extract_mobile(ch);
  }

  return Py_BuildValue("i", 1);
}


//
// disconnects a character from its socket
PyObject *mudsys_do_disconnect(PyObject *self, PyObject *args) {
  PyObject *pych = NULL;
  CHAR_DATA  *ch = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError,"A character to be dc'd must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may be dc'd.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to dc nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }

  if(charGetSocket(ch)) {
    SOCKET_DATA *sock = charGetSocket(ch);
    charSetSocket(ch, NULL);
    socketSetChar(sock, NULL);
    close_socket(sock, FALSE);
  }

  return Py_BuildValue("i", 1);
}


//
// add a new command to the mud, via a python script or module. Takes in a
// command name, a sort_by command, the function, a minimum and maximum 
// position in the form of strings, a level, and boolean values for whether the
// command can be performed by mobiles, and whether it interrupts actions.
PyObject *mudsys_add_cmd(PyObject *self, PyObject *args) {
  PyObject *func = NULL;
  char *name  = NULL, *sort_by = NULL, *min_pos = NULL, *max_pos = NULL,
       *group = NULL;
  bool mob_ok = FALSE, interrupts = FALSE;
  int min_pos_num, max_pos_num;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "szOsssbb", &name, &sort_by, &func,
  			&min_pos, &max_pos, &group, &mob_ok, &interrupts)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not add new command. Improper arguments supplied");
    return NULL;
  }

  // get our positions
  min_pos_num = posGetNum(min_pos);
  max_pos_num = posGetNum(max_pos);
  if(min_pos_num == POS_NONE || max_pos_num == POS_NONE) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not add new command. Invalid position names.");
    return NULL;
  }

  // add the command to the game
  add_py_cmd(name, sort_by, func, min_pos_num, max_pos_num,
	     group, mob_ok, interrupts);
  return Py_None;
}


//
// removes a command from the game
PyObject *mudsys_remove_cmd(PyObject *self, PyObject *args) {
  char *name = NULL;
  // parse all of the values
  if (!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "function requires string argument.");
    return NULL;
  }
  remove_cmd(name);
  return Py_None;
}



//*****************************************************************************
// MudSys module
//*****************************************************************************
void PyMudSys_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pymudsys_methods == NULL) pymudsys_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pymudsys_methods, def);
}


PyMODINIT_FUNC
init_PyMudSys(void) {
  // add all of our methods
  PyMudSys_addMethod("do_shutdown", mudsys_shutdown, METH_VARARGS,
		     "shuts the mud down.");
  PyMudSys_addMethod("do_copyover", mudsys_copyover, METH_VARARGS,
		     "performs a copyover on the mud.");
  PyMudSys_addMethod("sys_setval", mudsys_set_sys_val, METH_VARARGS,
		     "sets a system value on the mud.");
  PyMudSys_addMethod("sys_getval", mudsys_get_sys_val, METH_VARARGS,
		     "returns a system value on the mud.");
  PyMudSys_addMethod("do_save", mudsys_do_save, METH_VARARGS,
		     "save a character to disk");
  PyMudSys_addMethod("do_quit", mudsys_do_quit, METH_VARARGS,
		     "quit a character from game");
  PyMudSys_addMethod("do_disconnect", mudsys_do_disconnect, METH_VARARGS,
		     "disconnects a character from its socket");
  PyMudSys_addMethod("add_cmd", mudsys_add_cmd, METH_VARARGS,
		     "Add a new command to the game.");
  PyMudSys_addMethod("remove_cmd", mudsys_remove_cmd, METH_VARARGS,
		     "Removes a command from the game.");

  Py_InitModule3("mudsys", makePyMethods(pymudsys_methods),
		 "The mudsys module, for all MUD system utils.");
}
