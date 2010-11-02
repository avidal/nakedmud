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
#include "../items.h"
#include "../races.h"
#include "../handler.h"
#include "../utils.h"

#include "script.h"
#include "pychar.h"
#include "pyroom.h"
#include "pyobj.h"

// optional modules
#ifdef MODULE_CHAR_VARS
#include "../char_vars/char_vars.h"
#endif


typedef struct {
  PyObject_HEAD
  int uid;
} PyChar;


//*****************************************************************************
//
// allocation, deallocation, and initialiation
//
//*****************************************************************************
static void
PyChar_dealloc(PyChar *self) {
  self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
PyChar_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyChar *self;

    self = (PyChar  *)type->tp_alloc(type, 0);
    self->uid = NOBODY;//NULL;
    return (PyObject *)self;
}

static int
PyChar_init(PyChar *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[] = {"uid", NULL};
  int uid = NOBODY;

  // get the universal id
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &uid)) {
    PyErr_Format(PyExc_TypeError, 
                    "Characters may only be created by uid");
    return -1;
  }

  // make sure a character with the UID exists
  if(!propertyTableGet(mob_table, uid)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character with uid, %d, does not exist", uid);
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

//
// sends a newline-tagged message to the character
//
static PyObject *
PyChar_send(PyChar *self, PyObject *value) {
  char *mssg = NULL;
  if (!PyArg_ParseTuple(value, "s", &mssg)) {
    PyErr_Format(PyExc_TypeError, 
                    "Characters may only be sent strings");
    return NULL;
  }

  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch) {
    send_to_char(ch, "%s\r\n", mssg);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
                    "Tried to send message to nonexistant character, %d.", 
		    self->uid);
    return NULL;
  }
}


//
// make the character perform an action
//
static PyObject *
PyChar_act(PyChar *self, PyObject *value) {
  int scripts_ok     = TRUE;
  char *act          = NULL;
  if (!PyArg_ParseTuple(value, "s|i", &act, &scripts_ok)) {
    PyErr_Format(PyExc_TypeError, 
                    "Characters actions must be strings.");
    return NULL;
  }

  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch) {
    do_cmd(ch, act, scripts_ok, FALSE);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
                    "Nonexistant character, %d, tried to perform an action.", 
		    self->uid);
    return NULL;
  }
}


#ifdef MODULE_CHAR_VARS
//
// Get the value of a variable stored on the character
//
static PyObject *
PyChar_getvar(PyChar *self, PyObject *arg) {
  char *var = NULL;
  if (!PyArg_ParseTuple(arg, "s", &var)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character variables must have string names.");
    return NULL;
  }

  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) {
    int vartype = charGetVarType(ch, var);
    if(vartype == CHAR_VAR_INT)
      return Py_BuildValue("i", charGetInt(ch, var));
    else if(vartype == CHAR_VAR_LONG)
      return Py_BuildValue("i", charGetLong(ch, var));
    else if(vartype == CHAR_VAR_DOUBLE)
      return Py_BuildValue("d", charGetDouble(ch, var));
    else
      return Py_BuildValue("s", charGetString(ch, var));
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to get a variable value for nonexistant character, %d",
		 self->uid);
    return NULL;
  }
}


//
// Set the value of a variable assocciated with the character
//
static PyObject *
PyChar_setvar(PyChar *self, PyObject *args) {  
  char     *var = NULL;
  PyObject *val = NULL;

  if (!PyArg_ParseTuple(args, "sO", &var, &val)) {
    PyErr_Format(PyExc_TypeError, 
		 "Character setvar must be supplied with a var name and integer value.");
    return NULL;
  }


  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) {
    if(PyInt_Check(val))
      charSetInt(ch, var, (int)PyInt_AsLong(val));
    else if(PyFloat_Check(val))
      charSetDouble(ch, var, PyFloat_AsDouble(val));
    else if(PyString_Check(val))
      charSetString(ch, var, PyString_AsString(val));
    else {
      PyErr_Format(PyExc_TypeError,
		   "Tried to store a char_var of invalid type on char %d.",
		   self->uid);
      return NULL;
    }
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to set a variable value for nonexistant character, %d",
		 self->uid);
    return NULL;
  }
}
#endif



static PyMethodDef PyChar_methods[] = {
    {"send", (PyCFunction)PyChar_send, METH_VARARGS,
     "send a message to the character." },
    {"act", (PyCFunction)PyChar_act, METH_VARARGS,
     "make the character perform an action." },
#ifdef MODULE_CHAR_VARS
    {"getvar", (PyCFunction)PyChar_getvar, METH_VARARGS,
     "get the value of a special variable the character has."},
    {"setvar", (PyCFunction)PyChar_setvar, METH_VARARGS,
     "set the value of a special variable."},
#endif
    {NULL}  /* Sentinel */
};


//*****************************************************************************
//
// character attributes - mostly get and set
//
//*****************************************************************************
static PyObject *
PyChar_getname(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("s", charGetName(ch));
  else           return NULL;
}

static PyObject *
PyChar_getdesc(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("s", charGetDesc(ch));
  else           return NULL;
}

static PyObject *
PyChar_getrdesc(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("s", charGetRdesc(ch));
  else           return NULL;
}

static PyObject *
PyChar_getrace(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("s", raceGetName(charGetRace(ch)));
  else           return NULL;
}

static PyObject *
PyChar_getlevel(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("i", charGetLevel(ch));
  else           return NULL;
}

static PyObject *
PyChar_getsex(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("s", sexGetName(charGetSex(ch)));
  else           return NULL;
}

static PyObject *
PyChar_getposition(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("s", posGetName(charGetPos(ch)));
  else           return NULL;
}

static PyObject *
PyChar_getroom(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("O", newPyRoom(charGetRoom(ch)));
  else           return NULL;
}

static PyObject *
PyChar_getisnpc(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("i", charIsNPC(ch));
  else           return NULL;
}

static PyObject *
PyChar_getispc(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("i", !charIsNPC(ch));
  else           return NULL;
}

static PyObject *
PyChar_geton(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch == NULL) 
    return NULL;
  else if(charGetFurniture(ch) == NULL)
    return Py_None;
  else 
    return Py_BuildValue("i", newPyObj(charGetFurniture(ch)));
}

static PyObject *
PyChar_getuid(PyChar *self, void *closure) {
  return Py_BuildValue("i", self->uid);
}


static PyObject *
PyChar_getvnum(PyChar *self, void *closure) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch != NULL) return Py_BuildValue("i", charGetVnum(ch));
  else           return NULL;
}

static PyObject *
PyChar_getinv(PyChar *self, PyObject *args) {
  CHAR_DATA *ch = propertyTableGet(mob_table, self->uid);
  if(ch == NULL) 
    return NULL;

  LIST_ITERATOR *inv_i = newListIterator(charGetInventory(ch));
  PyObject *list = PyList_New(0);
  OBJ_DATA *obj;
  
  // for each obj in the inventory, add it to the Python list
  ITERATE_LIST(obj, inv_i)
    PyList_Append(list, newPyObj(obj));
  deleteListIterator(inv_i);
  return Py_BuildValue("O", list);
}



//
// Standard check to make sure the character exists when
// trying to set a value for it. If successful, assign the
// character to ch. Otherwise, return -1 (error)
//
#define PYCHAR_CHECK_CHAR_EXISTS(uid, ch)                                      \
  ch = propertyTableGet(mob_table, uid);                                       \
  if(ch == NULL) {                                                             \
    PyErr_Format(PyExc_TypeError,                                              \
		    "Tried to modify nonexistant character, %d", uid);         \
    return -1;                                                                 \
  }                                                                            


static int
PyChar_setname(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete character's name");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character names must be strings");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetName(ch, PyString_AsString(value));
  return 0;
}

static int
PyChar_setdesc(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete character's description");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character descriptions must be strings");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetDesc(ch, PyString_AsString(value));
  return 0;
}

static int
PyChar_setrdesc(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete character's rdesc");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character rdescs must be strings");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetRdesc(ch, PyString_AsString(value));
  return 0;
}

static int
PyChar_setrace(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's race");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character races must be strings");
    return -1;
  }

  int race = raceGetNum(PyString_AsString(value));
  if(race == RACE_NONE) {
    char buf[SMALL_BUFFER];
    sprintf(buf, "%s is an invalid race type", PyString_AsString(value));
    PyErr_Format(PyExc_TypeError, buf);
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetRace(ch, race);
  return 0;
}

static int
PyChar_seton(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's furniture.");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);

  if (value == Py_None) {
    char_from_furniture(ch);
    return 0;
  }
  else if(PyObj_Check(value)) {
    OBJ_DATA *obj = propertyTableGet(obj_table, PyObj_AsUid(value));
    if(obj == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "Tried to %s's furniture to a nonexistant object.",
		   charGetName(ch));
      return -1;
    }
    else if(objGetType(obj) == ITEM_FURNITURE) {
      if(charGetFurniture(ch))
	char_from_furniture(ch);
      char_to_furniture(ch, obj);
      return 0;
    }
  }

  PyErr_Format(PyExc_TypeError, 
	       "A Character's furniture may only be set to None or a ."
	       "furniture object.");
  return -1;
}

static int
PyChar_setsex(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's sex");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character sexes must be strings");
    return -1;
  }

  int sex = sexGetNum(PyString_AsString(value));
  if(sex == SEX_NONE) {
    char buf[SMALL_BUFFER];
    sprintf(buf, "%s is an invalid sex type", PyString_AsString(value));
    PyErr_Format(PyExc_TypeError, buf);
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetSex(ch, sex);
  return 0;
}

static int
PyChar_setposition(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's position");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character positions must be strings");
    return -1;
  }

  int pos = posGetNum(PyString_AsString(value));
  if(pos == POS_NONE) {
    char buf[SMALL_BUFFER];
    sprintf(buf, "%s is an invalid position type", PyString_AsString(value));
    PyErr_Format(PyExc_TypeError, buf);
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetPos(ch, pos);
  // players can't be on furniture if they are standing or flying
  if(poscmp(charGetPos(ch), POS_STANDING) >= 0 && charGetFurniture(ch))
    char_from_furniture(ch);
  return 0;
}

static int
PyChar_setlevel(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's level");
    return -1;
  }
  
  if (!PyInt_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character levels must be integers");
    return -1;
  }

  long lvl = PyInt_AsLong(value);
  if(lvl < 1 || lvl > MAX_LEVEL) {
    char buf[SMALL_BUFFER];
    sprintf(buf, "Character levels are bounded between 1 and %d", MAX_LEVEL);
    PyErr_Format(PyExc_TypeError, buf);
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetLevel(ch, lvl);
  return 0;
}

static int
PyChar_setroom(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's room");
    return -1;
  }

  int vnum = NOWHERE;

  if(PyRoom_Check(value))
    vnum = PyRoom_AsVnum(value);
  else if(PyInt_Check(value))
    vnum = (int)PyInt_AsLong(value); // hmmm... is this safe?
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Character's room must be integer an integer value or a "
		 "room object.");
    return -1;
  }

  ROOM_DATA *room = worldGetRoom(gameworld, vnum);
  if(room == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Attempting to move character to nonexistant room, %d.",
		 vnum);
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  // only move if we're not already here
  if(charGetRoom(ch) != room) {
    char_from_room(ch);
    char_to_room(ch, room);

    // if we were on furniture, make sure we dismount it
    if(charGetFurniture(ch))
      char_from_furniture(ch);
  }

  return 0;
}


static PyGetSetDef PyChar_getseters[] = {
  {"inv", (getter)PyChar_getinv, (setter)NULL,
   "The objects in the character's inventory", 
   NULL},
  {"objs", (getter)PyChar_getinv, (setter)NULL,
   "The objects in the character's inventory", 
   NULL},
  {"name", (getter)PyChar_getname, (setter)PyChar_setname,
   "the character's name",
   NULL},
  {"desc", (getter)PyChar_getdesc, (setter)PyChar_setdesc,
   "the character's description",
   NULL},
  {"rdesc", (getter)PyChar_getrdesc, (setter)PyChar_setrdesc,
   "the character's room description",
   NULL},
  {"level", (getter)PyChar_getlevel, (setter)PyChar_setlevel,
   "the character's level",
   NULL},
  {"sex", (getter)PyChar_getsex, (setter)PyChar_setsex,
   "the character's sex",
   NULL},
  {"race", (getter)PyChar_getrace, (setter)PyChar_setrace,
   "the character's race",
   NULL},
  {"position", (getter)PyChar_getposition, (setter)PyChar_setposition,
   "the character's position",
   NULL},
  {"room", (getter)PyChar_getroom, (setter)PyChar_setroom,
   "the character's room",
   NULL},
  {"on", (getter)PyChar_geton, (setter)PyChar_seton,
   "The furniture the character is sitting on/at. If the character is not "
   "on furniture, None is returned. To remove a character from furniture, "
   "then use None",
   NULL},
  {"uid", (getter)PyChar_getuid, (setter)NULL,
   "the unique identification number",
   NULL},
  {"vnum", (getter)PyChar_getvnum, (setter)NULL,
   "the virtual number for the NPC. Returns -1 for characters.",
   NULL},
  {"is_npc", (getter)PyChar_getisnpc, (setter)NULL,
   "returns 1 if the char is an NPC, and 0 otherwise.",
   NULL},
  {"is_pc", (getter)PyChar_getispc, (setter)NULL,
   "returns 1 if the char is an NPC, and 0 otherwise.",
   NULL},

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
PyChar_compare(PyChar *ch1, PyChar *ch2) {
  if(ch1->uid == ch2->uid)
    return 0;
  else if(ch1->uid < ch2->uid)
    return -1;
  else
    return 1;
}

static PyTypeObject PyChar_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "char.Char",               /*tp_name*/
    sizeof(PyChar),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyChar_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    (cmpfunc)PyChar_compare,   /*tp_compare*/
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
    "Char/Mob objects",        /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    PyChar_methods,            /* tp_methods */
    0,                         /* tp_members */
    PyChar_getseters,          /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PyChar_init,    /* tp_init */
    0,                         /* tp_alloc */
    PyChar_new,               /* tp_new */
};



//*****************************************************************************
//
// the char module
//
//*****************************************************************************
static PyObject *
PyChar_load_mob(PyObject *self, PyObject *args) {
  int mob_vnum    = NOBODY, room_vnum = NOWHERE;
  PyObject *to    = NULL;

  ROOM_DATA *room  = NULL;
  OBJ_DATA  *on    = NULL;
  char  *posname   = NULL;
  
  if (!PyArg_ParseTuple(args, "iO|s", &mob_vnum, &to, &posname)) {
    PyErr_Format(PyExc_TypeError, 
		 "Load char failed - it needs vnum and destination.");
    return NULL;
  }

  // see what we're trying to load to
  if(PyInt_Check(to))
    room_vnum = (int)PyInt_AsLong(to);
  else if(PyRoom_Check(to))
    room_vnum = (int)PyRoom_AsVnum(to);
  else if(PyObj_Check(to))
    on = propertyTableGet(obj_table, PyObj_AsUid(to));
  else {
    PyErr_Format(PyExc_TypeError, 
                    "Load char failed: invalid load-to type.");
    return NULL;
  }

  // check the mob
  CHAR_DATA *mob_proto = worldGetMob(gameworld, mob_vnum);
  if(mob_proto == NULL) {
    PyErr_Format(PyExc_TypeError, 
                    "Load char failed: mobile number does not exist.");
    return NULL;
  }


  // see if we're loading onto something
  if(on)
    room = objGetRoom(on);
  else
    room = worldGetRoom(gameworld, room_vnum);

  if(room == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Load char failed: room does not exist, or furniture is not. "
		 "in a room.");
    return NULL;
  }

  // copy the mob, and put it into the game
  CHAR_DATA *mob = charCopy(mob_proto);
  char_to_game(mob);
  char_to_room(mob, room);

  // now check if we need to put the char onto some furniture
  // default position is POS_SITTING
  if(on) {
    int pos = POS_SITTING;
    char_to_furniture(mob, on);
    if(posname)
      pos = posGetNum(posname);

    // if the position is none, or greater 
    // than sitting, default to sitting.
    if(pos == POS_NONE || poscmp(pos, POS_SITTING) > 0)
      pos = POS_SITTING;
    charSetPos(mob, pos);
  }
  else if(posname) {
    int pos = posGetNum(posname);
    // if it was an invalid name, set it to standing
    if(pos == POS_NONE)
      pos = POS_STANDING;
    charSetPos(mob, pos);
  }

  // check for initialization scripts
  try_scripts(SCRIPT_TYPE_INIT,
	      mob, SCRIPTOR_CHAR,
	      mob, NULL, room, NULL, NULL, NULL, 0);

  // create a python object for the new char, and return it
  PyChar *py_mob = (PyChar *)newPyChar(mob);
  return Py_BuildValue("O", py_mob);
}


static PyObject *
PyChar_count_mobs(PyObject *self, PyObject *args) {
  LIST *list = NULL;
  PyObject *tgt;
  PyObject *in = NULL;
  ROOM_DATA *room = NULL;
  OBJ_DATA  *furniture = NULL;
  int vnum = NOBODY;
  char *name = NULL;

  if (!PyArg_ParseTuple(args, "O|O", &tgt, &in)) {
    PyErr_Format(PyExc_TypeError, 
                    "count_mobs failed. No arguments supplied.");
    return NULL;
  }

  // see if we're looking by name or vnum
  if(PyInt_Check(tgt))
    vnum = PyInt_AsLong(tgt);
  else if(PyString_Check(tgt))
    name = PyString_AsString(tgt);
  else {
    PyErr_Format(PyExc_TypeError, 
                    "count_mobs failed. Invalid target type supplied.");
    return NULL;
  }

  // if we didn't supply something to look in, assume it means the world
  if(in == NULL)
    return Py_BuildValue("i", count_chars(NULL, mobile_list,name, vnum, FALSE));


  // see what we're looking in
  if(PyInt_Check(in))
    room = worldGetRoom(gameworld, PyInt_AsLong(in));
  else if(PyRoom_Check(in))
    room = worldGetRoom(gameworld, PyRoom_AsVnum(in));
  else if(PyObj_Check(in))
    furniture = propertyTableGet(obj_table, PyObj_AsUid(in));

  // now find the list we're dealing with
  if(room)      list = roomGetCharacters(room);
  if(furniture) list = objGetUsers(furniture);

  if(list == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "count_mobs failed. invalid argument supplied.");
    return NULL;
  }
  
  return Py_BuildValue("i", count_chars(NULL, list, name, vnum, FALSE));
}


static PyMethodDef char_module_methods[] = {
  { "load_mob", PyChar_load_mob, METH_VARARGS,
    "load a mobile with the specified vnum to a room." },
  { "count_mobs", PyChar_count_mobs, METH_VARARGS,
    "count how many occurances of a mobile there are in the specified scope. "
    "vnum or name can be used. Vnum -1 counts PCs" },
  {NULL, NULL, 0, NULL}  /* Sentinel */
};


PyMODINIT_FUNC
init_PyChar(void) 
{
    PyObject* m;

    if (PyType_Ready(&PyChar_Type) < 0)
        return;

    m = Py_InitModule3("char", char_module_methods,
                       "The char module, for all char/mob-related MUD stuff.");

    if (m == NULL)
      return;

    Py_INCREF(&PyChar_Type);
    PyModule_AddObject(m, "Char", (PyObject *)&PyChar_Type);
}

int PyChar_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PyChar_Type);
}

int PyChar_AsUid(PyObject *ch) {
  return ((PyChar *)ch)->uid;
}

PyObject *
newPyChar(CHAR_DATA *ch) {
  PyChar *py_ch = (PyChar *)PyChar_new(&PyChar_Type, NULL, NULL);
  py_ch->uid = charGetUID(ch);
  return (PyObject *)py_ch;
}
