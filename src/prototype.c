//*****************************************************************************
//
// prototype.c
//
// generic prototype datastructure for anything that can be generated with a
// python script (e.g. object, room, character). Supports inheritance from
// other prototypes.
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "prototype.h"
#include "storage.h"
#include "room.h"
#include "character.h"
#include "object.h"
#include "world.h"
#include "zone.h"
#include "handler.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/scripts.h"
#include "scripts/pychar.h"
#include "scripts/pyroom.h"
#include "scripts/pyobj.h"



//*****************************************************************************
// local datastructures, functions, and variables
//*****************************************************************************
struct prototype_data {
  char       *key;
  char   *parents;
  bool   abstract;
  BUFFER  *script;
};



//*****************************************************************************
// implementation of prototype.h
//*****************************************************************************
PROTO_DATA *newProto(void) {
  PROTO_DATA *data = malloc(sizeof(PROTO_DATA));
  data->key      = strdup("");
  data->parents  = strdup("");
  data->abstract = TRUE;
  data->script   = newBuffer(1);
  return data;
}

void deleteProto(PROTO_DATA *data) {
  if(data->key)     free(data->key);
  if(data->parents) free(data->parents);
  if(data->script)  deleteBuffer(data->script);
  free(data);
}

void protoCopyTo(PROTO_DATA *from, PROTO_DATA *to) {
  protoSetKey(to,      protoGetKey(from));
  protoSetParents(to,  protoGetParents(from));
  protoSetScript(to,   protoGetScript(from));
  protoSetAbstract(to, protoIsAbstract(from));
}

PROTO_DATA *protoCopy(PROTO_DATA *data) {
  PROTO_DATA *newproto = newProto();
  protoCopyTo(data, newproto);
  return newproto;
}

STORAGE_SET   *protoStore(PROTO_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "parents",  data->parents);
  store_bool  (set, "abstract", data->abstract);
  store_string(set, "script",   bufferString(data->script));
  return set;
}

PROTO_DATA *protoRead(STORAGE_SET *set) {
  PROTO_DATA *data = newProto();
  protoSetParents(data,  read_string(set, "parents"));
  protoSetAbstract(data, read_bool  (set, "abstract"));
  protoSetScript(data,   read_string(set, "script"));
  return data;
}

void protoSetKey(PROTO_DATA *data, const char *key) {
  if(data->key) free(data->key);
  data->key = strdupsafe(key);
}

void  protoSetParents(PROTO_DATA *data, const char *parents) {
  if(data->parents) free(data->parents);
  data->parents = strdupsafe(parents);
}

void   protoSetScript(PROTO_DATA *data, const char *script) {
  bufferClear(data->script);
  bufferCat(data->script, script);
}

void protoSetAbstract(PROTO_DATA *data, bool abstract) {
  data->abstract = abstract;
}

const char *protoGetKey(PROTO_DATA *data) {
  return data->key;
}

const char  *protoGetParents(PROTO_DATA *data) {
  return data->parents;
}

bool protoIsAbstract(PROTO_DATA *data) {
  return data->abstract;
}

const char   *protoGetScript(PROTO_DATA *data) {
  return bufferString(data->script);
}

BUFFER *protoGetScriptBuffer(PROTO_DATA *data) {
  return data->script;
}

bool protoRun(PROTO_DATA *proto, const char *type, void *pynewfunc, 
	      void *protoaddfunc, void *protoclassfunc, void *me) {
  // parse all of our parents
  int i, num_parents = 0;
  char     **parents = parse_keywords(proto->parents, &num_parents);
  bool    parents_ok = TRUE;
  for(i = 0; i < num_parents && parents_ok; i++) {
    PROTO_DATA *parent = NULL;
    // does our parent have a locale? If so, find it. If not, use ours
    int separator_pos = next_letter_in(parents[i], '@');
    if(separator_pos == -1)
      parent = worldGetType(gameworld, type, 
			    get_fullkey(parents[i],get_key_locale(proto->key)));
    else
      parent = worldGetType(gameworld, type, parents[i]);
    if(parent == NULL || !protoRun(parent, type, pynewfunc, protoaddfunc, 
				   protoclassfunc, me))
      parents_ok = FALSE;
    free(parents[i]);
  }

  // finish cleaning up our parent keys
  for(; i < num_parents; i++)
    free(parents[i]);
  free(parents);

  // did we encounter a problem w/ our parents?
  if(parents_ok == FALSE)
    return FALSE;

  // now, do us
  extern PyObject *newScriptDict();
  extern void      start_script(PyObject *dict, const char *script);
  PyObject *dict = restricted_script_dict();
  PyObject *pyme = ((PyObject *(*)(void *))pynewfunc)(me);
  if(protoaddfunc)
    ((void (*)(void *, const char *))protoaddfunc)(me, protoGetKey(proto));
  if(protoclassfunc)
    ((void (*)(void *, const char *))protoclassfunc)(me, protoGetKey(proto));

  PyDict_SetItemString(dict, "me", pyme);
  run_script(dict, bufferString(proto->script), 
	     get_key_locale(protoGetKey(proto)));
  Py_DECREF(dict);
  Py_DECREF(pyme);
  return last_script_ok();
}

CHAR_DATA *protoMobRun(PROTO_DATA *proto) {
  if(protoIsAbstract(proto))
    return NULL;
  CHAR_DATA *ch = newMobile();
  char_to_game(ch);
  if(!protoRun(proto, "mproto", newPyChar, charAddPrototype, charSetClass, ch)){
    extract_mobile(ch);
    ch = NULL;
  }
  return ch;
}

OBJ_DATA *protoObjRun(PROTO_DATA *proto) {
  if(protoIsAbstract(proto))
    return NULL;
  OBJ_DATA *obj = newObj();
  obj_to_game(obj);
  if(!protoRun(proto, "oproto", newPyObj, objAddPrototype, objSetClass, obj)) {
    extract_obj(obj);
    obj = NULL;
  }
  return obj;
}

ROOM_DATA *protoRoomRun(PROTO_DATA *proto) {
  if(protoIsAbstract(proto))
    return NULL;
  ROOM_DATA *room = newRoom();
  room_to_game(room);
  if(!protoRun(proto, "rproto", newPyRoom, roomAddPrototype,roomSetClass,room)){
    extract_room(room);
    room = NULL;
  }
  return room;
}
