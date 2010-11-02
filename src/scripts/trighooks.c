//*****************************************************************************
//
// trighooks.c
//
// Triggers attach on to rooms, objects, and mobiles as hooks. When a hook
// event occurs, all of the triggers of the right type will run. This header is
// just to allow scripts to initialize the hooks into the game. The init 
// function here should not be touched by anything other than scripts.c
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../hooks.h"
#include "../character.h"
#include "../room.h"
#include "../object.h"
#include "../world.h"
#include "../zone.h"

#include "scripts.h"
#include "pychar.h"
#include "pyobj.h"
#include "pyroom.h"
#include "pyexit.h"



//*****************************************************************************
// local datastructures and defines
//*****************************************************************************

// values for figuring out what "me" and optional variables are in gen_do_trig
#define VARTYPE_CHAR      0
#define VARTYPE_OBJ       1
#define VARTYPE_ROOM      2

// used for providing additional variables to gen_do_trig that are not standard
typedef struct {
  char *name;
  void *data;
  int   type;
} OPT_VAR;

OPT_VAR *newOptVar(const char *name, void *data, int type) {
  OPT_VAR *var = malloc(sizeof(OPT_VAR));
  var->name    = strdupsafe(name);
  var->data    = data;
  var->type    = type;
  return var;
}

void deleteOptVar(OPT_VAR *var) {
  if(var->name) free(var->name);
  free(var);
}



//*****************************************************************************
// trigger handlers
//*****************************************************************************

//
// generalized function for setting up a dictionary and running a trigger. The
// common types of variables can be supplied in the function. Additional ones
// can be added in the optional list, which must be deleted after use
void gen_do_trig(TRIGGER_DATA *trig, 
		 void *me, int me_type, CHAR_DATA *ch, OBJ_DATA *obj,
		 ROOM_DATA *room, EXIT_DATA *exit, const char *command,
		 const char *arg, LIST *optional) {
  // make our basic dictionary, and fill it up with these new variables
  PyObject *dict = restricted_script_dict();
  // now, import all of our variables
  if(command) {
    PyObject *pycmd = PyString_FromString(command);
    PyDict_SetItemString(dict, "cmd", pycmd);
    Py_DECREF(pycmd);
  }
  if(arg) {
    PyObject *pyarg = PyString_FromString(arg);
    PyDict_SetItemString(dict, "arg", pyarg);
    Py_DECREF(pyarg);
  }
  if(ch) {
    PyObject *pych = charGetPyForm(ch);
    PyDict_SetItemString(dict, "ch", pych);
    Py_DECREF(pych);
  }
  if(room) {
    PyObject *pyroom = roomGetPyForm(room);
    PyDict_SetItemString(dict, "room", pyroom);
    Py_DECREF(pyroom);
  }    
  if(obj) {
    PyObject *pyobj = objGetPyForm(obj);
    PyDict_SetItemString(dict, "obj", pyobj);
    Py_DECREF(pyobj);
  }
  if(exit) {
    PyObject *pyexit = newPyExit(exit);
    PyDict_SetItemString(dict, "ex", pyexit);
    Py_DECREF(pyexit);
  }

  // add the thing the tirgger is attached to
  if(me) {
    PyObject *pyme = NULL;
    switch(me_type) {
    case VARTYPE_CHAR:  pyme = charGetPyForm(me); break;
    case VARTYPE_OBJ:   pyme = objGetPyForm(me);  break;
    case VARTYPE_ROOM:  pyme = roomGetPyForm(me); break;
    }
    PyDict_SetItemString(dict, "me", pyme);
    Py_DECREF(pyme);
  }

  // now, add any optional variables
  if(optional) {
    LIST_ITERATOR *opt_i = newListIterator(optional);
    OPT_VAR         *opt = NULL;
    PyObject      *pyopt = NULL;
    ITERATE_LIST(opt, opt_i) {
      pyopt = NULL;
      switch(opt->type) {
      case VARTYPE_CHAR:  pyopt = charGetPyForm(opt->data); break;
      case VARTYPE_OBJ:   pyopt = objGetPyForm(opt->data);  break;
      case VARTYPE_ROOM:  pyopt = roomGetPyForm(opt->data); break;
      }
      PyDict_SetItemString(dict, opt->name, pyopt);
      Py_XDECREF(pyopt);
    } deleteListIterator(opt_i);
  }

  // run the script, then kill our dictionary
  triggerRun(trig, dict);
  Py_DECREF(dict);
}


//
// handles all of a character's triggers
void do_char_trigs(CHAR_DATA *ch, const char *type, void *thing, void *arg) {
  if(listSize(charGetTriggers(ch)) > 0) {
    // first, build a list of all our triggers of this type
    LIST           *trigs = newList();
    LIST_ITERATOR *trig_i = newListIterator(charGetTriggers(ch));
    char             *key = NULL;
    TRIGGER_DATA    *trig = NULL;
    ITERATE_LIST(key, trig_i) {
      if((trig = worldGetType(gameworld, "trigger", key)) != NULL &&
	 !strcasecmp(triggerGetType(trig), type))
	listPut(trigs, trig);
    } deleteListIterator(trig_i);

    // did we find any triggers?
    if(listSize(trigs) > 0) {
      trig_i = newListIterator(trigs);
      ITERATE_LIST(trig, trig_i) {
	if(!strcasecmp(type, "speech"))
	  gen_do_trig(trig,ch,VARTYPE_CHAR,thing,NULL,NULL,NULL,NULL,arg,NULL);
	else if(!strcasecmp(type, "move"))
	  gen_do_trig(trig,ch,VARTYPE_CHAR,NULL,NULL,NULL,thing,NULL,NULL,NULL);
	else if(!strcasecmp(type, "enter"))
	  gen_do_trig(trig,ch,VARTYPE_CHAR,thing,NULL,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "exit"))
	  gen_do_trig(trig,ch,VARTYPE_CHAR,thing,NULL,NULL,arg,NULL,NULL,NULL);
	else if(!strcasecmp(type, "greet"))
	  gen_do_trig(trig,ch,VARTYPE_CHAR,thing,NULL,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "give"))
	  gen_do_trig(trig,ch,VARTYPE_CHAR,thing,arg,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "receive"))
	  gen_do_trig(trig,ch,VARTYPE_CHAR,thing,arg,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "wear"))
	  gen_do_trig(trig,ch,VARTYPE_CHAR,NULL,thing,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "remove"))
	  gen_do_trig(trig,ch,VARTYPE_CHAR,NULL,thing,NULL,NULL,NULL,NULL,NULL);
	else {
	  log_string("Unrecognized trigger type %s attached to %s, uid %d.\r\n",
		     type, charGetClass(ch), charGetUID(ch));
	}
      } deleteListIterator(trig_i);
    }
    
    // clean up our mess
    deleteList(trigs);
  }
}

//
// handles all of an object's triggers
void do_obj_trigs(OBJ_DATA *obj, const char *type, void *thing, void *arg) {
  if(listSize(objGetTriggers(obj)) > 0) {
    // first, build a list of all our triggers of this type
    LIST           *trigs = newList();
    LIST_ITERATOR *trig_i = newListIterator(objGetTriggers(obj));
    char             *key = NULL;
    TRIGGER_DATA    *trig = NULL;
    ITERATE_LIST(key, trig_i) {
      if((trig = worldGetType(gameworld, "trigger", key)) != NULL &&
	 !strcasecmp(triggerGetType(trig), type))
	listPut(trigs, trig);
    } deleteListIterator(trig_i);

    // did we find any triggers?
    if(listSize(trigs) > 0) {
      trig_i = newListIterator(trigs);
      ITERATE_LIST(trig, trig_i) {
	if(!strcasecmp(type, "give")) {
	  // set up the optional "receiver" variable
	  LIST *opts = newList();
	  listPut(opts, newOptVar("recv", arg, VARTYPE_CHAR));
	  gen_do_trig(trig,obj,VARTYPE_OBJ,thing,NULL,NULL,NULL,NULL,NULL,opts);
	  deleteListWith(opts, deleteOptVar);
	}
	else if(!strcasecmp(type, "get"))
	  gen_do_trig(trig,obj,VARTYPE_OBJ,thing,NULL,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "drop"))
	  gen_do_trig(trig,obj,VARTYPE_OBJ,thing,NULL,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "wear"))
	  gen_do_trig(trig,obj,VARTYPE_OBJ,thing,NULL,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "remove"))
	  gen_do_trig(trig,obj,VARTYPE_OBJ,thing,NULL,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "open"))
	  gen_do_trig(trig,obj,VARTYPE_OBJ,thing,NULL,NULL,NULL,NULL,NULL,NULL);
	else {
	  log_string("Unrecognized trigger type %s attached to %s, uid %d.\r\n",
		     type, objGetClass(obj), objGetUID(obj));
	}
      } deleteListIterator(trig_i);
    }

    // clean up our mess
    deleteList(trigs);
  }
}

//
// handles all of a room's triggers
void do_room_trigs(ROOM_DATA *rm, const char *type, void *thing, void *arg){
  if(listSize(roomGetTriggers(rm)) > 0) {
    // first, build a list of all our triggers of this type
    LIST           *trigs = newList();
    LIST_ITERATOR *trig_i = newListIterator(roomGetTriggers(rm));
    char             *key = NULL;
    TRIGGER_DATA    *trig = NULL;
    ITERATE_LIST(key, trig_i) {
      if((trig = worldGetType(gameworld, "trigger", key)) != NULL &&
	 !strcasecmp(triggerGetType(trig), type))
	listPut(trigs, trig);
    } deleteListIterator(trig_i);

    // did we find any triggers?
    if(listSize(trigs) > 0) {
      trig_i = newListIterator(trigs);
      ITERATE_LIST(trig, trig_i) {
	if(!strcasecmp(type, "get"))
	  gen_do_trig(trig,rm,VARTYPE_ROOM,thing,arg,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "drop"))
	  gen_do_trig(trig,rm,VARTYPE_ROOM,thing,arg,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "enter"))
	  gen_do_trig(trig,rm,VARTYPE_ROOM,thing,NULL,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "exit"))
	  gen_do_trig(trig,rm,VARTYPE_ROOM,thing,NULL,NULL,arg,NULL,NULL,NULL);
	else if(!strcasecmp(type, "speech"))
	  gen_do_trig(trig,rm,VARTYPE_ROOM,thing,NULL,NULL,NULL,NULL,arg,NULL);
	else if(!strcasecmp(type, "reset"))
	  gen_do_trig(trig,rm,VARTYPE_ROOM,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
	else if(!strcasecmp(type, "open"))
	  gen_do_trig(trig,rm,VARTYPE_ROOM,thing,NULL,NULL,arg,NULL,NULL,NULL);
	else {
	  log_string("Unrecognized trigger type %s attached to %s, uid %d.\r\n",
		     type, roomGetClass(rm), roomGetUID(rm));
	}
      } deleteListIterator(trig_i);
    }
    
    // clean up our mess
    deleteList(trigs);
  }
}



//*****************************************************************************
// trighooks
//*****************************************************************************
void do_give_trighooks(CHAR_DATA *ch, CHAR_DATA *recv, OBJ_DATA *obj) {
  do_char_trigs(ch,   "give",    recv, obj);
  do_char_trigs(recv, "receive", ch,   obj);
  do_obj_trigs (obj,  "give",    ch,  recv);
}

void do_get_trighooks(CHAR_DATA *ch, OBJ_DATA *obj) {
  do_obj_trigs (obj,             "get", ch, NULL);
  do_room_trigs(charGetRoom(ch), "get", ch, obj);
}

void do_drop_trighooks(CHAR_DATA *ch, OBJ_DATA *obj) {
  do_obj_trigs (obj,             "drop", ch, NULL);
  do_room_trigs(charGetRoom(ch), "drop", ch,  obj);
}

void do_enter_trighooks(CHAR_DATA *ch, ROOM_DATA *room) {
  LIST_ITERATOR *mob_i = newListIterator(roomGetCharacters(room));
  CHAR_DATA       *mob = NULL;
  ITERATE_LIST(mob, mob_i) {
    if(ch != mob)
      do_char_trigs(mob, "enter", ch, NULL);
  } deleteListIterator(mob_i);
  do_room_trigs(room, "enter", ch, NULL);
}

void do_exit_trighooks(CHAR_DATA *ch, ROOM_DATA *room, EXIT_DATA *exit) {
  LIST_ITERATOR *mob_i = newListIterator(roomGetCharacters(room));
  CHAR_DATA       *mob = NULL;
  ITERATE_LIST(mob, mob_i) {
    if(ch != mob)
      do_char_trigs(mob, "exit", ch, exit);
  } deleteListIterator(mob_i);
  do_room_trigs(room, "exit", ch,   exit);
  do_char_trigs(ch,   "move", exit, NULL);
}

void do_ask_trighooks(CHAR_DATA *ch, CHAR_DATA *listener, char *speech) {
  do_char_trigs(listener, "speech", ch, speech);
}

void do_say_trighooks(CHAR_DATA *ch, char *speech) {
  LIST_ITERATOR *mob_i = newListIterator(roomGetCharacters(charGetRoom(ch)));
  CHAR_DATA       *mob = NULL;
  ITERATE_LIST(mob, mob_i) {
    if(ch != mob)
      do_char_trigs(mob, "speech", ch, speech);
  } deleteListIterator(mob_i);
  do_room_trigs(charGetRoom(ch), "speech", ch, speech);
}

void do_greet_trighooks(CHAR_DATA *ch, CHAR_DATA *greeted) {
  do_char_trigs(greeted, "greet", ch, NULL);
}

void do_wear_trighooks(CHAR_DATA *ch, OBJ_DATA *obj) {
  do_char_trigs(ch,  "wear", obj, NULL);
  do_obj_trigs (obj, "wear", ch,  NULL);
}

void do_remove_trighooks(CHAR_DATA *ch, OBJ_DATA *obj) {
  do_char_trigs(ch,  "remove", obj, NULL);
  do_obj_trigs (obj, "remove", ch,  NULL);
}

void do_reset_trighooks(ZONE_DATA *zone) {
  LIST_ITERATOR *res_i = newListIterator(zoneGetResettable(zone));
  char           *name = NULL;
  const char   *locale = zoneGetKey(zone);
  ROOM_DATA      *room = NULL;
  ITERATE_LIST(name, res_i) {
    room = worldGetRoom(gameworld, get_fullkey(name, locale));
    if(room != NULL) do_room_trigs(room, "reset", NULL, NULL);
  } deleteListIterator(res_i);
}

void do_open_door_trighooks(CHAR_DATA *ch, EXIT_DATA *ex) {
  do_room_trigs(charGetRoom(ch), "open", ch, ex);
}

void do_open_obj_trighooks(CHAR_DATA *ch, OBJ_DATA *obj) {
  do_obj_trigs(obj, "open", ch, NULL);
}



//*****************************************************************************
// implementation of trighooks.h
//*****************************************************************************
void init_trighooks(void) {
  // add all of our hooks to the game
  hookAdd("give",      do_give_trighooks);
  hookAdd("get",       do_get_trighooks);
  hookAdd("drop",      do_drop_trighooks);
  hookAdd("enter",     do_enter_trighooks);
  hookAdd("exit",      do_exit_trighooks);
  hookAdd("ask",       do_ask_trighooks);
  hookAdd("say",       do_say_trighooks);
  hookAdd("greet",     do_greet_trighooks);
  hookAdd("wear",      do_wear_trighooks);
  hookAdd("remove",    do_remove_trighooks);
  hookAdd("reset",     do_reset_trighooks);
  hookAdd("open_door", do_open_door_trighooks);
  hookAdd("open_obj",  do_open_obj_trighooks);
}
