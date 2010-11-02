//*****************************************************************************
//
// pyroom.c
//
// Contains the implementation of a module that contains all of the methods
// for dealing with rooms. Also contains a Python Class implementation of
// NakedMud rooms.
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../world.h"
#include "../room.h"
#include "../exit.h"
#include "../character.h"
#include "../handler.h"
#include "../utils.h"

#include "pyplugs.h"
#include "script_set.h"
#include "script.h"
#include "pychar.h"
#include "pyobj.h"
#include "pyroom.h"



//*****************************************************************************
// local structures and defines
//*****************************************************************************

// a list of the get/setters on the Room class
LIST *pyroom_getsetters = NULL;

// a list of the methods on the Room class
LIST *pyroom_methods = NULL;

typedef struct {
  PyObject_HEAD
  room_vnum vnum;
} PyRoom;



//*****************************************************************************
// allocation, deallocation, initialization, and comparison
//*****************************************************************************
static void
PyRoom_dealloc(PyRoom *self) {
  self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
PyRoom_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyRoom *self;
    self = (PyRoom *)type->tp_alloc(type, 0);
    self->vnum = NOWHERE;
    return (PyObject *)self;
}

static int
PyRoom_init(PyRoom *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[] = {"vnum", NULL};
  int vnum = NOWHERE;

  // get the vnum
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &vnum)) {
    PyErr_Format(PyExc_TypeError, 
                    "Rooms may only be created using a vnum");
    return -1;
  }

  // make sure a room with the vnum exists
  if(!worldGetRoom(gameworld, vnum)) {
    PyErr_Format(PyExc_TypeError, 
		 "Room with vnum, %d, does not exist", vnum);
    return -1;
  }

  self->vnum = vnum;
  return 0;
}

static int
PyRoom_compare(PyRoom *room1, PyRoom *room2) {
  if(room1->vnum == room2->vnum)
    return 0;
  else if(room1->vnum < room2->vnum)
    return -1;
  else
    return 1;
}



//*****************************************************************************
// getters and setters for the Room class
//*****************************************************************************
static PyObject *
PyRoom_getvnum(PyRoom *self, void *closure) {
  ROOM_DATA *room = worldGetRoom(gameworld, self->vnum);
  if(room != NULL) return Py_BuildValue("i", roomGetVnum(room));
  else             return NULL;
}

static PyObject *
PyRoom_getchars(PyRoom *self, PyObject *args) {
  ROOM_DATA *room = worldGetRoom(gameworld, self->vnum);
  if(room == NULL)
    return NULL;
  else {
    LIST_ITERATOR *char_i = newListIterator(roomGetCharacters(room));
    PyObject *list = PyList_New(0);
    CHAR_DATA *ch;

    // for each char in the room list, add him to a Python list
    ITERATE_LIST(ch, char_i)
      PyList_Append(list, newPyChar(ch));
    deleteListIterator(char_i);
    return Py_BuildValue("O", list);
  }
}

static PyObject *
PyRoom_getobjs(PyRoom *self, PyObject *args) {
  ROOM_DATA *room = worldGetRoom(gameworld, self->vnum);
  if(room == NULL)
    return NULL;
  else {
    LIST_ITERATOR *obj_i = newListIterator(roomGetContents(room));
    PyObject *list = PyList_New(0);
    OBJ_DATA *obj;

    // for each obj in the room list, add him to a Python list
    ITERATE_LIST(obj, obj_i)
      PyList_Append(list, newPyObj(obj));
    deleteListIterator(obj_i);
    return Py_BuildValue("O", list);
  }
}



//*****************************************************************************
// methods for the room class
//*****************************************************************************

//
// Send a newline-tagged message to everyone in the room
static PyObject *
PyRoom_send(PyRoom *self, PyObject *value) {
  char *mssg = NULL;
  if (!PyArg_ParseTuple(value, "s", &mssg)) {
    PyErr_Format(PyExc_TypeError, 
                    "Characters may only be sent strings");
    return NULL;
  }

  ROOM_DATA *room = worldGetRoom(gameworld, self->vnum);
  if(room) {
    send_to_list(roomGetCharacters(room), "%s\r\n", mssg);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
                    "Tried to send message to nonexistant room, %d.", 
		    self->vnum);
    return NULL;
  }
}


//
// close a door in the specified direction
static PyObject *
PyRoom_close(PyRoom *self, PyObject *value) {
  ROOM_DATA *room = NULL;
  EXIT_DATA *exit = NULL;
  char *dirname = NULL;
  int dir = DIR_NONE;

  if (!PyArg_ParseTuple(value, "s", &dirname)) {
    PyErr_Format(PyExc_TypeError, 
                    "Doornames provided to PyRoom_close must be directions.");
    return NULL;
  }

  room = worldGetRoom(gameworld, self->vnum);
  if(room == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to close door in non-existant room, %d.", 
		 self->vnum);
    return NULL;
  }

  // see if it's a normal exit
  dir = dirGetNum(dirname);

  if(dir != DIR_NONE)
    exit = roomGetExit(room, dir);
  else
    exit = roomGetExitSpecial(room, dirname);

  // make sure the exit exists
  if(exit == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to close non-existant exit, %s, in room %d.",
		 dirname, self->vnum);
    return NULL;
  }

  // make sure the exit can be closed in the first place
  if(!exitIsClosable(exit)) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to close exit, %s, in room %d that is not closable.",
		 dirname, self->vnum);
    return NULL;
  }

  exitSetClosed(exit, TRUE);
  return Py_BuildValue("i", 1);
}


//
// lock a door in the specified direction
static PyObject *
PyRoom_lock(PyRoom *self, PyObject *value) {
  ROOM_DATA *room = NULL;
  EXIT_DATA *exit = NULL;
  char *dirname = NULL;
  int dir = DIR_NONE;

  if (!PyArg_ParseTuple(value, "s", &dirname)) {
    PyErr_Format(PyExc_TypeError, 
                    "Doornames provided to PyRoom_lock must be directions.");
    return NULL;
  }

  room = worldGetRoom(gameworld, self->vnum);
  if(room == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to lock door in non-existant room, %d.", 
		 self->vnum);
    return NULL;
  }

  // see if it's a normal exit
  dir = dirGetNum(dirname);

  if(dir != DIR_NONE)
    exit = roomGetExit(room, dir);
  else
    exit = roomGetExitSpecial(room, dirname);

  // make sure the exit exists
  if(exit == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to lock non-existant exit, %s, in room %d.",
		 dirname, self->vnum);
    return NULL;
  }

  // make sure the exit can be closed in the first place
  if(!exitIsClosable(exit)) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to lock exit, %s, in room %d that is not closable.",
		 dirname, self->vnum);
    return NULL;
  }
  if(exitGetKey(exit) == NOTHING) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to lock exit, %s, in room %d that is not lockable.",
		 dirname, self->vnum);
    return NULL;
  }

  exitSetClosed(exit, TRUE);
  exitSetLocked(exit, TRUE);
  return Py_BuildValue("i", 1);
}


//
// lock a door in the specified direction
static PyObject *
PyRoom_unlock(PyRoom *self, PyObject *value) {
  ROOM_DATA *room = NULL;
  EXIT_DATA *exit = NULL;
  char *dirname = NULL;
  int dir = DIR_NONE;

  if (!PyArg_ParseTuple(value, "s", &dirname)) {
    PyErr_Format(PyExc_TypeError, 
                    "Doornames provided to PyRoom_unlock must be directions.");
    return NULL;
  }

  room = worldGetRoom(gameworld, self->vnum);
  if(room == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to unlock door in non-existant room, %d.", 
		 self->vnum);
    return NULL;
  }

  // see if it's a normal exit
  dir = dirGetNum(dirname);

  if(dir != DIR_NONE)
    exit = roomGetExit(room, dir);
  else
    exit = roomGetExitSpecial(room, dirname);

  // make sure the exit exists
  if(exit == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to unlock non-existant exit, %s, in room %d.",
		 dirname, self->vnum);
    return NULL;
  }

  // make sure the exit can be closed in the first place
  if(!exitIsClosable(exit)) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to unlock exit, %s, in room %d that is not closable.",
		 dirname, self->vnum);
    return NULL;
  }
  if(exitGetKey(exit) == NOTHING) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to unlock exit, %s, in room %d that is not lockable.",
		 dirname, self->vnum);
    return NULL;
  }

  exitSetLocked(exit, FALSE);
  return Py_BuildValue("i", 1);
}


//
// close a door in the specified direction
static PyObject *
PyRoom_open(PyRoom *self, PyObject *value) {
  ROOM_DATA *room = NULL;
  EXIT_DATA *exit = NULL;
  char *dirname = NULL;
  int dir = DIR_NONE;

  if (!PyArg_ParseTuple(value, "s", &dirname)) {
    PyErr_Format(PyExc_TypeError, 
                    "Doornames provided to PyRoom_open must be directions.");
    return NULL;
  }

  room = worldGetRoom(gameworld, self->vnum);
  if(room == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to open door in non-existant room, %d.", 
		 self->vnum);
    return NULL;
  }


  // see if it's a normal exit
  dir = dirGetNum(dirname);

  if(dir != DIR_NONE)
    exit = roomGetExit(room, dir);
  else
    exit = roomGetExitSpecial(room, dirname);

  // make sure the exit exists
  if(exit == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to open non-existant exit, %s, in room %d.",
		 dirname, self->vnum);
    return NULL;
  }

  exitSetClosed(exit, FALSE);
  exitSetLocked(exit, FALSE);
  return Py_BuildValue("i", 1);
}


static PyObject *
PyRoom_attach(PyRoom *self, PyObject *args) {  
  long vnum = NOTHING;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "i", &vnum)) {
    PyErr_Format(PyExc_TypeError, 
		 "To attach a script, the vnum must be supplied.");
    return NULL;
  }

  // pull out the character and do the attaching
  ROOM_DATA     *room = worldGetRoom(gameworld, self->vnum);
  SCRIPT_DATA *script = worldGetScript(gameworld, vnum);
  if(room != NULL && script != NULL) {
    scriptSetAdd(roomGetScripts(room), vnum);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to attach script to nonexistant room, %d, or script %d "
		 "does not exit.", self->vnum, (int)vnum);
    return NULL;
  }
}


static PyObject *
PyRoom_detach(PyRoom *self, PyObject *args) {  
  long vnum = NOTHING;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "i", &vnum)) {
    PyErr_Format(PyExc_TypeError, 
		 "To detach a script, the vnum must be suppplied.");
    return NULL;
  }

  // pull out the character and do the attaching
  ROOM_DATA     *room = worldGetRoom(gameworld, self->vnum);
  SCRIPT_DATA *script = worldGetScript(gameworld, (int)vnum);
  if(room != NULL && script != NULL) {
    scriptSetRemove(roomGetScripts(room), vnum);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to detach script from nonexistant room, %d, or script "
		 "%d does not exit.", self->vnum, (int)vnum);
    return NULL;
  }
}



//*****************************************************************************
// structures to define our methods and classes
//*****************************************************************************
static PyTypeObject PyRoom_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "room.Room",               /*tp_name*/
    sizeof(PyRoom),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyRoom_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    (cmpfunc)PyRoom_compare,   /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Python Room object",      /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */ 
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PyRoom_init,     /* tp_init */
    0,                         /* tp_alloc */
    PyRoom_new,                /* tp_new */
};

static PyMethodDef room_module_methods[] = {
  {NULL, NULL, 0, NULL}  /* Sentinel */
};



//*****************************************************************************
// implementation of pyroom.h
//*****************************************************************************

void PyRoom_addGetSetter(const char *name, void *g, void *s, const char *doc) {
  // make sure our list of get/setters is created
  if(pyroom_getsetters == NULL) pyroom_getsetters = newList();

  // make the GetSetter def
  PyGetSetDef *def = calloc(1, sizeof(PyGetSetDef));
  def->name        = strdup(name);
  def->get         = (getter)g;
  def->set         = (setter)s;
  def->doc         = (doc ? strdup(doc) : NULL);
  def->closure     = NULL;
  listPut(pyroom_getsetters, def);
}


void PyRoom_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pyroom_methods == NULL) pyroom_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pyroom_methods, def);
}


// initialize rooms for use. This must be called AFTER 
PyMODINIT_FUNC
init_PyRoom(void) {
    PyObject* module = NULL;

    // add all of the basic getsetters
    PyRoom_addGetSetter("vnum",  PyRoom_getvnum,   NULL, "The room's vnum");
    PyRoom_addGetSetter("chars", PyRoom_getchars,  NULL, "chars in the room");
    PyRoom_addGetSetter("objs",  PyRoom_getobjs,   NULL, "objects in the room");
    PyRoom_addGetSetter("contents",PyRoom_getobjs, NULL, "objects in the room");

    // add all of the basic methods
    PyRoom_addMethod("attach", PyRoom_attach, METH_VARARGS,
		     "attach a new script to the room.");
    PyRoom_addMethod("detach", PyRoom_detach, METH_VARARGS,
		     "detach a script from the room, by vnum.");
    PyRoom_addMethod("close", PyRoom_close, METH_VARARGS,
		     "close a door in the specified direction.");
    PyRoom_addMethod("open", PyRoom_open, METH_VARARGS,
		     "open a door in the specified direction. Also unlocks.");
    PyRoom_addMethod("lock", PyRoom_lock, METH_VARARGS,
		     "lock a door in the specified direction. Also closes.");
    PyRoom_addMethod("unlock", PyRoom_unlock, METH_VARARGS,
		     "unlocks a door in the specified direction.");
    PyRoom_addMethod("send", PyRoom_send, METH_VARARGS,
		     "send a message to everyone in the room.");

    // add in all the getsetters and methods
    makePyType(&PyRoom_Type, pyroom_getsetters, pyroom_methods);
    deleteListWith(pyroom_getsetters, free); pyroom_getsetters = NULL;
    deleteListWith(pyroom_methods,    free); pyroom_methods    = NULL;

    // make sure the room class is ready to be made
    if (PyType_Ready(&PyRoom_Type) < 0)
        return;

    // initialize the module
    module = Py_InitModule3("room", room_module_methods,
			    "The room module, for all MUD room-related stuff.");

    // make sure the module parsed OK
    if (module == NULL)
      return;

    // add the Room class to the room module
    PyModule_AddObject(module, "Room", (PyObject *)&PyRoom_Type);
    Py_INCREF(&PyRoom_Type);
}


int PyRoom_AsVnum(PyObject *room) {
  return ((PyRoom *)room)->vnum;
}

ROOM_DATA *PyRoom_AsRoom(PyObject *room) {
  return worldGetRoom(gameworld, PyRoom_AsVnum(room));
}

int PyRoom_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PyRoom_Type);
}

PyObject *
newPyRoom(ROOM_DATA *room) {
  PyRoom *py_room = (PyRoom *)PyRoom_new(&PyRoom_Type, NULL, NULL);
  py_room->vnum = roomGetVnum(room);
  return (PyObject *)py_room;
}
