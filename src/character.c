//*****************************************************************************
//
// character.c
//
// implementation of the character datastructure (PCs and NPCs)
//
//*****************************************************************************

#include "mud.h"
#include "socket.h"
#include "utils.h"
#include "body.h"
#include "log.h"
#include "races.h"
#include "handler.h"
#include "auxiliary.h"
#include "storage.h"
#include "room.h"
#include "character.h"


// probably good to increase this if your MUD relies heavily on spec_vars
#define SPEC_VAR_TABLE_SIZE      10

// mob UIDs (unique IDs) start at a million and go 
// up by one every time a new NPC is created
#define START_MOB_UID   (1000000)
int next_mob_uid  = START_MOB_UID;

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
  bitvector_t            prfs;  
  room_vnum              loadroom;

  // shared data for PCs and NPCs
  int                    uid;

  BODY_DATA            * body;
  int                    race;

  SOCKET_DATA          * socket;
  ROOM_DATA            * room;
  OBJ_DATA             * furniture;
  char                 * desc;
  char                 * name;
  int                    level;
  int                    sex;
  int                    position;

  LIST                 * inventory;
  HASHTABLE            * spec_vars;
  HASHTABLE            * aliases;
  HASHTABLE            * auxiliary_data;

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
  ch->prfs          = 0;

  ch->loadroom      = NOWHERE;
  ch->uid           = NOBODY;

  ch->race          = RACE_HUMAN;
  ch->body          = raceCreateBody(ch->race);
  ch->room          = NULL;
  ch->furniture     = NULL;
  ch->socket        = NULL;
  ch->desc          = strdup("");
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

  ch->spec_vars      = newHashtable(SPEC_VAR_TABLE_SIZE);
  ch->aliases        = newHashtable(10);
  ch->auxiliary_data = newAuxiliaryData(AUXILIARY_TYPE_CHAR);

  return ch;
};



//*****************************************************************************
//
// utility functions
//
//*****************************************************************************
void text_to_char(CHAR_DATA *ch, const char *txt)
{
  if (ch->socket && socketGetState(ch->socket) == STATE_PLAYING && txt && *txt){
    text_to_buffer(ch->socket, txt);
    ch->socket->bust_prompt = TRUE;
  }

  // if it's a PC or we are not in game, then
  // don't send the mesage to us
  if(!charIsNPC(ch))
    try_log(charGetName(ch), txt);
}

void send_to_char(CHAR_DATA *ch, const char *format, ...) {
  if(ch->socket && format && *format) {
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);
    text_to_char(ch, buf);
    return;
  }
};

void send_to_list(LIST *list, const char *format, ...) {
  if(format && *format) {
    // form the message
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);

    // send it out to everyone
    LIST_ITERATOR *list_i = newListIterator(list);
    CHAR_DATA *ch = NULL;
    ITERATE_LIST(ch, list_i)
      text_to_char(ch, buf);
    deleteListIterator(list_i);
  }
};

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

void         charSetBit       ( CHAR_DATA *ch, int field, int bit) {
  if(field == BITFIELD_PRFS)
    SET_BIT(ch->prfs, (1 << bit));
}

void         charRemoveBit    ( CHAR_DATA *ch, int field, int bit) {
  if(field == BITFIELD_PRFS)
    REMOVE_BIT(ch->prfs, (1 << bit));
}

void         charToggleBit    ( CHAR_DATA *ch, int field, int bit) {
  if(field == BITFIELD_PRFS)
    TOGGLE_BIT(ch->prfs, (1 << bit));
}

bool         charIsBitSet     ( CHAR_DATA *ch, int field, int bit) {
  if(field == BITFIELD_PRFS && IS_SET(ch->prfs, (1 << bit)))
    return TRUE;

  return FALSE;
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
  return ch->desc;
}

char       **charGetDescPtr   ( CHAR_DATA *ch) {
  return &(ch->desc);
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

bitvector_t  charGetBits      ( CHAR_DATA *ch, int field) {
  if(field == BITFIELD_PRFS)
    return ch->prfs;

  return 0;
}

BODY_DATA   *charGetBody      ( CHAR_DATA *ch) {
  return ch->body;
}

int          charGetRace  ( CHAR_DATA *ch) {
  return ch->race;
}

int          charGetUID   ( CHAR_DATA *ch) {
  return ch->uid;
}

room_vnum    charGetLoadroom (CHAR_DATA *ch) {
  return ch->loadroom;
}

HASHTABLE   *charGetVars  ( CHAR_DATA *ch) {
  return ch->spec_vars;
}

HASHTABLE   *charGetAliases  ( CHAR_DATA *ch) {
  return ch->aliases;
}

const char   *charGetAlias  ( CHAR_DATA *ch, const char *alias) {
  return hashGet(ch->aliases, alias);
}

void *charGetAuxiliaryData(const CHAR_DATA *ch, const char *name) {
  return hashGet(ch->auxiliary_data, name);
}

OBJ_DATA *charGetFurniture(CHAR_DATA *ch) {
  return ch->furniture;
}

int          charGetVarVal( CHAR_DATA *ch, const char *var) {
  return (int)hashGet(ch->spec_vars, var);
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
  if(ch->desc) free(ch->desc);
  ch->desc = strdup(desc ? desc : "");
};

void         charSetBody      ( CHAR_DATA *ch, BODY_DATA *body) {
  if(ch->body) {
    unequip_all(ch);
    deleteBody(ch->body);
  }
  ch->body = body;
}

void         charSetRace  (CHAR_DATA *ch, int race) {
  ch->race = race;
}

void         charSetUID(CHAR_DATA *ch, int uid) {
  ch->uid = uid;
}

void         charSetAlias  (CHAR_DATA *ch, const char *alias, const char *cmd){
  // pull out the last one
  char *oldcmd = hashRemove(ch->aliases, alias);
  if(oldcmd != NULL)
    free(oldcmd);
  // put in the new one if it exists
  if(cmd && *cmd)
    hashPut(ch->aliases, alias, strdup(cmd));
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

void charSetVar( CHAR_DATA *ch, const char *var, int val) {
  // make sure the varname has no spaces
  if(strstr(var, " ") != NULL) {
    log_string("ERROR: On %s, attempted to edit a variable with a space, '%s'",
	       charGetName(ch), var);
    return;
  }

  if(val == 0)
    hashRemove(ch->spec_vars, var);
  else
    hashPut(ch->spec_vars, var, (void *)val);
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
  if(mob->desc)        free(mob->desc);
  if(mob->rdesc)       free(mob->rdesc);
  if(mob->multi_rdesc) free(mob->multi_rdesc);
  if(mob->multi_name)  free(mob->multi_name);
  if(mob->keywords)    free(mob->keywords);
  deleteAuxiliaryData(mob->auxiliary_data);

  if(mob->spec_vars)   deleteHashtable(mob->spec_vars);
  if(mob->aliases) {
    HASH_ITERATOR *hash_i = newHashIterator(mob->aliases);
    const char     *alias = NULL;
    char             *cmd = NULL;
    while( (alias = hashIteratorCurrentKey(hash_i)) != NULL) {
      hashIteratorNext(hash_i);
      cmd = hashRemove(mob->aliases, alias);
      free(cmd);
    }
    deleteHashIterator(hash_i);
    deleteHashtable(mob->aliases);
  }
  free(mob);
}

STORAGE_SET *aliasStore(HASHTABLE *table) {
  STORAGE_SET *set       = new_storage_set();
  HASH_ITERATOR *hash_i  = newHashIterator(table);
  STORAGE_SET_LIST *list = new_storage_list();
  const char  *aliasname = NULL;

  store_list(set, "list", list, NULL);
  while( (aliasname = hashIteratorCurrentKey(hash_i)) != NULL) {
    STORAGE_SET *aliasset = new_storage_set();
    store_string(aliasset, "key", aliasname, NULL);
    store_string(aliasset, "val", hashIteratorCurrentVal(hash_i), NULL);
    hashIteratorNext(hash_i);
    storage_list_put(list, aliasset);
  }
  deleteHashIterator(hash_i);
  return set;
}

HASHTABLE *aliasRead(STORAGE_SET *set) {
  HASHTABLE *table       = newHashtable(10);
  STORAGE_SET_LIST *list = read_list(set, "list");
  STORAGE_SET *var       = NULL;

  while( (var = storage_list_next(list)) != NULL)
    hashPut(table, read_string(var, "key"), strdup(read_string(var, "val")));
  return table;
}


STORAGE_SET *variableStore(HASHTABLE *table) {
  STORAGE_SET *set       = new_storage_set();
  HASH_ITERATOR *hash_i  = newHashIterator(table);
  STORAGE_SET_LIST *list = new_storage_list();
  const char    *varname = NULL;

  store_list(set, "list", list, NULL);
  while( (varname = hashIteratorCurrentKey(hash_i)) != NULL) {
    STORAGE_SET *varset = new_storage_set();
    store_string(varset, "key", varname, NULL);
    store_int   (varset, "val", (int)hashIteratorCurrentVal(hash_i), NULL);
    hashIteratorNext(hash_i);
    storage_list_put(list, varset);
  }
  deleteHashIterator(hash_i);
  return set;
}

HASHTABLE *variableRead(STORAGE_SET *set) {
  HASHTABLE *table       = newHashtable(SPEC_VAR_TABLE_SIZE);
  STORAGE_SET_LIST *list = read_list(set, "list");
  STORAGE_SET *var       = NULL;

  while( (var = storage_list_next(list)) != NULL)
    hashPut(table, read_string(var, "key"), (void *)read_int(var, "val"));
  return table;
}


CHAR_DATA *charRead(STORAGE_SET *set) {
  CHAR_DATA *mob = newMobile();

  charSetVnum(mob,          read_int   (set, "vnum"));
  charSetName(mob,         read_string(set, "name"));
  charSetKeywords(mob,      read_string(set, "keywords"));
  charSetRdesc(mob,        read_string(set, "rdesc"));
  charSetDesc(mob,         read_string(set, "desc"));
  charSetMultiRdesc(mob,   read_string(set, "multirdesc"));
  charSetMultiName(mob,    read_string(set, "multiname"));
  charSetLevel(mob,        read_int   (set, "level"));
  charSetSex(mob,          read_int   (set, "sex"));
  charSetRace(mob,         read_int   (set, "race"));
  charSetPassword(mob,     read_string(set, "password"));

  // read in PC data
  if(*charGetPassword(mob)) {
    charSetUID(mob,        read_int   (set, "uid"));
    charSetLoadroom(mob,   read_int   (set, "loadroom"));
    charSetPos(mob,        read_int   (set, "position"));
    mob->prfs = parse_bits(read_string(set, "prfs"));
    deleteHashtable(mob->spec_vars);
    mob->spec_vars = variableRead(read_set(set, "variables"));
    deleteHashtable(mob->aliases);
    mob->aliases  = aliasRead(read_set(set, "aliases"));
  }
  // and NPC data
  else
    charSetDialog(mob,        read_int   (set, "dialog"));

  deleteAuxiliaryData(mob->auxiliary_data);
  mob->auxiliary_data = auxiliaryDataRead(read_set(set, "auxiliary"), 
					  AUXILIARY_TYPE_CHAR);
  return mob;
}


STORAGE_SET *charStore(CHAR_DATA *mob) {
  STORAGE_SET *set = new_storage_set();
  store_int   (set, "vnum",       mob->vnum,                 NULL);
  store_string(set, "name",       mob->name,                 NULL);
  store_string(set, "keywords",   mob->keywords,             NULL);
  store_string(set, "rdesc",      mob->rdesc,                NULL);
  store_string(set, "desc",       mob->desc,                 NULL);
  store_string(set, "multirdesc", mob->multi_rdesc,          NULL);
  store_string(set, "multiname",  mob->multi_name,           NULL);
  store_int   (set, "level",      mob->level,                NULL);
  store_int   (set, "sex",        mob->sex,                  NULL);
  store_int   (set, "race",       mob->race,                 NULL);

  // PC-only data
  if(!charIsNPC(mob)) {
    store_int   (set, "position",   mob->position,                 NULL);
    store_string(set, "prfs",       write_bits(mob->prfs),         NULL);
    store_string(set, "password",   mob->password,                 NULL);
    store_int   (set, "uid",        mob->uid,                      NULL);
    store_int   (set, "loadroom",   roomGetVnum(charGetRoom(mob)), NULL);
    store_set   (set, "variables",  variableStore(mob->spec_vars), NULL);
    store_set   (set, "aliases",    aliasStore(mob->aliases),      NULL);
  }
  // NPC-only data
  else
    store_int   (set, "dialog",     mob->dialog,               NULL);

  store_set(set,"auxiliary", auxiliaryDataStore(mob->auxiliary_data), NULL);
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
