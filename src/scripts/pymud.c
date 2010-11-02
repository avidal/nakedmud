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
#include "../character.h"

#include "script.h"
#include "pyroom.h"
#include "pychar.h"
#include "pyobj.h"
#include "pyplugs.h"



//*****************************************************************************
// local variables and functions
//*****************************************************************************
// global variables we have set.
PyObject  *globals = NULL;

// Python commands that have been added to the game
HASHTABLE *py_cmds = NULL;


//
// This is the glue between python commands and normal mud commands; checks for
// the command entry in py_cmds, and calls the appropriate function
COMMAND(cmd_py_cmd) {
  PyObject *func = hashGet(py_cmds, cmd);
  if(func == NULL)
    log_string("ERROR: Tried python command, %s, but it does not exist!\r\n", 
	       cmd);
  else {
    PyObject *arglist = Py_BuildValue("Osis", newPyChar(ch), cmd, subcmd, arg);
    PyObject *retval  = PyEval_CallObject(func, arglist);
    // check for an error:
    if(retval == NULL) {
      char *tb = getPythonTraceback();
      if(tb != NULL) {
	log_string("Error running python command, %s:\r\n%s\r\n", cmd, tb);
	free(tb);
      }
    }
    Py_XDECREF(retval);
    Py_XDECREF(arglist);
  }
}




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
static PyObject *
mud_get_global(PyObject *self, PyObject *args) {
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

static PyObject *
mud_set_global(PyObject *self, PyObject *args) {
  PyObject *key = NULL, *val = NULL;

  if (!PyArg_ParseTuple(args, "OO", &key, &val)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not set global variable - need key and value");
    return NULL;
  }

  PyDict_SetItem(globals, key, val);
  return Py_BuildValue("i", 1);
}


static PyObject *
mud_erase_global(PyObject *self, PyObject *args) {
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
// add a new command to the mud, via a python script or module. Takes in a
// command name, a sort_by command, the function, a subcmd int value, a minimum
// and maximum position in the form of strings, a level, and boolean values
// for whether the command can be performed by mobiles, and whether it 
// interrupts actions.
static PyObject *
mud_add_cmd(PyObject *self, PyObject *args) {
  PyObject *func = NULL;
  char *name  = NULL, *sort_by = NULL, *min_pos = NULL, *max_pos = NULL,
       *group = NULL;
  int  subcmd = 0;
  bool mob_ok = FALSE, interrupts = FALSE;
  int min_pos_num, max_pos_num;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "szOisssbb", &name, &sort_by, &func, &subcmd,
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

  // map our python function to its command name 
  Py_INCREF(func);
  hashPut(py_cmds, name, func);
  add_cmd(name, sort_by, cmd_py_cmd, subcmd, min_pos_num, max_pos_num,
	  group, mob_ok, interrupts);
  return Py_None;
}




//*****************************************************************************
//
// MUD module
//
//*****************************************************************************
static PyMethodDef mud_module_methods[] = {
    {"get_global",  mud_get_global, METH_VARARGS,
     "Get the value of a global variable."},
    {"set_global",  mud_set_global, METH_VARARGS,
     "Set the value of a global variable."},
    {"erase_global",  mud_erase_global, METH_VARARGS,
     "Erase the value of a global variable."},
    {"add_cmd", mud_add_cmd, METH_VARARGS,
     "Add a new command to the game,"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


PyMODINIT_FUNC
init_PyMud(void) {
  PyObject *m;

  globals = PyDict_New();
  Py_INCREF(globals);

  m = Py_InitModule3("mud", mud_module_methods,
		     "The mud module, for all MUD misc mud utils.");

  py_cmds = newHashtable();
}
