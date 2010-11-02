//*****************************************************************************
//
// character.c
//
// Basic implementation of the character datastructure (PCs and NPCs). Includes
// data for storing room, inventory, body, equipment, and other "essential"
// information. If you plan on adding any other information to characters, it
// is strongly suggested you do so through auxiliary data (see auxiliary.h).
//
// For a recap, IF YOU PLAN ON ADDING ANY OTHER INFORMATION TO CHARACTERS, IT
// IS STRONGLY SUGGESTED YOU DO SO THROUGH AUXILIARY DATA (see auxiliary.h).
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "body.h"
#include "races.h"
#include "handler.h"
#include "auxiliary.h"
#include "storage.h"
#include "room.h"
#include "character.h"


// mob UIDs (unique IDs) start at a million and go 
// up by one every time a new NPC is created
#define START_MOB_UID       1000000
int next_mob_uid  =   START_MOB_UID;

const char *sex_names[NUM_SEXES] = {
  "male",
  "female",
  "neutral"
};

const char *sexGetName(int sex) {
  return sex_names[sex];
}

int  sexGetNum(const char *sex) {
  int i;
  for(i = 0; i < NUM_SEXES; i++)
    if(!strcasecmp(sex, sex_names[i]))
      return i;
  return SEX_NONE;
}

struct pos_data {
  char *name;
  char *action_self;
  char *action_other;
};

const struct pos_data pos_info[NUM_POSITIONS] = {
  { "unconcious", "fall unconcious", "falls unconcious" },
  { "sleeping",   "sleep",           "sleeps"           },
  { "sitting",    "sit",             "sits"             },
  { "standing",   "stand",           "stands"           },
  { "flying",     "fly",             "flies"            }
};

const char *posGetName(int pos) {
  return pos_info[pos].name;
}

const char *posGetActionSelf(int pos) {
  return pos_info[pos].action_self;
}

const char *posGetActionOther(int pos) {
  return pos_info[pos].action_other;
}

int  posGetNum(const char *pos) {
  int i;
  for(i = 0; i < NUM_POSITIONS; i++)
    if(!strcasecmp(pos, pos_info[i].name))
      return i;
  return POS_NONE;
}

int poscmp(int pos1, int pos2) {
  // a little hack... assumes that they
  // are ordered in their definitions
  if(pos1 == pos2)     return 0;
  else if(pos1 < pos2) return -1;
  else                 return 1;
}


struct char_data {
  // data for PCs only
  char                 * password;
  room_vnum              loadroom;
  int                    imm_invis;

  // shared data for PCs and NPCs
  int                    uid;

  BODY_DATA            * body;
  char                 * race;
  //  int                    race;

  SOCKET_DATA          * socket;
  ROOM_DATA            * room;
  OBJ_DATA             * furniture;
  BUFFER               * desc;
  char                 * name;
  int                    level;
  int                    sex;
  int                    position;

  LIST                 * inventory;
  HASHTABLE            * auxiliary_data;
  BITVECTOR            * prfs;

  // data for NPCs only
  dialog_vnum            dialog;
  char                 * rdesc;
  char                 * multi_name;
  char                 * multi_rdesc;
  char                 * keywords;
  mob_vnum               vnum;  
};


CHAR_DATA *newChar() {
  CHAR_DATA *ch   = malloc(sizeof(CHAR_DATA));
  bzero(ch, sizeof(*ch));

  ch->password      = strdup("");
  ch->imm_invis     = 0;

  ch->loadroom      = NOWHERE;
  ch->uid           = NOBODY;

  ch->race          = strdup(raceDefault());//RACE_HUMAN;
  ch->body          = raceCreateBody(ch->race);
  ch->room          = NULL;
  ch->furniture     = NULL;
  ch->socket        = NULL;
  ch->desc          = newBuffer(1);
  ch->name          = strdup("");
  ch->level         = LEVEL_PLAYER;
  ch->sex           = SEX_NEUTRAL;
  ch->position      = POS_STANDING;
  ch->inventory     = newList();

  ch->rdesc         = strdup("");
  ch->keywords      = strdup("");
  ch->multi_rdesc   = strdup("");
  ch->multi_name    = strdup("");
  ch->dialog        = NOTHING;
  ch->vnum          = NOBODY;
  ch->prfs          = bitvectorInstanceOf("char_prfs");

  ch->auxiliary_data = newAuxiliaryData(AUXILIARY_TYPE_CHAR);

  return ch;
};



//*****************************************************************************
//
// utility functions
//
//*****************************************************************************
void charSetRdesc(CHAR_DATA *ch, const char *rdesc) {
  if(ch->rdesc) free(ch->rdesc);
  ch->rdesc =   strdup(rdesc ? rdesc : "");
}

void charSetMultiRdesc(CHAR_DATA *ch, const char *multi_rdesc) {
  if(ch->multi_rdesc) free(ch->multi_rdesc);
  ch->multi_rdesc =   strdup(multi_rdesc ? multi_rdesc : "");
}

void charSetMultiName(CHAR_DATA *ch, const char *multi_name) {
  if(ch->multi_name) free(ch->multi_name);
  ch->multi_name =   strdup(multi_name ? multi_name : "");
}

bool charIsNPC( CHAR_DATA *ch) {
  return (ch->uid >= START_MOB_UID);
}

bool charIsName( CHAR_DATA *ch, const char *name) {
  if(charIsNPC(ch))
    return is_keyword(ch->keywords, name, TRUE);
  else
    return !strncasecmp(ch->name, name, strlen(name));
}


//*****************************************************************************
//
// set and get functions
//
//*****************************************************************************
LIST        *charGetInventory ( CHAR_DATA *ch) {
  return ch->inventory;
};

SOCKET_DATA *charGetSocket    ( CHAR_DATA *ch) {
  return ch->socket;
};

ROOM_DATA   *charGetRoom      ( CHAR_DATA *ch) {
  return ch->room;
};

const char  *charGetName      ( CHAR_DATA *ch) {
  return ch->name;
};

const char  *charGetPassword  ( CHAR_DATA *ch) {
  return ch->password;
};

const char  *charGetDesc      ( CHAR_DATA *ch) {
  return bufferString(ch->desc);
}

BUFFER      *charGetDescBuffer( CHAR_DATA *ch) {
  return ch->desc;
}

const char  *charGetRdesc     ( CHAR_DATA *ch) {
  return ch->rdesc;
}

const char  *charGetMultiRdesc( CHAR_DATA *ch) {
  return ch->multi_rdesc;
}

const char  *charGetMultiName( CHAR_DATA *ch) {
  return ch->multi_name;
}

int          charGetLevel     ( CHAR_DATA *ch) {
  return ch->level;
};

int         charGetSex        ( CHAR_DATA *ch) {
  return ch->sex;
};


int         charGetPos        ( CHAR_DATA *ch) {
  return ch->position;
};

BODY_DATA   *charGetBody      ( CHAR_DATA *ch) {
  return ch->body;
}

const char  *charGetRace  ( CHAR_DATA *ch) {
  return ch->race;
}

int          charGetUID   ( CHAR_DATA *ch) {
  return ch->uid;
}

room_vnum    charGetLoadroom (CHAR_DATA *ch) {
  return ch->loadroom;
}

void *charGetAuxiliaryData(const CHAR_DATA *ch, const char *name) {
  return hashGet(ch->auxiliary_data, name);
}

OBJ_DATA *charGetFurniture(CHAR_DATA *ch) {
  return ch->furniture;
}

int charGetImmInvis(CHAR_DATA *ch) {
  return ch->imm_invis;
}

BITVECTOR *charGetPrfs(CHAR_DATA *ch) {
  return ch->prfs;
}

void         charSetSocket    ( CHAR_DATA *ch, SOCKET_DATA *socket) {
  ch->socket = socket;
};

void         charSetRoom      ( CHAR_DATA *ch, ROOM_DATA *room) {
  ch->room   = room;
};

void         charSetName      ( CHAR_DATA *ch, const char *name) {
  if(ch->name) free(ch->name);
  ch->name = strdup(name ? name : "");
};

void         charSetPassword  ( CHAR_DATA *ch, const char *password) {
  if(ch->password) free(ch->password);
  ch->password = strdup(password ? password : "");
};

void         charSetLevel     ( CHAR_DATA *ch, int level) {
  ch->level = level;
};

void         charSetSex       ( CHAR_DATA *ch, int sex) {
  ch->sex = sex;
};

void         charSetPos       ( CHAR_DATA *ch, int pos) {
  ch->position = pos;
};

void         charSetDesc      ( CHAR_DATA *ch, const char *desc) {
  bufferClear(ch->desc);
  bufferCat(ch->desc, (desc ? desc : ""));
};

void         charSetBody      ( CHAR_DATA *ch, BODY_DATA *body) {
  if(ch->body) {
    unequip_all(ch);
    deleteBody(ch->body);
  }
  ch->body = body;
}

void         charSetRace  (CHAR_DATA *ch, const char *race) {
  if(ch->race) free(ch->race);
  ch->race = strdup(race);
}

void         charSetUID(CHAR_DATA *ch, int uid) {
  ch->uid = uid;
}

void         charResetBody(CHAR_DATA *ch) {
  charSetBody(ch, raceCreateBody(ch->race));
}

void charSetLoadroom(CHAR_DATA *ch, room_vnum loadroom) {
  ch->loadroom = loadroom;
}

void charSetFurniture(CHAR_DATA *ch, OBJ_DATA *furniture) {
  ch->furniture = furniture;
}

void charSetImmInvis(CHAR_DATA *ch, int level) {
  ch->imm_invis = level;
}


//*****************************************************************************
//
// mob-specific functions
//
//*****************************************************************************
CHAR_DATA *newMobile() {
  CHAR_DATA *mob = newChar();
  mob->uid = next_mob_uid++;
  return mob;
};


void deleteChar( CHAR_DATA *mob) {
  // it is assumed we've already unequipped
  // all of the items that are on our body (extract_char)
  if(mob->body) deleteBody(mob->body);
  // it's also assumed we've extracted our inventory
  deleteList(mob->inventory);

  if(mob->password)    free(mob->password);
  if(mob->name)        free(mob->name);
  if(mob->desc)        deleteBuffer(mob->desc);
  if(mob->rdesc)       free(mob->rdesc);
  if(mob->multi_rdesc) free(mob->multi_rdesc);
  if(mob->multi_name)  free(mob->multi_name);
  if(mob->keywords)    free(mob->keywords);
  if(mob->prfs)        deleteBitvector(mob->prfs);
  deleteAuxiliaryData(mob->auxiliary_data);

  free(mob);
}


CHAR_DATA *charRead(STORAGE_SET *set) {
  CHAR_DATA *mob = newMobile();

  charSetVnum(mob,         read_int   (set, "vnum"));
  charSetName(mob,         read_string(set, "name"));
  charSetKeywords(mob,     read_string(set, "keywords"));
  charSetRdesc(mob,        read_string(set, "rdesc"));
  charSetDesc(mob,         read_string(set, "desc"));
  charSetMultiRdesc(mob,   read_string(set, "multirdesc"));
  charSetMultiName(mob,    read_string(set, "multiname"));
  charSetLevel(mob,        read_int   (set, "level"));
  charSetSex(mob,          read_int   (set, "sex"));
  charSetRace(mob,         read_string(set, "race"));
  charSetPassword(mob,     read_string(set, "password"));
  bitSet(mob->prfs,        read_string(set, "prfs"));

  // read in PC data
  if(*charGetPassword(mob)) {
    charSetImmInvis(mob,   read_int   (set, "imm_invis"));
    charSetUID(mob,        read_int   (set, "uid"));
    charSetLoadroom(mob,   read_int   (set, "loadroom"));
    charSetPos(mob,        read_int   (set, "position"));
  }
  // and NPC data
  else
    charSetDialog(mob,        read_int   (set, "dialog"));

  deleteAuxiliaryData(mob->auxiliary_data);
  mob->auxiliary_data = auxiliaryDataRead(read_set(set, "auxiliary"), 
					  AUXILIARY_TYPE_CHAR);

  // make sure our race is OK
  if(!isRace(mob->race)) {
    free(mob->race);
    mob->race = strdup(raceDefault());
  }

  // reset our body to the default for our race
  charResetBody(mob);

  return mob;
}


STORAGE_SET *charStore(CHAR_DATA *mob) {
  STORAGE_SET *set = new_storage_set();
  store_int   (set, "vnum",       mob->vnum);
  store_string(set, "name",       mob->name);
  store_string(set, "keywords",   mob->keywords);
  store_string(set, "rdesc",      mob->rdesc);
  store_string(set, "desc",       bufferString(mob->desc));
  store_string(set, "multirdesc", mob->multi_rdesc);
  store_string(set, "multiname",  mob->multi_name);
  store_int   (set, "level",      mob->level);
  store_int   (set, "sex",        mob->sex);
  store_string(set, "race",       mob->race);
  store_string(set, "prfs",       bitvectorGetBits(mob->prfs));

  // PC-only data
  if(!charIsNPC(mob)) {
    store_int   (set, "imm_invis",  mob->imm_invis);
    store_int   (set, "position",   mob->position);
    store_string(set, "password",   mob->password);
    store_int   (set, "uid",        mob->uid);
    store_int   (set, "loadroom",   roomGetVnum(charGetRoom(mob)));
  }
  // NPC-only data
  else
    store_int   (set, "dialog",     mob->dialog);

  store_set(set,"auxiliary", auxiliaryDataStore(mob->auxiliary_data));
  return set;
}


void charCopyTo( CHAR_DATA *from, CHAR_DATA *to) {
  charSetVnum    (to, charGetVnum(from));
  charSetKeywords(to, charGetKeywords(from));
  charSetDialog  (to, charGetDialog(from));

  charSetName       (to, charGetName(from));
  charSetDesc       (to, charGetDesc(from));
  charSetRdesc      (to, charGetRdesc(from));
  charSetMultiRdesc (to, charGetMultiRdesc(from));
  charSetMultiName  (to, charGetMultiName(from));
  charSetLevel      (to, charGetLevel(from));
  charSetSex        (to, charGetSex(from));
  charSetPos        (to, charGetPos(from));
  charSetRace       (to, charGetRace(from));
  charSetBody       (to, bodyCopy(charGetBody(from)));
  charSetImmInvis   (to, charGetImmInvis(from));
  bitvectorCopyTo   (from->prfs, to->prfs);

  auxiliaryDataCopyTo(from->auxiliary_data, to->auxiliary_data);
}

CHAR_DATA *charCopy( CHAR_DATA *mob) {
  // use newMobile() ... we want to create a new UID
  CHAR_DATA *newmob = newMobile();
  charCopyTo(mob, newmob);
  return newmob;
}


//*****************************************************************************
//
// mob set and get functions
//
//*****************************************************************************
void charSetKeywords(CHAR_DATA *ch, const char *keywords) {
  if(ch->keywords) free(ch->keywords);
  ch->keywords = strdup(keywords ? keywords : "");
}

void charSetVnum(CHAR_DATA *ch, mob_vnum vnum) {
  ch->vnum = vnum;
}

void charSetDialog(CHAR_DATA *ch, dialog_vnum vnum) {
  ch->dialog = vnum;
}

mob_vnum     charGetVnum       ( CHAR_DATA *ch) {
  return ch->vnum;
}

dialog_vnum  charGetDialog     ( CHAR_DATA *ch) {
  return ch->dialog;
}

const char  *charGetKeywords   ( CHAR_DATA *ch) {
  return ch->keywords;
}
