//*****************************************************************************
//
// races.c
//
// contains all of the information assocciated with different races.
//
//*****************************************************************************

#include "mud.h"
#include "races.h"
#include "body.h"
#include "utils.h"

struct race_data {
  char *name;
  char *abbrev;
  int  bodytype;
  bool pc_race;
};

const struct race_data race_info[NUM_RACES] = {
  { "human", "hum",  BODYTYPE_HUMANOID, TRUE  },
  { "elf",   "elf",  BODYTYPE_HUMANOID, TRUE  },
  { "dragon", "drg", BODYTYPE_DRAGON,  FALSE  }
};

bool raceIsForPC(int race) {
  return race_info[race].pc_race;
}

BODY_DATA *raceCreateBody(int race) {
  return bodyCreate(race_info[race].bodytype);
}

const char *raceGetName(int race) {
  return race_info[race].name;
}

const char *raceGetAbbrev(int race) {
  return race_info[race].abbrev;
}

int raceGetNum(const char *race) {
  int i;
  for(i = 0; i < NUM_RACES; i++)
    if(!strcasecmp(race_info[i].name, race))
      return i;
  return RACE_NONE;
}

int raceGetAbbrevNum(const char *race) {
  int i;
  for(i = 0; i < NUM_RACES; i++)
    if(!strcasecmp(race_info[i].abbrev, race))
      return i;
  return RACE_NONE;
}
