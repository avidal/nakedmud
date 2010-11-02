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

#include "scripts.h"
#include "pyroom.h"
#include "pychar.h"
#include "pyobj.h"
#include "pyplugs.h"



//*****************************************************************************
// local variables and functions
//*****************************************************************************
// global variables we have set.
PyObject  *globals = NULL;



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
// add a new command to the mud, via a python script or module. Takes in a
// command name, a sort_by command, the function, a minimum and maximum 
// position in the form of strings, a level, and boolean values for whether the
// command can be performed by mobiles, and whether it interrupts actions.
PyObject *mud_add_cmd(PyObject *self, PyObject *args) {
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
// format a string to be into a typical description style
PyObject *mud_format_string(PyObject *self, PyObject *args) {
  char *string = NULL;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "s", &string)) {
    PyErr_Format(PyExc_TypeError, 
		 "Can not format non-string values.");
    return NULL;
  }

  // dup the string so we can work with it and not intrude on the PyString data
  BUFFER *buf = newBuffer(MAX_BUFFER);
  bufferCat(buf, string);
  bufferFormat(buf, SCREEN_WIDTH, PARA_INDENT);
  PyObject *ret = Py_BuildValue("s", bufferString(buf));
  deleteBuffer(buf);
  return ret;
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
  if(is_keyword(type_str, "in", FALSE))
    SET_BIT(type, FIND_TYPE_IN_OBJ);

  // do the search
  int found_type = FOUND_NONE;
  void    *found = generic_find(looker, arg, type, scope, all_ok, &found_type);

  if(found_type == FOUND_CHAR)
    return Py_BuildValue("Os", newPyChar(found), "char");
  else if(found_type == FOUND_OBJ)
    return Py_BuildValue("Os", newPyObj(found), "obj");
  else if(found_type == FOUND_IN_OBJ)
    return Py_BuildValue("Os", newPyObj(found), "in");
  // now it gets a bit more tricky... we have to see what other bit was set
  else if(found_type == FOUND_LIST) {
    PyObject         *list = PyList_New(0);
    LIST_ITERATOR *found_i = newListIterator(found);
    void        *one_found = NULL;
    if(IS_SET(type, FIND_TYPE_CHAR)) {
      ITERATE_LIST(one_found, found_i)
	PyList_Append(list, newPyChar(one_found));
    }
    else if(IS_SET(type, FIND_TYPE_OBJ | FIND_TYPE_IN_OBJ)) {
      ITERATE_LIST(one_found, found_i)
	PyList_Append(list, newPyChar(one_found));
    }
    deleteListIterator(found_i);
    deleteList(found);
    return Py_BuildValue("Os", list, "list");
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



//*****************************************************************************
// MUD module
//*****************************************************************************
PyMethodDef mud_module_methods[] = {
    {"get_global",  mud_get_global, METH_VARARGS,
     "Get the value of a global variable."},
    {"set_global",  mud_set_global, METH_VARARGS,
     "Set the value of a global variable."},
    {"erase_global",  mud_erase_global, METH_VARARGS,
     "Erase the value of a global variable."},
    {"add_cmd", mud_add_cmd, METH_VARARGS,
     "Add a new command to the game."},
    {"message", mud_message, METH_VARARGS,
     "plugs into the message() function from inform.h" },
    {"format_string", mud_format_string, METH_VARARGS,
     "format a string to be 80 chars wide and indented. Like a desc."},
    {"generic_find",  mud_generic_find, METH_VARARGS,
     "Python wrapper around the generic_find() function"},
    {"extract", mud_extract, METH_VARARGS,
    "extracts an object or character from the game. This method is dangerous, "
    "since the object may still be needed in whichever function called the "
    "script that activated this method" },
    {"ite", mud_ite, METH_VARARGS,
     "A functional form of an if-then-else statement. Takes 2 arguments "
     "(condition, if action) and an optional third (else action). If no else "
     "action is specified and the condition is false, None is returned." },
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
