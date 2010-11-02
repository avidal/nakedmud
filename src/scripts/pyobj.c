//*****************************************************************************
//
// py_char.c
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
#include "../character.h"
#include "../object.h"
#include "../races.h"
#include "../handler.h"
#include "../utils.h"

#include "script.h"
#include "script_set.h"
#include "pychar.h"
#include "pyroom.h"
#include "pyobj.h"


typedef struct {
  PyObject_HEAD
  int uid;
} PyObj;


//*****************************************************************************
//
// allocation, deallocation, and initialiation
//
//*****************************************************************************
static void
PyObj_dealloc(PyObj *self) {
  self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
PyObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyObj *self;

    self = (PyObj  *)type->tp_alloc(type, 0);
    self->uid = NOTHING;
    return (PyObject *)self;
}

static int
PyObj_init(PyObj *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[] = {"uid", NULL};
  int uid = NOTHING;

  // get the universal id
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &uid)) {
    PyErr_Format(PyExc_TypeError, 
                    "Objects may only be created by uid");
    return -1;
  }

  // make sure a object with the UID exists
  if(!propertyTableGet(obj_table, uid)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object with uid, %d, does not exist", uid);
    return -1;
  }

  self->uid = uid;
  return 0;
}




//*****************************************************************************
//
// methods and stuff for building the class
//
//*****************************************************************************

static PyObject *
PyObj_attach(PyObj *self, PyObject *args) {  
  long vnum = NOTHING;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "i", &vnum)) {
    PyErr_Format(PyExc_TypeError, 
		 "To attach a script, the vnum must be suppplied.");
    return NULL;
  }

  // pull out the character and do the attaching
  OBJ_DATA       *obj = propertyTableGet(obj_table, self->uid);
  SCRIPT_DATA *script = worldGetScript(gameworld, vnum);
  if(obj != NULL && script != NULL) {
    scriptSetAdd(objGetScripts(obj), vnum);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to attach script to nonexistant obj, %d, or script %d "
		 "does not exit.", self->uid, (int)vnum);
    return NULL;
  }
}


static PyObject *
PyObj_detach(PyObj *self, PyObject *args) {  
  long vnum = NOTHING;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "i", &vnum)) {
    PyErr_Format(PyExc_TypeError, 
		 "To detach a script, the vnum must be suppplied.");
    return NULL;
  }

  // pull out the character and do the attaching
  OBJ_DATA       *obj = propertyTableGet(obj_table, self->uid);
  SCRIPT_DATA *script = worldGetScript(gameworld, vnum);
  if(obj != NULL && script != NULL) {
    scriptSetRemove(objGetScripts(obj), vnum);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to detach script from nonexistant obj, %d, or script "
		 "%d does not exit.", self->uid, (int)vnum);
    return NULL;
  }
}


static PyMethodDef PyObj_methods[] = {
  {"attach", (PyCFunction)PyObj_attach, METH_VARARGS,
   "attach a new script to the object." },
  {"detach", (PyCFunction)PyObj_detach, METH_VARARGS,
   "detach a script from the object." },
  {NULL}  /* Sentinel */
};


//*****************************************************************************
//
// character attributes - mostly get and set
//
//*****************************************************************************
static PyObject *
PyObj_getname(PyObj *self, void *closure) {
  OBJ_DATA *obj = propertyTableGet(obj_table, self->uid);
  if(obj != NULL) return Py_BuildValue("s", objGetName(obj));
  else           return NULL;
}

static PyObject *
PyObj_getdesc(PyObj *self, void *closure) {
  OBJ_DATA *obj = propertyTableGet(obj_table, self->uid);
  if(obj != NULL) return Py_BuildValue("s", objGetDesc(obj));
  else           return NULL;
}

static PyObject *
PyObj_getrdesc(PyObj *self, void *closure) {
  OBJ_DATA *obj = propertyTableGet(obj_table, self->uid);
  if(obj != NULL) return Py_BuildValue("s", objGetRdesc(obj));
  else           return NULL;
}

static PyObject *
PyObj_getuid(PyObj *self, void *closure) {
  return Py_BuildValue("i", self->uid);
}

static PyObject *
PyObj_getvnum(PyObj *self, void *closure) {
  OBJ_DATA *obj = propertyTableGet(obj_table, self->uid);
  if(obj != NULL) return Py_BuildValue("i", objGetVnum(obj));
  else           return NULL;
}

static PyObject *
PyObj_getcontents(PyObj *self, PyObject *args) {
  OBJ_DATA *obj = propertyTableGet(obj_table, self->uid);
  if(obj == NULL) 
    return NULL;

  LIST_ITERATOR *cont_i = newListIterator(objGetContents(obj));
  PyObject *list = PyList_New(0);
  OBJ_DATA *cont;
  
  // for each obj in the contentory, add it to the Python list
  ITERATE_LIST(cont, cont_i)
    PyList_Append(list, newPyObj(cont));
  deleteListIterator(cont_i);
  return Py_BuildValue("O", list);
}

static PyObject *
PyObj_getchars(PyObj *self, PyObject *args) {
  OBJ_DATA *obj = propertyTableGet(obj_table, self->uid);
  if(obj == NULL) 
    return NULL;

  LIST_ITERATOR *char_i = newListIterator(objGetUsers(obj));
  PyObject *list = PyList_New(0);
  CHAR_DATA *ch;
  
  // for each obj in the contentory, add it to the Python list
  ITERATE_LIST(ch, char_i)
    PyList_Append(list, newPyChar(ch));
  deleteListIterator(char_i);
  return Py_BuildValue("O", list);
}


//
// Standard check to make sure the object exists when
// trying to set a value for it. If successful, assign the
// object to ch. Otherwise, return -1 (error)
//
#define PYOBJ_CHECK_OBJ_EXISTS(uid, obj)                                       \
  obj = propertyTableGet(obj_table, uid);                                      \
  if(obj == NULL) {                                                            \
    PyErr_Format(PyExc_TypeError,                                              \
		    "Tried to modify nonexistant object, %d", uid);            \
    return -1;                                                                 \
  }                                                                            

static int
PyObj_setname(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's name");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object names must be strings");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  objSetName(obj, PyString_AsString(value));
  return 0;
}

static int
PyObj_setdesc(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's description");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object descriptions must be strings");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  objSetDesc(obj, PyString_AsString(value));
  return 0;
}

static int
PyObj_setrdesc(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's rdesc");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object rdescs must be strings");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  objSetRdesc(obj, PyString_AsString(value));
  return 0;
}


static PyGetSetDef PyObj_getseters[] = {
  {"contents", (getter)PyObj_getcontents, (setter)NULL,
   "the object's contents",
   NULL},
  {"objs", (getter)PyObj_getcontents, (setter)NULL,
   "the object's contents",
   NULL},
  {"chars", (getter)PyObj_getchars, (setter)NULL,
   "the characters sitting on/riding the object",
   NULL},
  {"name", (getter)PyObj_getname, (setter)PyObj_setname,
   "the object's name",
   NULL},
  {"desc", (getter)PyObj_getdesc, (setter)PyObj_setdesc,
   "the object's description",
   NULL},
  {"rdesc", (getter)PyObj_getrdesc, (setter)PyObj_setrdesc,
   "the object's room description",
   NULL},
  {"uid", (getter)PyObj_getuid, (setter)NULL,
   "the unique identification number",
   NULL},
  {"vnum", (getter)PyObj_getvnum, (setter)NULL,
   "the virtual number for the object.",
   NULL},
  {NULL}  /* Sentinel */
};



//*****************************************************************************
//
// comparators, getattr, setattr, and all that other class stuff
//
//*****************************************************************************

//
// compare one object to another
//
static int
PyObj_compare(PyObj *obj1, PyObj *obj2) {
  if(obj1->uid == obj2->uid)
    return 0;
  else if(obj1->uid < obj2->uid)
    return -1;
  else
    return 1;
}

static PyTypeObject PyObj_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "obj.Obj",                 /*tp_name*/
    sizeof(PyObj),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyObj_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    (cmpfunc)PyObj_compare,    /*tp_compare*/
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
    "Obj/Obj objects",         /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    PyObj_methods,             /* tp_methods */
    0,                         /* tp_members */
    PyObj_getseters,           /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PyObj_init,      /* tp_init */
    0,                         /* tp_alloc */
    PyObj_new,                 /* tp_new */
};



//*****************************************************************************
//
// the obj module
//
//*****************************************************************************
static PyObject *
PyObj_load_obj(PyObject *self, PyObject *args) {
  int obj_vnum = NOBODY;
  PyObject *in = NULL;

  ROOM_DATA *room = NULL; // are we loading to a room?
  OBJ_DATA  *cont = NULL; // are we loading to a container?
  CHAR_DATA *ch   = NULL; // are we loading to a character?
  char *equip_to  = NULL; // are we trying to equip the character?

  if (!PyArg_ParseTuple(args, "iO|s", &obj_vnum, &in, &equip_to)) {
    PyErr_Format(PyExc_TypeError, 
		 "Load obj failed - it needs a vnum and destination.");
    return NULL;
  }

  // check the obj
  OBJ_DATA *obj_proto = worldGetObj(gameworld, obj_vnum);
  if(obj_proto == NULL) {
    PyErr_Format(PyExc_TypeError, 
                    "Load obj failed: object number does not exist.");
    return NULL;
  }

  // copy the object
  OBJ_DATA *obj = objCopy(obj_proto);


  // figure out what we're trying to load this thing into
  if(PyInt_Check(in))
    room = worldGetRoom(gameworld, (int)PyInt_AsLong(in));
  else if(PyRoom_Check(in))
    room = worldGetRoom(gameworld, PyRoom_AsVnum(in));
  else if(PyObj_Check(in))
    cont = propertyTableGet(obj_table, PyObj_AsUid(in));
  else if(PyChar_Check(in))
    ch   = propertyTableGet(mob_table, PyChar_AsUid(in));


  // figure out where we're trying to load the object to
  if(room != NULL) {
    obj_to_game(obj);
    obj_to_room(obj, room);
  }
  else if(cont != NULL) {
    obj_to_game(obj);
    obj_to_obj(obj, cont);
  }
  else if(ch != NULL) {
    // see if we're trying to equip the object
    if(equip_to) {
      obj_to_game(obj);
      if(!try_equip(ch, obj, equip_to))
	obj_to_char(obj, ch);
    }
    else {
      obj_to_game(obj);
      obj_to_char(obj, ch);
    }
  }

  // We couldn't figure out the destination!
  else {
    PyErr_Format(PyExc_TypeError, 
                    "Load obj failed: destination does not exist.");
    return NULL;
  }

  // check for initialization scripts
  try_scripts(SCRIPT_TYPE_INIT,
	      obj, SCRIPTOR_OBJ,
	      ch, cont, room, NULL, NULL, 0);

  // create a python object for the new obj, and return it
  PyObj *py_obj = (PyObj *)newPyObj(obj);
  return Py_BuildValue("O", py_obj);
}


static PyObject *
PyObj_count_objs(PyObject *self, PyObject *args) {
  LIST *list      = NULL;
  PyObject *tgt;
  PyObject *in    = NULL;
  ROOM_DATA *room = NULL;
  OBJ_DATA  *cont = NULL;
  CHAR_DATA *ch   = NULL;
  int vnum = NOTHING;
  char *name = NULL;

  if (!PyArg_ParseTuple(args, "O|O", &tgt, &in)) {
    PyErr_Format(PyExc_TypeError, 
                    "count_objs failed. No arguments supplied.");
    return NULL;
  }

  // see if we're looking by name or vnum
  if(PyInt_Check(tgt))
    vnum = PyInt_AsLong(tgt);
  else if(PyString_Check(tgt))
    name = PyString_AsString(tgt);
  else {
    PyErr_Format(PyExc_TypeError, 
                    "count_objs failed. Invalid target type supplied.");
    return NULL;
  }

  // if we didn't supply something to look in, assume it means the world
  if(in == NULL)
    return Py_BuildValue("i", count_objs(NULL, object_list, name, vnum, FALSE));

  // see what we're looking in
  if(PyInt_Check(in))
    room = worldGetRoom(gameworld, PyInt_AsLong(in));
  else if(PyRoom_Check(in))
    room = worldGetRoom(gameworld, PyRoom_AsVnum(in));
  else if(PyObj_Check(in))
    cont = propertyTableGet(obj_table, PyObj_AsUid(in));
  else if(PyChar_Check(in))
    ch   = propertyTableGet(mob_table, PyChar_AsUid(in));

  // now find the list we're dealing with
  if(room) list = roomGetContents(room);
  if(cont) list = objGetContents(cont);
  if(ch)   list = charGetInventory(ch);

  if(list == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "count_objs failed. invalid argument supplied.");
    return NULL;
  }
  
  return Py_BuildValue("i", count_objs(NULL, list, name, vnum, FALSE));
}


static PyObject *
PyObj_find_obj(PyObject *self, PyObject *args) {
  LIST *list           = object_list;
  PyObject *in         = NULL;
  ROOM_DATA *room      = NULL;
  OBJ_DATA  *cont      = NULL;
  CHAR_DATA *ch        = NULL;
  CHAR_DATA *looker_ch = NULL;
  PyObject *looker     = NULL;
  char *tgt            = NULL;

  if (!PyArg_ParseTuple(args, "s|OO", &tgt, &in, &looker)) {
    PyErr_Format(PyExc_TypeError, 
                    "find_obj failed. No arguments supplied.");
    return NULL;
  }

  // check for scope of search
  if(in) {
    if(PyInt_Check(in))
      room = worldGetRoom(gameworld, PyInt_AsLong(in));
    else if(PyRoom_Check(in))
      room = worldGetRoom(gameworld, PyRoom_AsVnum(in));
    else if(PyObj_Check(in))
      cont = propertyTableGet(obj_table, PyObj_AsUid(in));
    else if(PyChar_Check(in))
      ch   = propertyTableGet(mob_table, PyChar_AsUid(in));
  }

  // check to see who's looking
  if(looker && PyChar_Check(looker))
    looker_ch = propertyTableGet(mob_table, PyChar_AsUid(looker));

  // now find the list we're dealing with
  if(room) list = roomGetContents(room);
  if(cont) list = objGetContents(cont);
  if(ch)   list = charGetInventory(ch);

  // now, do the search
  int count = 1;
  char name[SMALL_BUFFER] = "";
  get_count(tgt, name, &count);

  // we're just looking for a single item
  if(count != COUNT_ALL) {
    OBJ_DATA *obj    = find_obj(looker_ch, list, count, name, NOTHING, 
				(looker_ch ? TRUE : FALSE));
    PyObject *py_obj = Py_None;
    if(obj) py_obj = newPyObj(obj);
    return Py_BuildValue("O", py_obj);
  }
  // otherwise, return everything that meets our critereon
  else {
    //***********
    // FINISH ME
    //***********
    return Py_BuildValue("O", Py_None);
  }
}



static PyMethodDef obj_module_methods[] = {
  { "load_obj", PyObj_load_obj, METH_VARARGS,
    "load a object with the specified vnum to a room." },
  { "count_objs", PyObj_count_objs, METH_VARARGS,
    "count how many occurances of an object there are in the specified scope. "
    "vnum or name can be used."},
  { "find_obj", PyObj_find_obj, METH_VARARGS,
    "Takes a string argument, and returns the object(s) in the scope that "
    "correspond to what the string is searching for."},
  {NULL, NULL, 0, NULL}  /* Sentinel */
};



PyMODINIT_FUNC
init_PyObj(void) 
{
    PyObject* m;

    if (PyType_Ready(&PyObj_Type) < 0)
        return;

    m = Py_InitModule3("obj", obj_module_methods,
                       "The object module, for all object-related MUD stuff.");

    if (m == NULL)
      return;

    Py_INCREF(&PyObj_Type);
    PyModule_AddObject(m, "Obj", (PyObject *)&PyObj_Type);
}

int PyObj_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PyObj_Type);
}

int PyObj_AsUid(PyObject *obj) {
  return ((PyObj *)obj)->uid;
}

PyObject *
newPyObj(OBJ_DATA *obj) {
  PyObj *py_obj = (PyObj *)PyObj_new(&PyObj_Type, NULL, NULL);
  py_obj->uid = objGetUID(obj);
  return (PyObject *)py_obj;
}
