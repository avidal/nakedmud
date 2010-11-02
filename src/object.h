#ifndef __OBJECT_H
#define __OBJECT_H
//*****************************************************************************
//
// object.h
//
// this is the main interface for working with objects (swords, staves, etc..)
// contains all of the functions for interacting with the datastructure.
//
//*****************************************************************************



OBJ_DATA    *newObj         ();
void         deleteObj      (OBJ_DATA *obj);

OBJ_DATA    *objRead        (STORAGE_SET *set);
STORAGE_SET *objStore       (OBJ_DATA *obj);

void         objCopyTo      (OBJ_DATA *from, OBJ_DATA *to);
OBJ_DATA    *objCopy        (OBJ_DATA *obj);

bool         objIsName      (OBJ_DATA *obj, const char *name);
void         objAddChar     (OBJ_DATA *obj, CHAR_DATA *ch);
void         objRemoveChar  (OBJ_DATA *obj, CHAR_DATA *ch);



//*****************************************************************************
//
// set and get functions
//
//*****************************************************************************
obj_vnum     objGetVnum      (OBJ_DATA *obj);
const char  *objGetName      (OBJ_DATA *obj);
const char  *objGetKeywords  (OBJ_DATA *obj);
const char  *objGetRdesc     (OBJ_DATA *obj);
const char  *objGetDesc      (OBJ_DATA *obj);
const char  *objGetMultiName (OBJ_DATA *obj);
const char  *objGetMultiRdesc(OBJ_DATA *obj);
EDESC_SET   *objGetEdescs    (OBJ_DATA *obj);
const char  *objGetEdesc     (OBJ_DATA *obj, const char *keyword);
char       **objGetDescPtr   (OBJ_DATA *obj);
CHAR_DATA   *objGetCarrier   (OBJ_DATA *obj);
CHAR_DATA   *objGetWearer    (OBJ_DATA *obj);
OBJ_DATA    *objGetContainer (OBJ_DATA *obj);
ROOM_DATA   *objGetRoom      (OBJ_DATA *obj);
LIST        *objGetContents  (OBJ_DATA *obj);
LIST        *objGetUsers     (OBJ_DATA *obj);
int          objGetType      (OBJ_DATA *obj);
int          objGetSubtype   (OBJ_DATA *obj);
int          objGetUID       (OBJ_DATA *obj);
int          objGetVal       (OBJ_DATA *obj, int num);
double       objGetWeight    (OBJ_DATA *obj);
double       objGetWeightRaw (OBJ_DATA *obj);
double       objGetCapacity  (OBJ_DATA *obj);
void        *objGetAuxiliaryData(const OBJ_DATA *obj, const char *name);

void         objSetVnum      (OBJ_DATA *obj, obj_vnum vnum);
void         objSetName      (OBJ_DATA *obj, const char *name);
void         objSetKeywords  (OBJ_DATA *obj, const char *keywords);
void         objSetRdesc     (OBJ_DATA *obj, const char *rdesc);
void         objSetDesc      (OBJ_DATA *obj, const char *desc);
void         objSetMultiName (OBJ_DATA *obj, const char *multi_name);
void         objSetMultiRdesc(OBJ_DATA *obj, const char *multi_rdesc);
void         objSetEdescs    (OBJ_DATA *obj, EDESC_SET *edesc);
void         objSetCarrier   (OBJ_DATA *obj, CHAR_DATA *ch);
void         objSetWearer    (OBJ_DATA *obj, CHAR_DATA *ch);
void         objSetContainer (OBJ_DATA *obj, OBJ_DATA  *cont);
void         objSetRoom      (OBJ_DATA *obj, ROOM_DATA *room);
void         objSetType      (OBJ_DATA *obj, int type);
void         objSetSubtype   (OBJ_DATA *obj, int subtype);
void         objSetVal       (OBJ_DATA *obj, int num, int val);
void         objSetWeightRaw (OBJ_DATA *obj, double weight);
void         objSetCapacity  (OBJ_DATA *obj, double capacity);

void         objToggleBit     (OBJ_DATA *obj, int field, int bit);
void         objSetBit        (OBJ_DATA *obj, int field, int bit);
void         objRemoveBit     (OBJ_DATA *obj, int field, int bit);
bool         objIsBitSet      (OBJ_DATA *obj, int field, int bit);
void         objPrintBits     (OBJ_DATA *obj, int field, char *buf);
const char  *objBitGetName    (int field, int bit);

//*****************************************************************************
//
// Bitfields and their bits
//
//*****************************************************************************
#define BITFIELD_OBJ              0
#define OBJ_NOTAKE                0  // (1 << 0)
#define NUM_OBJ_BITS              1

#define BITFIELD_WEAP             1
#define NUM_WEAP_BITS             0

#define BITFIELD_WORN             2
#define NUM_WORN_BITS             0



#endif // __OBJECT_H
