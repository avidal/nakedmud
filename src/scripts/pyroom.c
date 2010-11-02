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
#include "../utils.h"
#include "../world.h"
#include "../room.h"
#include "../exit.h"
#include "../extra_descs.h"
#include "../character.h"
#include "../handler.h"
#include "../prototype.h"
#include "../commands.h"

#include "pyplugs.h"
#include "scripts.h"
#include "pychar.h"
#include "pyobj.h"
#include "pyexit.h"
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
  int uid;
} PyRoom;



//*****************************************************************************
// allocation, deallocation, initialization, and comparison
//*****************************************************************************
void PyRoom_dealloc(PyRoom *self) {
  self->ob_type->tp_free((PyObject*)self);
}

PyObject *PyRoom_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyRoom *self;
    self = (PyRoom *)type->tp_alloc(type, 0);
    self->uid = NOWHERE;
    return (PyObject *)self;
}

int PyRoom_init(PyRoom *self, PyObject *args, PyObject *kwds) {
  char *kwlist[] = {"uid", NULL};
  int        uid = NOTHING;

  // get the vnum
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &uid)) {
    PyErr_Format(PyExc_TypeError, "Rooms may only be created using a uid");
    return -1;
  }

  // make sure a room with the vnum exists
  if(!propertyTableGet(room_table, uid)) {
    PyErr_Format(PyExc_TypeError, 
		 "Room with uid, %d, does not exist", uid);
    return -1;
  }

  self->uid = uid;
  return 0;
}

int PyRoom_compare(PyRoom *room1, PyRoom *room2) {
  if(room1->uid == room2->uid)
    return 0;
  else if(room1->uid < room2->uid)
    return -1;
  else
    return 1;
}



//*****************************************************************************
// getters and setters for the Room class
//*****************************************************************************
PyObject *PyRoom_getclass(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) return Py_BuildValue("s", roomGetClass(room));
  else             return NULL;
}

PyObject *PyRoom_getuid(PyRoom *self, void *closure) {
  return Py_BuildValue("i", self->uid);
}

PyObject *PyRoom_getname(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL)  return Py_BuildValue("s", roomGetName(room));
  else              return NULL;
}

PyObject *PyRoom_getterrain(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL)  
    return Py_BuildValue("s", terrainGetName(roomGetTerrain(room)));
  else
    return NULL;
}

PyObject *PyRoom_getdesc(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL)  return Py_BuildValue("s", roomGetDesc(room));
  else              return NULL;
}

PyObject *PyRoom_getexnames(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL)  return NULL;

  PyObject      *list = PyList_New(0);
  LIST       *ex_list = roomGetExitNames(room);
  LIST_ITERATOR *ex_i = newListIterator(ex_list);
  char           *dir = NULL;
  ITERATE_LIST(dir, ex_i) {
    PyList_Append(list, Py_BuildValue("s", dir));
  } deleteListIterator(ex_i);
  deleteListWith(ex_list, free);
  return list;
}

PyObject *PyRoom_getchars(PyRoom *self, PyObject *args) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
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

PyObject *PyRoom_getobjs(PyRoom *self, PyObject *args) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
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


//
// Standard check to make sure the room exists when trying to set a value for 
// it. If successful, assign the room to rm. Otherwise, return -1 (error)
#define PYROOM_CHECK_ROOM_EXISTS(uid, room)			  	\
  room = propertyTableGet(room_table, uid);				\
  if(room == NULL) {							\
    PyErr_Format(PyExc_TypeError,					\
		 "Tried to modify nonexistent room, %d", uid);		\
    return -1;                                                          \
  }                                                                            

int PyRoom_setname(PyRoom *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete room's name");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Room names must be strings");
    return -1;
  }

  ROOM_DATA *room;
  PYROOM_CHECK_ROOM_EXISTS(self->uid, room);
  roomSetName(room, PyString_AsString(value));
  return 0;
}

int PyRoom_setdesc(PyRoom *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete room's desc");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Room descs must be strings");
    return -1;
  }

  ROOM_DATA *room;
  PYROOM_CHECK_ROOM_EXISTS(self->uid, room);
  roomSetDesc(room, PyString_AsString(value));
  return 0;
}

int PyRoom_setterrain(PyRoom *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete room's terrain");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Room terrain type must be a string");
    return -1;
  }
  
  if(terrainGetNum(PyString_AsString(value)) == TERRAIN_NONE) {
    PyErr_Format(PyExc_TypeError, "Invalid terrain type, %s", 
		 PyString_AsString(value));
    return -1;
  }


  ROOM_DATA *room;
  PYROOM_CHECK_ROOM_EXISTS(self->uid, room);
  roomSetTerrain(room, terrainGetNum(PyString_AsString(value)));
  return 0;
}



//*****************************************************************************
// methods for the room class
//*****************************************************************************

//
// Send a newline-tagged message to everyone in the room
PyObject *PyRoom_send(PyRoom *self, PyObject *value) {
  char *mssg = NULL;
  if (!PyArg_ParseTuple(value, "s", &mssg)) {
    PyErr_Format(PyExc_TypeError, 
                    "Characters may only be sent strings");
    return NULL;
  }

  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room) {
    send_to_list(roomGetCharacters(room), "%s\r\n", mssg);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
                    "Tried to send message to nonexistant room, %d.", 
		    self->uid);
    return NULL;
  }
}


//
// create a new extra description for the room
PyObject *PyRoom_edesc(PyRoom *self, PyObject *value) {
  char *keywords = NULL;
  char     *desc = NULL;

  if (!PyArg_ParseTuple(value, "ss", &keywords, &desc)) {
    PyErr_Format(PyExc_TypeError, "Extra descs must be strings.");
    return NULL;
  }

  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) {
    EDESC_DATA *edesc = newEdesc(keywords, desc);
    edescSetPut(roomGetEdescs(room), edesc);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError,
		 "Tried to set edesc for nonexistent room, %d.", self->uid);
    return NULL;
  }
}


//
// Get an exit in the room by its direction name
PyObject *PyRoom_get_exit(PyRoom *self, PyObject *value) {
  ROOM_DATA *room = NULL;
  char       *dir = NULL;

  if (!PyArg_ParseTuple(value, "s", &dir)) {
    PyErr_Format(PyExc_TypeError, "Direction of exit not supplied.");
    return NULL;
  }

  if((room = PyRoom_AsRoom((PyObject *)self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to get exit of nonexistent room, %d.", 
		 self->uid);
    return NULL;
  }

  // get the exit
  const char *cdir = dir;
  if(dirGetAbbrevNum(dir) != DIR_NONE)
    cdir = dirGetName(dirGetAbbrevNum(dir));

  EXIT_DATA *exit = roomGetExit(room, cdir);
  if(exit != NULL)
    return Py_BuildValue("O", newPyExit(exit));
  else
    return Py_None;
}


//
// Fills an exit in the given direction
PyObject *PyRoom_fill(PyRoom *self, PyObject *value) {
  ROOM_DATA *room = NULL;
  char       *dir = NULL;

  if (!PyArg_ParseTuple(value, "s", &dir)) {
    PyErr_Format(PyExc_TypeError, "Direction not supplied to fill.");
    return NULL;
  }

  if((room = PyRoom_AsRoom((PyObject *)self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to fill in non-existant room, %d.", 
		 self->uid);
    return NULL;
  }

  // remove the exit
  EXIT_DATA *exit = roomRemoveExit(room, dir);
  if(exit != NULL) {
    exit_from_game(exit);
    deleteExit(exit);

    // is it a special exit? If so, we may need to remove the command as well
    if(dirGetNum(dir) == DIR_NONE) {
      CMD_DATA *cmd = nearMapRemove(roomGetCmdTable(room), dir);
      if(cmd != NULL)
	deleteCmd(cmd);
    }
  }

  return Py_BuildValue("i", 1);
}


//
// Links a room to another room, in the specified direction
PyObject *PyRoom_dig(PyRoom *self, PyObject *value) {
  ROOM_DATA   *room = NULL;
  PyObject *py_dest = NULL;
  char         *dir = NULL;
  const char  *cdir = NULL;
  const char  *dest = NULL;

  if (!PyArg_ParseTuple(value, "sO", &dir, &py_dest)) {
    PyErr_Format(PyExc_TypeError, 
		 "When digging, a direction and destination are needed.");
    return NULL;
  }

  if((room = PyRoom_AsRoom((PyObject *)self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to dig in non-existant room, %d.", 
		 self->uid);
    return NULL;
  }

  // make sure we have a valid destination
  if(PyString_Check(py_dest))
    dest = get_fullkey(PyString_AsString(py_dest), get_script_locale());
  else if(PyRoom_Check(py_dest)) {
    ROOM_DATA *to_room = PyRoom_AsRoom(py_dest);
    if(to_room != NULL)
      dest = roomGetClass(to_room);
    else {
      PyErr_Format(PyExc_StandardError, 
		   "Tried to dig from %s to invalid room, %d",
		   roomGetClass(room), PyRoom_AsUid(py_dest));
      return NULL;
    }
  }
  else {
    PyErr_Format(PyExc_TypeError, "Invalid destination type in room, %s.",
		 roomGetClass(room));
    return NULL;
  }

  // are we using a special direction name?
  int dir_num        = dirGetNum(dir);
  int dir_abbrev_num = dirGetAbbrevNum(dir);
  if(dir_abbrev_num != DIR_NONE)
    cdir = dirGetName(dir_abbrev_num);
  else
    cdir = dir;

  // do we already have an exit?
  EXIT_DATA *exit = roomGetExit(room, cdir);
  if(exit != NULL) {
    exitSetTo(exit, dest);
  }
  else {
    exit = newExit();
    exit_to_game(exit);
    exitSetTo(exit, dest);
    roomSetExit(room, cdir, exit);

    // if we're digging a special exit, add a cmd for it to the room cmd table
    if(dir_num == DIR_NONE && dir_abbrev_num == DIR_NONE)
      nearMapPut(roomGetCmdTable(room), cdir, NULL,
		 newCmd(cdir, cmd_move, POS_STANDING, POS_FLYING, 
			"player", TRUE, TRUE));
  }

  return Py_BuildValue("O", newPyExit(exit));
}


PyObject *PyRoom_attach(PyRoom *self, PyObject *args) {  
  char *key = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "To attach a script, the key must be supplied.");
    return NULL;
  }

  // pull out the room and do the attaching
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach script to nonexistant room, %d.", self->uid);
    return NULL;
  }

  TRIGGER_DATA *trig =
    worldGetType(gameworld, "trigger", 
		 get_fullkey_relative(key, get_script_locale()));
  if(trig != NULL) {
    triggerListAdd(roomGetTriggers(room), triggerGetKey(trig));
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to attach nonexistant script, %s, to room %s.",
		 key, roomGetClass(room));
    return NULL;
  }
}


PyObject *PyRoom_detach(PyRoom *self, PyObject *args) {  
  char *key = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "To detach a script, the key must be suppplied.");
    return NULL;
  }

  // pull out the room and do the attaching
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) {
    const char *fkey = get_fullkey_relative(key, get_script_locale());
    triggerListRemove(roomGetTriggers(room), fkey);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to detach script from nonexistant room, %d.",self->uid);
    return NULL;
  }
}


//
// adds a new command to the room
PyObject *PyRoom_add_cmd(PyRoom *self, PyObject *args) {
  PyObject *func = NULL;
  char *name  = NULL, *sort_by = NULL, *min_pos = NULL, *max_pos = NULL,
       *group = NULL;
  bool mob_ok = FALSE, interrupts = FALSE;
  int min_pos_num, max_pos_num;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "szOsssbb", &name, &sort_by, &func,
  			&min_pos, &max_pos, &group, &mob_ok, &interrupts)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not add new room command. Improper arguments supplied");
    return NULL;
  }

  // make sure the room exists
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to add command to nonexistent room, %d", self->uid);
    return NULL;
  }

  // get our positions
  min_pos_num = posGetNum(min_pos);
  max_pos_num = posGetNum(max_pos);
  if(min_pos_num == POS_NONE || max_pos_num == POS_NONE) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not add new room command. Invalid position names.");
    return NULL;
  }

  // add the command to the game
  nearMapPut(roomGetCmdTable(room), name, sort_by,
	     newPyCmd(name, func, POS_STANDING, POS_FLYING,
		      group, TRUE, TRUE));
  return Py_None;
}

//
// returns whether or not the character is an instance of the prototype
PyObject *PyRoom_isinstance(PyRoom *self, PyObject *args) {  
  char *type = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &type)) {
    PyErr_Format(PyExc_TypeError, "isinstance only accepts strings.");
    return NULL;
  }

  // pull out the object and check the type
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL)
    return Py_BuildValue("i", 
        roomIsInstance(room, get_fullkey_relative(type, get_script_locale())));
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to check instances of nonexistent room, %d.",self->uid);
    return NULL;
  }
}



//*****************************************************************************
// structures to define our methods and classes
//*****************************************************************************
PyTypeObject PyRoom_Type = {
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

PyMethodDef room_module_methods[] = {
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
    PyRoom_addGetSetter("name",    PyRoom_getname,     PyRoom_setname, 
			"the room's name");
    PyRoom_addGetSetter("desc",    PyRoom_getdesc,     PyRoom_setdesc, 
			"the room's desc");
    PyRoom_addGetSetter("class",   PyRoom_getclass,    NULL, 
			"The room's class");
    PyRoom_addGetSetter("chars",   PyRoom_getchars,    NULL, 
			"chars in the room");
    PyRoom_addGetSetter("objs",  PyRoom_getobjs,       NULL, 
			"objects in the room");
    PyRoom_addGetSetter("contents",PyRoom_getobjs,     NULL, 
			"objects in the room");
    PyRoom_addGetSetter("exnames", PyRoom_getexnames,  NULL, 
			"the room's exits");
    PyRoom_addGetSetter("uid",     PyRoom_getuid,      NULL,
			"the room's uid");
    PyRoom_addGetSetter("terrain", PyRoom_getterrain,  PyRoom_setterrain,
			"the room's terrain type");

    // add all of the basic methods
    PyRoom_addMethod("attach", PyRoom_attach, METH_VARARGS,
		     "attach a new script to the room.");
    PyRoom_addMethod("detach", PyRoom_detach, METH_VARARGS,
		     "detach a script from the room, by vnum.");
    PyRoom_addMethod("dig", PyRoom_dig, METH_VARARGS,
		     "digs in direction to the target room. Returns exit.");
    PyRoom_addMethod("fill", PyRoom_fill, METH_VARARGS,
		     "fills in direction for the room.");
    PyRoom_addMethod("exit", PyRoom_get_exit, METH_VARARGS,
		     "gets an exit in the room with the given direction name.");
    PyRoom_addMethod("send", PyRoom_send, METH_VARARGS,
		     "send a message to everyone in the room.");
    PyRoom_addMethod("edesc", PyRoom_edesc, METH_VARARGS,
		     "adds an extra description to the room.");
    PyRoom_addMethod("add_cmd", PyRoom_add_cmd, METH_VARARGS,
		     "adds a command to the room.");
    PyRoom_addMethod("isinstance", PyRoom_isinstance, METH_VARARGS,
		     "returns whether or not the room inherits from the proto");

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
    PyTypeObject *type = &PyRoom_Type;
    PyModule_AddObject(module, "Room", (PyObject *)type);
    Py_INCREF(&PyRoom_Type);
}

int PyRoom_AsUid(PyObject *room) {
  return ((PyRoom *)room)->uid;
}

ROOM_DATA *PyRoom_AsRoom(PyObject *room) {
  return propertyTableGet(room_table, PyRoom_AsUid(room));
}

int PyRoom_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PyRoom_Type);
}

PyObject *
newPyRoom(ROOM_DATA *room) {
  PyRoom *py_room = (PyRoom *)PyRoom_new(&PyRoom_Type, NULL, NULL);
  py_room->uid = roomGetUID(room);
  return (PyObject *)py_room;
}
