//*****************************************************************************
//
// room.c
//
// the basic implementation of the room data structure. If you plan on adding
// any other information to rooms, it is strongly suggested you do so through
// auxiliary data (see auxiliary.h)
//
// For a recap, IF YOU PLAN ON ADDING ANY OTHER INFORMATION TO ROOMS, IT
// IS STRONGLY SUGGESTED YOU DO SO THROUGH AUXILIARY DATA (see auxiliary.h).
//
//*****************************************************************************

#include "mud.h"
#include "exit.h"
#include "extra_descs.h"
#include "utils.h"
#include "handler.h"
#include "character.h"
#include "auxiliary.h"
#include "storage.h"
#include "room_reset.h"
#include "room.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/script.h"



// how many special exits do we expect to have?
// We should really be using a map instead of a hashtable
#define SPECIAL_EXIT_BUCKETS     2


struct room_data {
  int   vnum;              // what vnum are we?

  int         terrain;           // what kind of terrain do we have?
  char       *name;              // what is the name of our room?
  BUFFER     *desc;              // our description

  EDESC_SET  *edescs;            // the extra descriptions in the room
  EXIT_DATA **exits;             // the normal exists
  HASHTABLE  *special_exits;     // what other special exits do we have?

  LIST       *contents;          // what objects do we contain in the room?
  LIST       *characters;        // who is in our room?

  LIST       *reset;             // what are our reset instructions?
  HASHTABLE  *auxiliary_data;    // data modules have installed in us
};


//*****************************************************************************
//
// implementation of the room.h interface
//
//*****************************************************************************
ROOM_DATA *newRoom() {
  int i;

  ROOM_DATA *room = malloc(sizeof(ROOM_DATA));
  room->vnum = NOWHERE;

  room->name      = strdup("");
  room->desc      = newBuffer(1);


  room->terrain = TERRAIN_INDOORS;

  // create all of the exits
  room->exits = malloc(sizeof(EXIT_DATA *) * NUM_DIRS);
  for(i = 0; i < NUM_DIRS; i++)
    room->exits[i] = NULL;

  room->special_exits  = newHashtableSize(SPECIAL_EXIT_BUCKETS);
  room->auxiliary_data = newAuxiliaryData(AUXILIARY_TYPE_ROOM);

  room->edescs     = newEdescSet();
  room->contents   = newList();
  room->characters = newList();
  room->reset      = newList();

  return room;
};


void deleteRoom(ROOM_DATA *room) {
  int i;

  // delete contents
  deleteListWith(room->contents,   extract_obj);
  deleteListWith(room->characters, extract_mobile);
  deleteListWith(room->reset,      deleteReset);

  // delete extra descriptions
  if(room->edescs) deleteEdescSet(room->edescs);

  // delete the normal exits
  for(i = 0; i < NUM_DIRS; i++)
    if(room->exits[i] != NULL)
      deleteExit(room->exits[i]);
  free(room->exits);

  // delete the special exits
  if(room->special_exits) {
    HASH_ITERATOR *hash_i = newHashIterator(room->special_exits);
    EXIT_DATA *exit = NULL;

    for(;(exit=hashIteratorCurrentVal(hash_i)) != NULL;hashIteratorNext(hash_i))
      deleteExit(exit);

    deleteHashtable(room->special_exits);
    deleteHashIterator(hash_i);
  }

  // delete strings
  if(room->name)        free(room->name);
  if(room->desc)        deleteBuffer(room->desc);
  deleteAuxiliaryData(room->auxiliary_data);

  free(room);
};


STORAGE_SET *roomStore(ROOM_DATA *room) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "name",    room->name);
  store_string(set, "desc",    bufferString(room->desc));
  store_int   (set, "vnum",    room->vnum);
  store_int   (set, "terrain", room->terrain);
  store_set   (set, "edescs",  edescSetStore(room->edescs));

  STORAGE_SET_LIST *exits = new_storage_list();
  store_list(set, "exits", exits);

  // save all of the normal directions
  int i;
  for(i = 0; i < NUM_DIRS; i++) {
    if(room->exits[i]) {
      STORAGE_SET *exit = exitStore(room->exits[i]);
      store_string(exit, "direction", dirGetName(i));
      storage_list_put(exits, exit);
    }
  }

  // and now the special directions
  int num_special_exits = 0;
  const char **special_exits = roomGetExitNames(room, &num_special_exits);
  for(i = 0; i < num_special_exits; i++) {
    STORAGE_SET *exit = exitStore(roomGetExitSpecial(room, special_exits[i]));
    store_string(exit, "direction", special_exits[i]);
    storage_list_put(exits, exit);
  }
  if(special_exits) free(special_exits);

  store_list(set, "reset",     gen_store_list(room->reset, resetStore));
  store_set(set, "auxiliary",  auxiliaryDataStore(room->auxiliary_data));
  return set;
}


ROOM_DATA *roomRead(STORAGE_SET *set) {
  ROOM_DATA *room = newRoom();

  roomSetVnum(room,              read_int   (set, "vnum"));
  roomSetName(room,              read_string(set, "name"));
  roomSetDesc(room,              read_string(set, "desc"));
  roomSetTerrain(room,           read_int   (set, "terrain"));
  roomSetEdescs(room,  edescSetRead(read_set(set, "edescs")));

  STORAGE_SET_LIST *exits = read_list(set, "exits");
  STORAGE_SET       *exit = NULL;
  while( (exit = storage_list_next(exits)) != NULL) {
    EXIT_DATA   *ex = exitRead(exit);
    const char *dir = read_string(exit, "direction");
    if(dirGetNum(dir) != DIR_NONE)
      roomSetExit(room, dirGetNum(dir), ex);
    else
      roomSetExitSpecial(room, dir, ex);
  }

  deleteList(room->reset, deleteReset);
  room->reset = gen_read_list(read_list(set, "reset"), resetRead);

  deleteAuxiliaryData(room->auxiliary_data);
  room->auxiliary_data = auxiliaryDataRead(read_set(set, "auxiliary"), 
					   AUXILIARY_TYPE_ROOM);
  return room;
}



ROOM_DATA *roomCopy(ROOM_DATA *room) {
  ROOM_DATA *R = newRoom();
  roomCopyTo(room, R);
  return R;
}


void roomCopyTo(ROOM_DATA *from, ROOM_DATA *to) {
  int i, num_spec_exits;
  const char **spec_exits;

  // we just want to copy data ABOUT the room, and not stuff
  // contained in the particular instance (e.g. players, contents)

  roomSetVnum    (to, roomGetVnum(from));
  roomSetName    (to, roomGetName(from));
  roomSetDesc    (to, roomGetDesc(from));
  roomSetTerrain (to, roomGetTerrain(from));

  // set our edescs
  roomSetEdescs(to, edescSetCopy(from->edescs));

  // set our normal exits
  for(i = 0; i < NUM_DIRS; i++)
    roomSetExit(to, i, (roomGetExit(from, i)?
			exitCopy(roomGetExit(from, i)):NULL));


  // free the special exits of the <to> room
  spec_exits = roomGetExitNames(to, &num_spec_exits);
  for(i = 0; i < num_spec_exits; i++)
    roomSetExitSpecial(to, spec_exits[i], NULL);
  free(spec_exits);
  deleteHashtable(to->special_exits);
  to->special_exits = newHashtableSize(SPECIAL_EXIT_BUCKETS);


  // set the special exits of the <to> room
  spec_exits = roomGetExitNames(from, &num_spec_exits);
  for(i = 0; i < num_spec_exits; i++)
    roomSetExitSpecial(to, spec_exits[i], 
		       exitCopy(roomGetExitSpecial(from, spec_exits[i])));
  free(spec_exits);

  
  // delete all of our old reset data, and copy over the new stuff
  RESET_DATA *reset;
  while( (reset = listPop(to->reset)) != NULL)
    deleteReset(reset);
  deleteList(to->reset);
  to->reset = listCopyWith(from->reset, resetCopy);

  // copy all of our auxiliary data
  auxiliaryDataCopyTo(from->auxiliary_data, to->auxiliary_data);
}


void       roomDigExit        (ROOM_DATA *room, int dir, int to) {
  // we already have an exit in that direction... change the destination
  if(roomGetExit(room, dir))
    exitSetTo(roomGetExit(room, dir), to);
  // create a new exit
  else {
    EXIT_DATA *exit = newExit();
    exitSetTo(exit, to);
    roomSetExit(room, dir, exit);
  }
}

void roomDigExitSpecial (ROOM_DATA *room, const char *dir, int to) {
  // we already have an exit in that direction ... change the destination
  if(roomGetExitSpecial(room, dir))
    exitSetTo(roomGetExitSpecial(room, dir), to);
  // create a new exit
  else {
    EXIT_DATA *exit = newExit();
    exitSetTo(exit, to);
    roomSetExitSpecial(room, dir, exit);
  }
}

void roomReset(ROOM_DATA *room) {
  resetRunOn(room->reset, room, INITIATOR_ROOM);
  try_scripts(SCRIPT_TYPE_INIT,
	      room, SCRIPTOR_ROOM,
	      NULL, NULL, room, NULL, NULL, 0);
}


LIST       *roomGetResets      (const ROOM_DATA *room) {
  return room->reset;
}

void        roomRemoveReset    (ROOM_DATA *room, RESET_DATA *reset) {
  listRemove(room->reset, reset);
}

void        roomAddReset       (ROOM_DATA *room, RESET_DATA *reset) {
  listPut(room->reset, reset);
}


//*****************************************************************************
//
// add and remove functions
//
//*****************************************************************************
void       roomRemoveChar       (ROOM_DATA *room, const CHAR_DATA *ch) {
  listRemove(room->characters, ch);
};

void       roomRemoveObj        (ROOM_DATA *room, const OBJ_DATA *obj) {
  listRemove(room->contents, obj);
};

void       roomAddChar          (ROOM_DATA *room, CHAR_DATA *ch) {
  listPut(room->characters, ch);
};

void       roomAddObj           (ROOM_DATA *room, OBJ_DATA *obj) {
  listPut(room->contents, obj);
};


//*****************************************************************************
//
// get and set functions for rooms
//
//*****************************************************************************
LIST       *roomGetContents    (const ROOM_DATA *room) {
  return room->contents;
};

LIST       *roomGetCharacters  (const ROOM_DATA *room) {
  return room->characters;
};

int   roomGetVnum        (const ROOM_DATA *room) {
  return room->vnum;
};

const char *roomGetName        (const ROOM_DATA *room) {
  return room->name;
};

const char *roomGetDesc        (const ROOM_DATA *room) {
  return bufferString(room->desc);
};

BUFFER *roomGetDescBuffer(const ROOM_DATA *room) {
  return room->desc;
}

int         roomGetTerrain     (const ROOM_DATA *room) {
  return room->terrain;
};

EXIT_DATA  *roomGetExit        (const ROOM_DATA *room, int dir) {
  return room->exits[dir];
};

EXIT_DATA  *roomGetExitSpecial (const ROOM_DATA *room, const char *dir) {
  return hashGet(room->special_exits, dir);
};

const char      **roomGetExitNames   (const ROOM_DATA *room, int *num) {
  int i;

  *num = hashSize(room->special_exits);
  const char **names = malloc(sizeof(char *) * *num);
  HASH_ITERATOR *hash_i = newHashIterator(room->special_exits);

  for(i = 0; i < *num; i++, hashIteratorNext(hash_i))
    names[i] = hashIteratorCurrentKey(hash_i);
  deleteHashIterator(hash_i);

  return names;
};

EDESC_SET  *roomGetEdescs      (const ROOM_DATA *room) {
  return room->edescs;
}

const char *roomGetEdesc       (const ROOM_DATA *room, const char *keyword) {
  EDESC_DATA *edesc = edescSetGet(room->edescs, keyword);
  if(edesc) return edescSetGetDesc(edesc);
  else return NULL;
}

void *roomGetAuxiliaryData     (const ROOM_DATA *room, const char *name) {
  return hashGet(room->auxiliary_data, name);
}

void        roomSetEdescs      (ROOM_DATA *room, EDESC_SET *edescs) {
  if(room->edescs) deleteEdescSet(room->edescs);
  room->edescs = edescs;
}

void        roomSetVnum        (ROOM_DATA *room, int vnum) {
  room->vnum = vnum;
};

void        roomSetName        (ROOM_DATA *room, const char *name) {
  if(room->name) free(room->name);
  room->name = strdup(name ? name : "");
};

void        roomSetDesc (ROOM_DATA *room, const char *desc) {
  bufferClear(room->desc);
  bufferCat(room->desc, (desc ? desc : ""));
};

void        roomSetTerrain     (ROOM_DATA *room, int terrain_type) {
  room->terrain = terrain_type;
};

void        roomSetExit        (ROOM_DATA *room,int dir, EXIT_DATA *exit){
  if(room->exits[dir] != NULL)
    deleteExit(room->exits[dir]);
  room->exits[dir] = exit;
};

void roomSetExitSpecial (ROOM_DATA *room,const char *dir, EXIT_DATA *exit) {
  if(hashIn(room->special_exits, dir))
    deleteExit(hashRemove(room->special_exits, dir));
  if(exit)
    hashPut(room->special_exits, dir, exit);
};




//*****************************************************************************
//
// direction stuff
//
//*****************************************************************************
const char *dir_names[NUM_DIRS] = {
  "north",
  "east",
  "south",
  "west",
  "up",
  "down",
  "northeast",
  "southeast",
  "southwest",
  "northwest"
};

const char *dir_abbrevs[NUM_DIRS] = {
  "n", "e", "s", "w", "u", "d",
  "ne", "se", "sw", "nw"
};

const char *dirGetName(int dir) {
  return dir_names[dir];
};

const char *dirGetAbbrev(int dir) {
  return dir_abbrevs[dir];
};

int dirGetOpposite(int dir) {
  switch(dir) {
  case DIR_NORTH:        return DIR_SOUTH;
  case DIR_EAST:         return DIR_WEST;
  case DIR_SOUTH:        return DIR_NORTH;
  case DIR_WEST:         return DIR_EAST;
  case DIR_UP:           return DIR_DOWN;
  case DIR_DOWN:         return DIR_UP;
  case DIR_NORTHEAST:    return DIR_SOUTHWEST;
  case DIR_SOUTHEAST:    return DIR_NORTHWEST;
  case DIR_SOUTHWEST:    return DIR_NORTHEAST;
  case DIR_NORTHWEST:    return DIR_SOUTHEAST;
  default:               return DIR_NONE;
  };
};

int dirGetNum(const char *dir) {
  int i;
  for(i = 0; i < NUM_DIRS; i++)
    if(!strcasecmp(dir, dir_names[i]))
      return i;
  return DIR_NONE;
};

int dirGetAbbrevNum(const char *dir) {
  int i;
  for(i = 0; i < NUM_DIRS; i++)
    if(!strcasecmp(dir, dir_abbrevs[i]))
      return i;
  return DIR_NONE;
};


//*****************************************************************************
//
// terrain stuff
//
//*****************************************************************************
struct terrain_data {
  char   *name;
};

const struct terrain_data terrain_types[NUM_TERRAINS] = {
  { "Inside"        },
  { "City"          },
  { "Road"          },
  { "Alley"         },
  { "Bridge"        },
  { "Shallow Water" },
  { "Deep Water"    },
  { "Ocean"         },
  { "Underwater"    },
  { "Field"         },
  { "Plains"        },
  { "Meadow"        },
  { "Forest"        },
  { "Deep Forest"   },
  { "Hills"         },
  { "High Hills"    },
  { "Mountains"     },
  { "Swamp"         },
  { "Deep Swamp"    },
  { "Sand"          },
  { "Desert"        }, 
  { "Ice"           },
  { "Glacier"       },
  { "Cavern"        },
};


const char *terrainGetName(int terrain) {
  return terrain_types[terrain].name;
}

int terrainGetNum(const char *terrain) {
  int i;
  for(i = 0; i < NUM_TERRAINS; i++)
    if(!strcasecmp(terrain, terrain_types[i].name))
      return i;
  return TERRAIN_NONE;
}
