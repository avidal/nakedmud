//*****************************************************************************
//
// pyroom.c
//
// A python extention to allow python scripts to treat MUD characters as an
// object within the script.
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../world.h"
#include "../room.h"
#include "../exit.h"
#include "../character.h"
#include "../races.h"
#include "../handler.h"
#include "../utils.h"

#include "script.h"
#include "pyroom.h"
#include "pychar.h"
#include "pyobj.h"

typedef struct {
  PyObject_HEAD
  room_vnum vnum;
} PyRoom;


//*****************************************************************************
//
// allocation, deallocation, and initialiation
//
//*****************************************************************************
static void
PyRoom_dealloc(PyRoom *self) {
  self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
PyRoom_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyRoom *self;

    self = (PyRoom  *)type->tp_alloc(type, 0);
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
		 "Rom with vnum, %d, does not exist", vnum);
    return -1;
  }

  self->vnum = vnum;
  return 0;
}



//*****************************************************************************
//
// methods and stuff for building the class
//
//*****************************************************************************

//
// close a door in the specified direction
//
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
//
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
//
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
//
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



static PyMethodDef PyRoom_methods[] = {
    {"close", (PyCFunction)PyRoom_close, METH_VARARGS,
     "close a door in the specified direction." },
    {"open", (PyCFunction)PyRoom_open, METH_VARARGS,
     "open a door in the specified direction. Unlocks it if neccessary." },
    {"lock", (PyCFunction)PyRoom_lock, METH_VARARGS,
     "lock a door in the specified direction, closing it if it is open." },
    {"unlock", (PyCFunction)PyRoom_unlock, METH_VARARGS,
     "unlocks the door in the specified direction." },
    {NULL}  /* Sentinel */
};


//*****************************************************************************
//
// character attributes - mostly get and set
//
//*****************************************************************************
static PyObject *
PyRoom_getvnum(PyRoom *self, void *closure) {
  ROOM_DATA *room = worldGetRoom(gameworld, self->vnum);
  if(room != NULL) return Py_BuildValue("i", roomGetVnum(room));
  else             return NULL;
}

static PyObject *
PyRoom_getcharacters(PyRoom *self, PyObject *args) {
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
PyRoom_getcontents(PyRoom *self, PyObject *args) {
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


static PyGetSetDef PyRoom_getseters[] = {
  {"vnum", (getter)PyRoom_getvnum, (setter)NULL,
   "The room's vnum", NULL},
  {"chars", (getter)PyRoom_getcharacters, (setter)NULL,
   "The characters in the room", NULL},
  {"objs", (getter)PyRoom_getcontents, (setter)NULL,
   "The objects in the room", NULL},
  {"contents", (getter)PyRoom_getcontents, (setter)NULL,
   "The objects in the room", NULL},
  {NULL}  /* Sentinel */
};



//*****************************************************************************
//
// comparators, getattr, setattr, and all that other class stuff
//
//*****************************************************************************

//
// compare one character to another
//
static int
PyRoom_compare(PyRoom *room1, PyRoom *room2) {
  if(room1->vnum == room2->vnum)
    return 0;
  else if(room1->vnum < room2->vnum)
    return -1;
  else
    return 1;
}

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
    PyRoom_methods,            /* tp_methods */
    0,                         /* tp_members */
    PyRoom_getseters,          /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PyRoom_init,    /* tp_init */
    0,                         /* tp_alloc */
    PyRoom_new,               /* tp_new */
};



//*****************************************************************************
//
// the room module
//
//*****************************************************************************
static PyMethodDef room_module_methods[] = {
  {NULL, NULL, 0, NULL}  /* Sentinel */
};


PyMODINIT_FUNC
init_PyRoom(void) {
    PyObject* m;

    if (PyType_Ready(&PyRoom_Type) < 0)
        return;

    m = Py_InitModule3("room", room_module_methods,
                       "The room module, for all MUD room-related stuff.");

    if (m == NULL)
      return;

    Py_INCREF(&PyRoom_Type);
    PyModule_AddObject(m, "Room", (PyObject *)&PyRoom_Type);
}

int PyRoom_AsVnum(PyObject *room) {
  return ((PyRoom *)room)->vnum;
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
