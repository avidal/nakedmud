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
#include "hooks.h"
#include "event.h"

typedef struct event_data EVENT_DATA;
LIST    *events = NULL;

struct event_data {
  void *owner;   // who is the lucky person who owns this event?
  void (*  on_complete)(void *owner, void *data, char *arg);
  bool (*  check_involvement)(void *thing, void *data);
  int   delay;   // how much longer until the event is complete?
  int   tot_time;// what is the total delay before the event fires?
  void *data;    // data for the event
  char *arg;     // an argument supplied to an event
  bool  requeue; // is the event requeue'd after it goes off?
};




//******************************************************************************
//
// a small test for delayed events ... proof of concept
//
//******************************************************************************
void devent_on_complete(CHAR_DATA *owner, void *data, char *arg) {
  communicate(owner, arg, COMM_GLOBAL);
}

bool check_devent_involvement(void *thing, void *data) {
  return (thing == data);
}

COMMAND(cmd_devent) {
  if(!*arg)
    send_to_char(ch, "What did you want to delay-chat?\r\n");
  else {
    start_event(ch,
		5 SECONDS,
		devent_on_complete, check_devent_involvement, 
		ch, arg);
  }
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
		     void *data, const char *arg, bool requeue) {
  EVENT_DATA *event        = malloc(sizeof(EVENT_DATA));
  event->owner             = owner;
  event->on_complete       = on_complete;
  event->check_involvement = check_involvement;
  event->delay             = delay;
  event->tot_time          = delay;
  event->data              = data;
  event->arg               = strdupsafe(arg);
  event->requeue           = requeue;
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

// the hook for interrupting events when something is removed from game
void interrupt_events_hook(void *thing, void *none1, void *none2) {
  interrupt_events_involving(thing);
}



//*****************************************************************************
// event list handling
//*****************************************************************************
void init_events() {
  events = newList();

  // add our proof of concept command
  add_cmd("devent", NULL, cmd_devent, POS_SLEEPING, POS_FLYING,
	  "admin", TRUE, FALSE);

  // make sure all events involving the object/char are cancelled when
  // either is extracted from the game
  hookAdd("obj_from_game",  interrupt_events_hook);
  hookAdd("char_from_game", interrupt_events_hook);
  hookAdd("room_from_game", interrupt_events_hook);
}

void interrupt_event(EVENT_DATA *event) {
  deleteEvent(event);
}

void interrupt_events_involving(void *thing) {
  LIST_ITERATOR *ev_i = newListIterator(events);
  EVENT_DATA   *event = NULL;

  ITERATE_LIST(event, ev_i) {
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
		 const char *arg) {
  listPut(events, newEvent(owner, delay, on_complete, check_involvement,
			   data, arg, FALSE));
}

void start_update(void *owner, 
		  int   delay,
		  void *on_complete,
		  void *check_involvement,
		  void *data,
		  const char *arg) {
  listPut(events, newEvent(owner, delay, on_complete, check_involvement,
			   data, arg, TRUE));
}

void pulse_events(int time) {
  LIST_ITERATOR *ev_i = newListIterator(events);
  EVENT_DATA   *event = NULL;

  // go over all of the events
  ITERATE_LIST(event, ev_i) {
    // decrement the delay
    event->delay -= time;
    // pop the character from the list, and run the event
    if(event->delay <= 0) {
      listRemove(events, event);
      run_event(event);
      // if we need to requeue, reset the timer 
      // and put us back in the list
      if(event->requeue) {
	event->delay = event->tot_time;
	listPut(events, event);
      }
      // otherwise, just delete the event
      else
	deleteEvent(event);
    }
  }
  deleteListIterator(ev_i);
}
