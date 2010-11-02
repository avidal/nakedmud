#ifndef __RACES_H
#define __RACES_H
//*****************************************************************************
//
// races.h
//
// contains all of the information assocciated with different races.
//
//*****************************************************************************


#define RACE_NONE           (-1)
#define RACE_HUMAN            0
#define RACE_ELF              1
#define RACE_DRAGON           2
#define NUM_RACES             3

//
// Create a copy of the bodytype assocciated with the race.
//
BODY_DATA *raceCreateBody(int race);

bool raceIsForPC(int race);
const char *raceGetName(int race);
const char *raceGetAbbrev(int race);
int raceGetNum(const char *race);
int raceGetAbbrevNum(const char *race);

#endif // __RACES_H
