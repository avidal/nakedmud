//*****************************************************************************
//
// time.c
//
// A small module for handling time of day in the MUD.
//
//*****************************************************************************

#include <mud.h>
#include <utils.h>
#include <inform.h>
#include <character.h>
#include <room.h>
#include <event.h>
#include <storage.h>
#include <auxiliary.h>
#include "mudtime.h"


#define TIME_FILE   "../lib/misc/time"  // where do we keep time data?
#define TIME_UPDATE_DELAY    5 MINUTES  // how long is an in-game hour?
#define HOURS_PER_DAY               24  // how many hours are in a day?
#define NUM_MONTHS                   4  // how many months are in a year?
#define DAYS_PER_WEEK                3  // how many days are in a week?
#define USE_AMPM                  TRUE  // do we use the am/pm system?


struct month_data {
  char *name;             // what is the name of the month?
  int   num_days;         // how many days are in the month?
  int   morning_starts;   // what time does morning start at?
  int   afternoon_starts; // what time does the afternoon start at?
  int   evening_starts;   // what times does evening start at?
  int   night_starts;     // what time does the night start at?
};

struct day_data {
  char *name;
};

const struct month_data month_info[NUM_MONTHS] = {
  // month name                 days   morning   afternoon    evening   night
  { "the month of dawn",          30,        6,         12,        18,     22 },
  { "the month of light",         30,        4,         10,        19,     23 },
  { "the month of the moon",      30,        6,         12,        17,     21 },
  { "the month of twilight",      30,        5,         13,        16,     20 },
};

const struct day_data day_info[DAYS_PER_WEEK] = {
  { "the day of work" },
  { "the day of play" },
  { "the day of rest" },
};


int curr_hour         = 0; // what hour is it in the day?
int curr_day_of_week  = 0; // what day of the week is it?
int curr_day_of_month = 0; // what day is it in the month?
int curr_month        = 0; // what month is it in the year?
int curr_year         = 0; // what year is it?



//*****************************************************************************
//
// auxiliary data
//
// the time module gives rooms the ability to have different day and night
// descriptions. Below are the functions required for installing this.
//
//*****************************************************************************
typedef struct time_aux_data {
  char *night_desc;        // our description at night time
} TIME_AUX_DATA;

TIME_AUX_DATA *
newTimeAuxData() {
  TIME_AUX_DATA *data = malloc(sizeof(TIME_AUX_DATA));
  data->night_desc = strdup("");
  return data;
}

void
deleteTimeAuxData(TIME_AUX_DATA *data) {
  if(data->night_desc) free(data->night_desc);
  free(data);
}

void
timeAuxDataCopyTo(TIME_AUX_DATA *from, TIME_AUX_DATA *to) {
  if(to->night_desc) free(to->night_desc);
  to->night_desc = strdup(from->night_desc ? from->night_desc : "");
}

TIME_AUX_DATA *
timeAuxDataCopy(TIME_AUX_DATA *data) {
  TIME_AUX_DATA *newdata = newTimeAuxData();
  timeAuxDataCopyTo(data, newdata);
  return newdata;
}

STORAGE_SET *timeAuxDataStore(TIME_AUX_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "night_desc", data->night_desc, NULL);
  return set;
}

TIME_AUX_DATA *timeAuxDataRead(STORAGE_SET *set) {
  TIME_AUX_DATA *data = malloc(sizeof(TIME_AUX_DATA));
  data->night_desc = strdup(read_string(set, "night_desc"));
  return data;
}

const char *roomGetNightDesc(ROOM_DATA *room) {
  TIME_AUX_DATA *data = roomGetAuxiliaryData(room, "time_aux_data");
  return data->night_desc;
}

char **roomGetNightDescPtr(ROOM_DATA *room) {
  TIME_AUX_DATA *data = roomGetAuxiliaryData(room, "time_aux_data");
  return &(data->night_desc);
}

void roomSetNightDesc(ROOM_DATA *room, const char *desc) {
  TIME_AUX_DATA *data = roomGetAuxiliaryData(room, "time_aux_data");
  if(data->night_desc) free(data->night_desc);
  data->night_desc = strdup(desc ? desc : "");
}



//*****************************************************************************
//
// time handling functions
//
//*****************************************************************************

//
// Handle the hourly update of our times
//
void handle_time_update(void *self, void *data, char *arg) {
  curr_hour++;

  if(curr_hour >= HOURS_PER_DAY) {
    curr_hour = 0;
    curr_day_of_week++;
    curr_day_of_month++;
  }

  if(curr_day_of_week >= DAYS_PER_WEEK)
    curr_day_of_week = 0;

  if(curr_day_of_month >= month_info[curr_month].num_days) {
    curr_day_of_month = 0;
    curr_month++;
  }

  if(curr_month >= NUM_MONTHS) {
    curr_month = 0;
    curr_year++;
  }


  // check to see if we've rolled over to a new time of day
  if(curr_hour == month_info[curr_month].morning_starts)
    send_outdoors("The morning sun slowly pokes its head up over the eastern horizon.\r\n");
  if(curr_hour == month_info[curr_month].afternoon_starts)
    send_outdoors("The sun hangs high overhead as afternoon approaches.\r\n");
  if(curr_hour == month_info[curr_month].evening_starts)
    send_outdoors("The sun slowly begins its descent as evening approaches.\r\n");
  if(curr_hour == month_info[curr_month].night_starts)
    send_outdoors("The sky goes dark as the sun passes over the western horizon.\r\n");

  // save our time data to file
  STORAGE_SET *set = new_storage_set();
  store_int(set, "hour",         curr_hour,         NULL);
  store_int(set, "day_of_week",  curr_day_of_week,  NULL);
  store_int(set, "day_of_month", curr_day_of_month, NULL);
  store_int(set, "month",        curr_month,        NULL);
  store_int(set, "year",         curr_year,         NULL);
  storage_write(set, TIME_FILE);
  storage_close(set);

  // throw ourself back into the event queue
  start_event(NULL,
	      TIME_UPDATE_DELAY,
	      handle_time_update,
	      NULL, NULL, NULL);
}


void init_time() {
  STORAGE_SET *set  = storage_read(TIME_FILE);
  if(set != NULL) {
    curr_hour         = read_int(set, "hour");
    curr_day_of_week  = read_int(set, "day_of_week");
    curr_day_of_month = read_int(set, "day_of_month");
    curr_month        = read_int(set, "month");
    curr_year         = read_int(set, "year");
    storage_close(set);
  }
  else
    curr_hour = curr_day_of_week = curr_day_of_month = curr_month = curr_year = 0;


  // add the time command
  add_cmd("time", NULL, cmd_time, 0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, FALSE);

  // add night descriptions for rooms
  auxiliariesInstall("time_aux_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_ROOM,
				       newTimeAuxData, deleteTimeAuxData,
				       timeAuxDataCopyTo, timeAuxDataCopy,
				       timeAuxDataStore, timeAuxDataRead));

  // start our time updater
  start_event(NULL,
	      TIME_UPDATE_DELAY,
	      handle_time_update,
	      NULL, NULL, NULL);
}


const char *get_day() {
  return day_info[curr_day_of_week].name;
}


const char *get_month() {
  return month_info[curr_month].name;
}


int get_year() {
  return curr_year;
}


int get_hour() {
  return curr_hour;
}


bool is_morning() {
  return (curr_hour >= month_info[curr_month].morning_starts &&
	  curr_hour <  month_info[curr_month].afternoon_starts);
}


bool is_afternoon() {
  return (curr_hour >= month_info[curr_month].afternoon_starts &&
	  curr_hour <  month_info[curr_month].evening_starts);
}


bool is_evening() {
  return (curr_hour >= month_info[curr_month].evening_starts &&
	  curr_hour <  month_info[curr_month].night_starts);
}


bool is_night() {
  return (curr_hour >= month_info[curr_month].night_starts ||
	  curr_hour <  month_info[curr_month].morning_starts);
}


//
// is it am or pm?
//
bool is_am() {
  return (curr_hour < HOURS_PER_DAY/2);
}


//
// what time is it, am/pm - wise?
//
int ampm_hour() {
  int ampm_time = curr_hour - (is_am() ? 0 : HOURS_PER_DAY/2);
  if(ampm_time == 0) ampm_time = HOURS_PER_DAY/2;
  return ampm_time;
}


COMMAND(cmd_time) {
  send_to_char(ch,
	       "It is %d o'clock%s during the %s of %s,\r\n"
	       "which is the %d%s day of %s, year %d.\r\n",
	       (USE_AMPM ? ampm_hour() : curr_hour+1),
	       (USE_AMPM ? (is_am() ? " a.m" : " p.m") : ""),
	       (is_morning() ? "morning" :
		(is_afternoon() ? "afternoon" :
		 (is_evening() ? "evening" : "night"))),
	       day_info[curr_day_of_week].name,
	       curr_day_of_month+1, numth(curr_day_of_month+1),
	       month_info[curr_month].name, curr_year);
}
