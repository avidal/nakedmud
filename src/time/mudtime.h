#ifndef __MUDTIME_H
#define __MUDTIME_H
//*****************************************************************************
//
// mudtime.h
//
// We can't use time.c, because it is a standard C header. A small module for
// handling time of day in the MUD.
//
//*****************************************************************************

//
// This must be put at the top of mud.h so the rest of the MUD knows that
// we've got the time module installed
// #define MODULE_TIME
//

//
// Set up the time module
//
void init_time();


//
// what day is it?
//
const char *get_day();


//
// what month is it?
//
const char *get_month();


//
// what hour is it
//
int get_hour();


//
// what year is it?
//
int get_year();


//
// What period of the day are we in?
//
bool is_morning();
bool is_afternoon();
bool is_evening();
bool is_night();


//
// Tell the character information about the time
// 
COMMAND(cmd_time);


//
// If we have the time module installed, there is the option of
// supplying a night description for the room, that will be shifted
// to when it becomes evening/night time.
//
const char *roomGetNightDesc   (ROOM_DATA *room);
char      **roomGetNightDescPtr(ROOM_DATA *room);
void        roomSetNightDesc   (ROOM_DATA *room, const char *desc);


#endif // __MUDTIME_H
