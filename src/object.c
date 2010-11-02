//*****************************************************************************
//
// object.c
//
// this contains the basic implementation of the object data structure, and all
// of the functions needed to interact with it. If you plan on adding any other
// information to objects, it is strongly suggested you do so through auxiliary
// data (see auxiliary.h).
//
// For a recap, IF YOU PLAN ON ADDING ANY OTHER INFORMATION TO OBJECTS, IT
// IS STRONGLY SUGGESTED YOU DO SO THROUGH AUXILIARY DATA (see auxiliary.h).
//
//*****************************************************************************
#include "mud.h"
#include "extra_descs.h"
#include "utils.h"
#include "body.h"
#include "handler.h"
#include "storage.h"
#include "auxiliary.h"
#include "object.h"


// obj UIDs (unique IDs) start at a million and go 
// up by one every time a new object is created
int next_obj_uid = 1000000;


struct object_data {
  int vnum;                 // our number for builders
  int      uid;                  // our unique identifier
  double   weight;               // how much do we weigh, minus contents
  
  char *name;                    // our name - e.g. "a shirt"
  char *keywords;                // words to reference us by
  char *rdesc;                   // our room description
  char *multi_name;              // our name when more than 1 appears
  char *multi_rdesc;             // our rdesc when more than 1 appears
  BUFFER *desc;                  // the description when we are looked at
  BITVECTOR *bits;               // the object bits we have turned on

  // only one of these should be set at a time
  OBJ_DATA  *container;          // the thing we are in
  ROOM_DATA *room;               // the room we are in
  CHAR_DATA *carrier;            // who has us in their inventory 
  CHAR_DATA *wearer;             // who is wearing us

  LIST      *contents;           // other objects within us
  LIST      *users;              // the people using us (furniture and stuff)

  EDESC_SET  *edescs;            // special descriptions that can be seen on us

  HASHTABLE  *auxiliary_data;    // data modules have installed in us
};


OBJ_DATA *newObj() {
  OBJ_DATA *obj = calloc(1, sizeof(OBJ_DATA));
  obj->uid            = next_obj_uid++;
  obj->vnum           = NOTHING;

  obj->weight         = 0.1;

  obj->bits           = bitvectorInstanceOf("obj_bits");
  obj->name           = strdup("");
  obj->keywords       = strdup("");
  obj->rdesc          = strdup("");
  obj->multi_name     = strdup("");
  obj->multi_rdesc    = strdup("");
  obj->desc           = newBuffer(1);

  obj->contents       = newList();
  obj->users          = newList();

  obj->edescs         = newEdescSet();
  obj->auxiliary_data = newAuxiliaryData(AUXILIARY_TYPE_OBJ);

  return obj;
}


void deleteObj(OBJ_DATA *obj) {
  // don't remove the objects from this container.
  // it is assumed this has been done already (extract_obj)
  deleteList(obj->contents);
  // same goes for users
  deleteList(obj->users);

  if(obj->name)       free(obj->name);
  if(obj->keywords)   free(obj->keywords);
  if(obj->rdesc)      free(obj->rdesc);
  if(obj->desc)       deleteBuffer(obj->desc);
  if(obj->multi_name) free(obj->multi_name);
  if(obj->multi_rdesc)free(obj->multi_rdesc);
  if(obj->bits)     deleteBitvector(obj->bits);
  if(obj->edescs)   deleteEdescSet(obj->edescs);
  deleteAuxiliaryData(obj->auxiliary_data);

  free(obj);
}


OBJ_DATA *objRead(STORAGE_SET *set) {
  OBJ_DATA *obj = newObj();
  objSetVnum(obj,               read_int(set, "vnum"));
  objSetWeightRaw(obj,       read_double(set, "weight"));
  objSetName(obj,            read_string(set, "name"));
  objSetKeywords(obj,        read_string(set, "keywords"));
  objSetRdesc(obj,           read_string(set, "rdesc"));
  objSetDesc(obj,            read_string(set, "desc"));
  objSetMultiName(obj,       read_string(set, "multiname"));
  objSetMultiRdesc(obj,      read_string(set, "multirdesc"));
  objSetEdescs(obj,   edescSetRead(read_set(set, "edescs")));
  bitSet(obj->bits,read_string(set, "obj_bits"));
  deleteAuxiliaryData(obj->auxiliary_data);
  obj->auxiliary_data = auxiliaryDataRead(read_set(set, "auxiliary"), 
					  AUXILIARY_TYPE_OBJ);

  // parse all of our contents
  STORAGE_SET_LIST *contents = read_list(set, "contents");
  LIST *cont_list = gen_read_list(contents, objRead);
  OBJ_DATA *content = NULL;
  // make sure they're put into us
  while( (content = listPop(cont_list)) != NULL)
    obj_to_obj(content, obj);
  deleteList(cont_list);

  return obj;
}


STORAGE_SET *objStore(OBJ_DATA *obj) {
  STORAGE_SET *set = new_storage_set();
  store_int   (set, "vnum",      obj->vnum);
  store_double(set, "weight",    obj->weight);
  store_string(set, "name",      obj->name);
  store_string(set, "keywords",  obj->keywords);
  store_string(set, "rdesc",     obj->rdesc);
  store_string(set, "desc",      bufferString(obj->desc));
  store_string(set, "multiname", obj->multi_name);
  store_string(set, "multirdesc",obj->multi_rdesc);
  store_set   (set, "edescs",    edescSetStore(obj->edescs));
  store_string(set, "obj_bits",  bitvectorGetBits(obj->bits));
  store_set   (set, "auxiliary", auxiliaryDataStore(obj->auxiliary_data));
  store_list  (set, "contents",  gen_store_list(obj->contents, objStore));

  return set;
}


void objCopyTo(OBJ_DATA *from, OBJ_DATA *to) {
  objSetWeightRaw (to, objGetWeightRaw(from));
  objSetVnum      (to, objGetVnum(from));
  objSetName      (to, objGetName(from));
  objSetKeywords  (to, objGetKeywords(from));
  objSetRdesc     (to, objGetRdesc(from));
  objSetDesc      (to, objGetDesc(from));
  objSetMultiName (to, objGetMultiName(from));
  objSetMultiRdesc(to, objGetMultiRdesc(from));
  edescSetCopyTo  (objGetEdescs(from), objGetEdescs(to));
  bitvectorCopyTo (from->bits, to->bits);
  auxiliaryDataCopyTo(from->auxiliary_data, to->auxiliary_data);
}

OBJ_DATA *objCopy(OBJ_DATA *obj) {
  OBJ_DATA *newobj = newObj();
  objCopyTo(obj, newobj);
  return newobj;
}

bool objIsName(OBJ_DATA *obj, const char *name) {
  return is_keyword(obj->keywords, name, TRUE);
}


void objAddChar(OBJ_DATA *obj, CHAR_DATA *ch) {
  listPut(obj->users, ch);
}

void objRemoveChar(OBJ_DATA *obj, CHAR_DATA *ch) {
  listRemove(obj->users, ch);
}



//*****************************************************************************
//
// set and get functions
//
//*****************************************************************************
LIST *objGetContents(OBJ_DATA *obj) {
  return obj->contents;
}

LIST *objGetUsers(OBJ_DATA *obj) {
  return obj->users;
}

int objGetVnum(OBJ_DATA *obj) {
  return obj->vnum;
}

const char *objGetName(OBJ_DATA *obj) {
  return obj->name;
}

const char *objGetKeywords(OBJ_DATA *obj) {
  return obj->keywords;
}

const char  *objGetRdesc   (OBJ_DATA *obj) {
  return obj->rdesc;
}

const char  *objGetDesc    (OBJ_DATA *obj) {
  return bufferString(obj->desc);
}

const char  *objGetMultiName(OBJ_DATA *obj) {
  return obj->multi_name;
}

BUFFER *objGetDescBuffer(OBJ_DATA *obj) {
  return obj->desc;
}

const char  *objGetMultiRdesc(OBJ_DATA *obj) {
  return obj->multi_rdesc;
}

EDESC_SET *objGetEdescs(OBJ_DATA *obj) {
  return obj->edescs;
}

const char *objGetEdesc(OBJ_DATA *obj, const char *keyword) {
  EDESC_DATA *edesc = edescSetGet(obj->edescs, keyword);
  if(edesc) return edescSetGetDesc(edesc);
  else return NULL;
}

CHAR_DATA *objGetCarrier(OBJ_DATA *obj) {
  return obj->carrier;
}

CHAR_DATA *objGetWearer(OBJ_DATA *obj) {
  return obj->wearer;
}

OBJ_DATA *objGetContainer(OBJ_DATA *obj) {
  return obj->container;
}

ROOM_DATA *objGetRoom(OBJ_DATA *obj) {
  return obj->room;
}

int objGetUID(OBJ_DATA *obj) {
  return obj->uid;
}

double objGetWeightRaw(OBJ_DATA *obj) {
  return obj->weight;
}

double objGetWeight(OBJ_DATA *obj) {
  double tot_weight = obj->weight;
  if(listSize(obj->contents) > 0) {
    LIST_ITERATOR *cont_i = newListIterator(obj->contents);
    OBJ_DATA *cont = NULL;

    ITERATE_LIST(cont, cont_i)
      tot_weight += objGetWeight(cont);
    deleteListIterator(cont_i);
  }
  return tot_weight;
}

BITVECTOR *objGetBits(OBJ_DATA *obj) {
  return obj->bits;
}

void *objGetAuxiliaryData(const OBJ_DATA *obj, const char *name) {
  return hashGet(obj->auxiliary_data, name);
}

void objSetVnum(OBJ_DATA *obj, int vnum) {
  obj->vnum = vnum;
}

void objSetKeywords(OBJ_DATA *obj, const char *keywords) {
  if(obj->keywords) free(obj->keywords);
  obj->keywords = strdup(keywords ? keywords : "");
}

void objSetRdesc(OBJ_DATA *obj, const char *rdesc) {
  if(obj->rdesc) free(obj->rdesc);
  obj->rdesc = strdup(rdesc ? rdesc : "");
}

void objSetName(OBJ_DATA *obj, const char *name) {
  if(obj->name) free(obj->name);
  obj->name = strdup(name ? name : "");
}

void objSetDesc(OBJ_DATA *obj, const char *desc) {
  bufferClear(obj->desc);
  bufferCat(obj->desc, (desc ? desc : ""));
}

void objSetMultiName(OBJ_DATA *obj, const char *multi_name) {
  if(obj->multi_name) free(obj->multi_name);
  obj->multi_name = strdup(multi_name ? multi_name : "");
}

void objSetMultiRdesc(OBJ_DATA *obj, const char *multi_rdesc) {
  if(obj->multi_rdesc) free(obj->multi_rdesc);
  obj->multi_rdesc = strdup(multi_rdesc ? multi_rdesc : "");
}

void objSetEdescs(OBJ_DATA *obj, EDESC_SET *edescs) {
  if(obj->edescs) deleteEdescSet(obj->edescs);
  obj->edescs = edescs;
}

void objSetCarrier(OBJ_DATA *obj, CHAR_DATA *ch) {
  obj->carrier = ch;
}

void objSetWearer(OBJ_DATA *obj, CHAR_DATA *ch) {
  obj->wearer = ch;
}

void objSetContainer(OBJ_DATA *obj, OBJ_DATA  *cont) {
  obj->container = cont;
}

void objSetRoom(OBJ_DATA *obj, ROOM_DATA *room) {
  obj->room = room;
}

void objSetWeightRaw(OBJ_DATA *obj, double weight) {
  obj->weight = weight;
}
