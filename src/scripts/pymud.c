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

#include "script.h"
#include "pyroom.h"
#include "pychar.h"
#include "pyobj.h"

PyObject *globals = NULL;


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

  //  Py_INCREF(val);
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

  //  Py_INCREF(Py_None);
  PyDict_SetItem(globals, key, Py_None);
  return Py_BuildValue("i", 1);
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
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


PyMODINIT_FUNC
init_PyMud(void) {
  PyObject *m;

  globals = PyDict_New();
  Py_INCREF(globals);

  m = Py_InitModule3("mud", mud_module_methods,
		     "The mud module, for all MUD misc mud utils.");
}
