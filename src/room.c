//*****************************************************************************
//
// room.h
//
// the implementation of the room data structure, and its corresponding 
// functions
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


#ifdef MODULE_SCRIPTS
#include "modules/scripts/script.h"
#endif


// how many special exits do we expect to have?
// We should really be using a map instead of a hashtable
#define SPECIAL_EXIT_BUCKETS     2


struct room_data {
  room_vnum   vnum;              // what vnum are we?

  int   terrain;                 // what kind of terrain do we have?
  char *name;                    // what is the name of our room?
  char *desc;                    // our description

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
  room->desc      = strdup("");


  room->terrain = TERRAIN_INDOORS;

  // create all of the exits
  room->exits = malloc(sizeof(EXIT_DATA *) * NUM_DIRS);
  for(i = 0; i < NUM_DIRS; i++)
    room->exits[i] = NULL;

  room->special_exits  = newHashtable(SPECIAL_EXIT_BUCKETS);
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
  if(room->desc)        free(room->desc);
  deleteAuxiliaryData(room->auxiliary_data);

  free(room);
};


STORAGE_SET *roomStore(ROOM_DATA *room) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "name",    room->name,                  NULL);
  store_string(set, "desc",    room->desc,                  NULL);
  store_int   (set, "vnum",    room->vnum,                  NULL);
  store_int   (set, "terrain", room->terrain,               NULL);
  store_set   (set, "edescs",  edescSetStore(room->edescs), NULL);

  STORAGE_SET_LIST *exits = new_storage_list();
  store_list(set, "exits", exits, NULL);

  // save all of the normal directions
  int i;
  for(i = 0; i < NUM_DIRS; i++) {
    if(room->exits[i]) {
      STORAGE_SET *exit = exitStore(room->exits[i]);
      store_string(exit, "direction", dirGetName(i), NULL);
      storage_list_put(exits, exit);
    }
  }

  // and now the special directions
  int num_special_exits = 0;
  char **special_exits = roomGetExitNames(room, &num_special_exits);
  for(i = 0; i < num_special_exits; i++) {
    STORAGE_SET *exit = exitStore(roomGetExitSpecial(room, special_exits[i]));
    store_string(exit, "direction", special_exits[i], NULL);
    storage_list_put(exits, exit);
  }
  if(special_exits) free(special_exits);

  // and store our reset data
  store_list(set, "reset",     gen_store_list(room->reset, resetStore), NULL);

  store_set(set, "auxiliary",  auxiliaryDataStore(room->auxiliary_data), NULL);
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
  char **spec_exits;

  // we just want to copy data ABOUT the room, and not stuff
  // contained in the particular instance (e.g. players, contents)

  roomSetVnum    (to, roomGetVnum(from));
  roomSetName    (to, roomGetName(from));
  roomSetDesc    (to, roomGetDesc(from));
  roomSetTerrain (to, roomGetTerrain(from));

  // set our edescs
  roomSetEdescs(to, copyEdescSet(from->edescs));

  // set our normal exits
  for(i = 0; i < NUM_DIRS; i++)
    roomSetExit(to, i, (roomGetExit(from, i)?
			exitCopy(roomGetExit(from, i)):NULL));


  // free the special exits of the <to> room
  spec_exits = roomGetExitNames(to, &num_spec_exits);
  for(i = 0; i < num_spec_exits; i++) {
    roomSetExitSpecial(to, spec_exits[i], NULL);
    free(spec_exits[i]);
  }
  free(spec_exits);
  deleteHashtable(to->special_exits);
  to->special_exits = newHashtable(SPECIAL_EXIT_BUCKETS);


  // set the special exits of the <to> room
  spec_exits = roomGetExitNames(from, &num_spec_exits);
  for(i = 0; i < num_spec_exits; i++) {
    roomSetExitSpecial(to, spec_exits[i], 
		       exitCopy(roomGetExitSpecial(from, spec_exits[i])));
    free(spec_exits[i]);
  }
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


void       roomDigExit        (ROOM_DATA *room, int dir, room_vnum to) {
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

void roomDigExitSpecial (ROOM_DATA *room, const char *dir, room_vnum to) {
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
#ifdef MODULE_SCRIPTS
  try_scripts(SCRIPT_TYPE_INIT,
	      room, SCRIPTOR_ROOM,
	      NULL, NULL, room, NULL, NULL, NULL, 0);
#endif
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

room_vnum   roomGetVnum        (const ROOM_DATA *room) {
  return room->vnum;
};

const char *roomGetName        (const ROOM_DATA *room) {
  return room->name;
};

const char *roomGetDesc        (const ROOM_DATA *room) {
  return room->desc;
};

char      **roomGetDescPtr     (ROOM_DATA *room) {
  return &(room->desc);
};

int         roomGetTerrain     (const ROOM_DATA *room) {
  return room->terrain;
};

EXIT_DATA  *roomGetExit        (const ROOM_DATA *room, int dir) {
  return room->exits[dir];
};

EXIT_DATA  *roomGetExitSpecial (const ROOM_DATA *room, const char *dir) {
  return hashGet(room->special_exits, dir);
};

char      **roomGetExitNames   (const ROOM_DATA *room, int *num) {
  int i;

  *num = hashSize(room->special_exits);
  char **names = malloc(sizeof(char *) * *num);
  HASH_ITERATOR *hash_i = newHashIterator(room->special_exits);

  for(i = 0; i < *num; i++, hashIteratorNext(hash_i))
    names[i] = strdup(hashIteratorCurrentKey(hash_i));

  deleteHashIterator(hash_i);
  return names;
};

EDESC_SET  *roomGetEdescs      (const ROOM_DATA *room) {
  return room->edescs;
}

const char *roomGetEdesc       (const ROOM_DATA *room, const char *keyword) {
  EDESC_DATA *edesc = getEdesc(room->edescs, keyword);
  if(edesc) return getEdescDescription(edesc);
  else return NULL;
}

void *roomGetAuxiliaryData     (const ROOM_DATA *room, const char *name) {
  return hashGet(room->auxiliary_data, name);
}

void        roomSetEdescs      (ROOM_DATA *room, EDESC_SET *edescs) {
  if(room->edescs) deleteEdescSet(room->edescs);
  room->edescs = edescs;
}

void        roomSetVnum        (ROOM_DATA *room, room_vnum vnum) {
  room->vnum = vnum;
};

void        roomSetName        (ROOM_DATA *room, const char *name) {
  if(room->name) free(room->name);
  room->name = strdup(name ? name : "");
};

void        roomSetDesc (ROOM_DATA *room, const char *desc) {
  if(room->desc) free(room->desc);
  room->desc = strdup(desc ? desc : "");
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
  char   *default_rname;
  char   *default_rdesc;
  char   *map_symbol;
  double  move_difficulty;
  double  visibility;
};

const struct terrain_data terrain_types[NUM_TERRAINS] = {
  { "Inside",             
    "Inside", 
    "You are not outside. This desc should never be seen.\r\n",
    "{w.",      1.0,    1.0 },

  { "City",               
    "In the City", 
    "You are in a city. This desc should never be seen.\r\n",
    "{g.",      1.0,    0.6 },

  { "Road", 
    "On a Road", 
    "The road beneath your feet is quite well made, and looks like it is "
    "taken care of on a regular basis. The foundation is a thick mortar, and "
    "there are virtually no cracks in it. You see the occasionally band of "
    "travellers pass you by, or a merchant riding a wagon hitched up to a "
    "horse heading in the opposite direction as you.\r\n",
    "{w.",      1.0,    1.0 },  

  { "Bridge",             
    "On a Bridge", 
    "The bridge appears to be quite sturdy; its foundation is a solid stone, "
    "and it looks like it was crafted by master builders. To either side of "
    "the bridge, you see a road continuing on for quite some ways.\r\n",
    "{g#",      1.0,    1.0 },

  { "Shallow Water",       
    "In Shallow Water", 
    "The water is refreshingly cool on your skin, and it is shallow enough "
    "that you are able to wade in it without relative ease. Occasionally, you "
    "will see a glint of color under the water, which is probably a fish "
    "swimming by you.\r\n",
    "{B~",      0.5,    1.0 },

  { "Deep Water",    
    "In Deep Waters", 
    "The water here is much too deep to wade into. Ripples undulate around "
    "you on the water's surface, spreading out in a beautiful rippling "
    "pattern. Occasionally, you see a bird dive down into the water and "
    "snatch up a fish.\r\n",
    "{bw",      0.2,    1.0 },

  { "Ocean",
    "On The Ocean",
    "Water, water everywhere! On all sides, you are surrounded by an endless "
    "view of water. At least the water is calm; occasionally, small waves "
    "rock you back and forth, but other than that there is very little "
    "disturbance of the water. Occasionally, you see a group of sea birds "
    "flying overhead, or a blurry figure swimming slightly below the surface "
    "of the water.\r\n",
    "{bo",      0.1,    0.8 },

  { "Underwater",         
    "Underwater", 
    "Your vision is somewhat obscured by the water around you, but you manage "
    "to make out shapes of various water creatures around you; hoards of "
    "fish, sponges, and seaweed. Many more exotic creatures swim about as "
    "well, but you are hard pressed to name any of them.\r\n",
    "{bu",      0.1,    0.5 },

  { "Field",              
    "In a Field", 
    "You are in a field. Occasionally, you hear birds chirping, and "
    "you see the occasional one fly overhead. The wilderness is quite "
    "peaceful.\r\n",
    "{g\"",    1.0,     1.0 },

  { "Plains",             
    "On The Plains", 
    "You are on the open plains. Occasionally, You see the occasional hawk "
    "fly overhead, and you spot many small rodents as you walk. The plains "
    "are quite flat, and you can literally see for miles.\r\n",
    "#Yp",    1.0,      1.0 },

  { "Meadow",
    "In A Meadow",
    "The meadow is a sea of tall, dried yellowish grass. There is a slight "
    "breeze, and the grass is bent in the direction the wind is blowing. You "
    "see the occasional grasshopper leap past you as you make your way "
    "through the meadow. Birds circle in the air above - they look like tiny "
    "specks from where you are.\r\n",
    "#Ym",    1.0,      1.0 },

  { "Forest",             
    "In a Wooded Forest",
    "Trees surround you on all sides. Poplar, fur, birch - all kinds. "
    "The trees are not so thick that you have a hard time seeing, but "
    "your vision is definitely impeded slightly by the trees.\r\n",
    "{Gf",    0.8,      0.7 },

  { "Deep Forest",        
    "In a Deep Forest", 
    "Trees surround you on all sides. Poplar, fur, birch - all kinds. "
    "The trees get quite thick here, and it becomes rather hard to see "
    "anything that is not in your immediate vicinity.\r\n",
    "{gD",    0.5,      0.4 },

  { "Hills",              
    "On Rolling Hills", 
    "Small hills cover the ground here, in wavy patterns. The ground is "
    "fairly nondescript - dirt and grass. Occasionally, you spot a hole "
    "in the ground. You would guess it to be the home of some small rodent. "
    "\r\n",
    "{yh",    0.7,      0.8 },

  { "High Hills",
    "Amidst High Hills",
    "The hills here are steep and plentiful. They are scattered across the "
    "terrain like waves in the ocean. The ground here is rather hard, and "
    "very rough. You spot the occasional patch of grass, but for the most "
    "part, the ground is simply dirt. You spot holes in the ground every now "
    "and then. Occasionally, a small rodent pokes its head in one, and then "
    "scurries back into its shelter.\r\n",
    "{yH",   0.4,        0.5 },

  { "Mountains",         
    "In the Mountains", 
    "The mountain is quite rocky, and here and there you see patches of snow "
    "on the ground. Are visible here and there, but you notice they start to "
    "thin out as the mountain rises up.\r\n",
    "{w^",   0.1,        0.1 },

  { "Swamp",              
    "In a Swamp", 
    "The odor of the swamp is reminiscent of mold and rotting materials. "
    "The ground is quite moist, and occasionally large bubbles float up to "
    "the surface of it and pop, letting out rather noxious fumes. The air "
    "is also quite damp, and very warm. Occasionally, you will hear the cry "
    "of a bird off in the distance, or what sounds like twigs snapping.\r\n",
    "#rs",   0.3,        0.5 },

  { "Deep Swamp",
    "Deep in the Swamp",
    "The air here is humid, and a very musky smell lingers about. The ground "
    "here is quite moist - dirty, murky water rises up to your ankles. Your "
    "surroundings are ominously quiet, except for the occasional shrill cry "
    "of a bird off in the distance. Rotten pieces of wood float in the water "
    "beneath your feet. Some drift as if they are a live. But, of course, "
    "driftwood doesn't live. Or maybe that's not driftwood.\r\n",
    "#rS",   0.1,        0.3 },

  { "Sand",               
    "On Sand", 
    "The sand is a bright golden yellow, and have a very fine grain. You "
    "see the occasional rock sticking out of the sand at an odd angle, and "
    "a small crab here or there, scurrying along.\r\n",
    "{y\"",  0.8,        1.0 },

  { "Desert",
    "In A Desert",
    "Sand streches out around you for miles in every direction. The air is "
    "unbearably hot, and very dry. Your lips are getting more and more "
    "chapped with every passing moment. The sun beams down on you with its "
    "relentless gaze, burning your skin and making you slightly lightheaded. "
    "It is a wonder even the most hearty of animals can live out here.\r\n",
    "{yD",   0.7,        1.0 },

  { "Ice",
    "On Ice",
    "Ice needs a description.\r\n",
    "{CI",   0.5,        0.7 },

  { "Glacier",
    "Amidst Glaciers",
    "Glaciers need a description.\r\n",
    "{cG",   0.3,        0.3 },

  { "Cavern",
    "Within a Cavern",
    "Caverns need a description.\r\n",
    " ",   1.0,        0.7 }
};


const char *terrainGetName(int terrain) {
  return terrain_types[terrain].name;
}

const char *terrainGetDefaultRname(int terrain) {
  return terrain_types[terrain].default_rname;
}

const char *terrainGetDefaultRdesc(int terrain) {
  return terrain_types[terrain].default_rdesc;
}

const char *terrainGetMapSymbol(int terrain) {
  return terrain_types[terrain].map_symbol;
}

double      terrainGetVisibility(int terrain) {
  return terrain_types[terrain].visibility;
}

double      terrainGetMoveDifficulty(int terrain) {
  return terrain_types[terrain].move_difficulty;
}

int terrainGetNum(const char *terrain) {
  int i;
  for(i = 0; i < NUM_TERRAINS; i++)
    if(!strcasecmp(terrain, terrain_types[i].name))
      return i;
  return TERRAIN_NONE;
}

