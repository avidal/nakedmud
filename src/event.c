//*****************************************************************************
//
// event.h
//
// this is the interface for the event handler. Events are temporally
// delayed events. Events and actions (see action.h) may seem similar. They
// do, however, share some distinct differences. Whereas actions can only
// be attached to characters, and only one action can be attached to a 
// character at a time, events can be attached to anything, and anything can
// have any number of events attached to it at a time. And, indeed, they do
// not even need to be attached to anything! Some examples of what might
// constitute an event are things like a quest that is scheduled to start in
// 5 minutes, a disease that will kill someone in 5 minutes unless they find
// a cure for it, or perhaps a scheduled game reboot.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "character.h"
#include "event.h"

typedef struct event_data EVENT_DATA;
LIST    *events = NULL;

struct event_data {
  void *owner; // who is the lucky person who owns this event?
  void (*  on_complete)(void *owner, void *data, char *arg);
  bool (*  check_involvement)(void *thing, void *data);
  int   delay; // how much longer until the event is complete?
  void *data;  // data for the event
  char *arg;   // an argument supplied to an event
};




//******************************************************************************
//
// a small test for delayed events ... proof of concept
//
//******************************************************************************
struct devent_data {
  CHAR_DATA *speaker;
};

void devent_on_complete(CHAR_DATA *owner, struct devent_data *data, char *arg) {
  communicate(data->speaker, arg, COMM_GLOBAL);
  free(data);
}

bool check_devent_involvement(void *thing, struct devent_data *data) {
  if(thing == data->speaker)
    return TRUE;
  return FALSE;
}

COMMAND(cmd_devent) {
  struct devent_data *data = malloc(sizeof(struct devent_data));
  data->speaker = ch;
  start_event(ch,
	      5 SECONDS,
	      devent_on_complete, check_devent_involvement, 
	      data, arg);
}



//*****************************************************************************
//
// event handling
//
//*****************************************************************************
EVENT_DATA *newEvent(void *owner,
		     int delay, 	
		     void (*  on_complete)(void *owner, void *data, char *arg),
		     bool (* check_involvement)(void *thing, void *data),
		     void *data, char *arg) {
  EVENT_DATA *event        = malloc(sizeof(EVENT_DATA));
  event->owner             = owner;
  event->on_complete       = on_complete;
  event->check_involvement = check_involvement;
  event->delay             = delay;
  event->data              = data;
  event->arg               = strdup(arg ? arg :"");
  return event;
}

void deleteEvent(EVENT_DATA *event) {
  if(event->arg) free(event->arg);
  free(event);
}

void run_event(EVENT_DATA *event) {
  if(event->on_complete)
    event->on_complete(event->owner, event->data, event->arg);
}




//*****************************************************************************
//
// event list handling
//
//*****************************************************************************
void init_events() {
  events = newList();

  // add our proof of concept command
  add_cmd("devent", NULL, cmd_devent, 0, POS_SLEEPING, POS_FLYING,
	  LEVEL_ADMIN, FALSE);
}

void interrupt_event(EVENT_DATA *event) {
  deleteEvent(event);
}

void interrupt_events_involving(void *thing) {
  LIST_ITERATOR *ev_i = newListIterator(events);
  EVENT_DATA   *event = NULL;

  // we can't use ITERATE_LIST here, because there are times
  // where we will remove our current element  from the list, 
  // making it kinda hard to go onto the next one afterwards ;)
  while( (event = listIteratorCurrent(ev_i)) != NULL) {
    listIteratorNext(ev_i);

    // if we've found involvement, pop it on out
    if(event->owner == thing ||
       (event->check_involvement != NULL && 
	event->check_involvement(thing, event->data))) {
      listRemove(events, event);
      deleteEvent(event);
    }
  }
  deleteListIterator(ev_i);
}

void start_event(void *owner, 
		 int   delay,
		 void *on_complete,
		 void *check_involvement,
		 void *data,
		 char *arg) {
  listPut(events, newEvent(owner, delay, on_complete, check_involvement,
			   data, arg));
}

void pulse_events(int time) {
  LIST_ITERATOR *ev_i = newListIterator(events);
  EVENT_DATA   *event = NULL;

  // we can't use ITERATE_LIST here, because there are times
  // where we will remove our current element  from the list, 
  // making it kinda hard to go onto the next one afterwards ;)
  while( (event = listIteratorCurrent(ev_i)) != NULL) {
    listIteratorNext(ev_i);

    // decrement the delay
    event->delay -= time;
    // pop the character from the list, and run the event
    if(event->delay <= 0) {
      listRemove(events, event);
      run_event(event);
      deleteEvent(event);
    }
  }
  deleteListIterator(ev_i);
}
