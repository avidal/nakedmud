//*****************************************************************************
//
// magic.c
//
// A little magic system that has similarities to Richard Bartle's Spellbinder
// (http://www.mud.co.uk/richard/spellbnd.htm). Players chain gestures 
// together to make magical effects happen. The system is a little bit 
// different, in that players have to draw on power from different sources 
// (fire, water, earth, air) before they can cast spells. They can combine
// powers from different sources to make more elaborate powers (e.g. fire +
// earth = magma). Other differences exist; for instance, this magic system
// is realtime, whereas Bartle's is turn-based. Characters have to "spend"
// and "tap" magic power, whereas in Bartle's game, magic did not have a source
// one had to draw into, or magic power one had to spend.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "event.h"
#include "action.h"
#include "magic.h"
#include "character.h"   // send_to_char()
#include "inform.h"      // message()



//*****************************************************************************
//
// local data, structures, and functions
//
//*****************************************************************************

// how often do we check for mana increases and manaburn?
#define MAG_EVENT_DELAY      (5 SECONDS)
#define MAG_ACTION_DELAY      (1 SECOND)
#define MANABURN_MIN                  5


//
// Where all of our characters and their actions are stored
//
HASHMAP *mag_map = NULL;



//
// All of the info we need for keeping track of the magic
// status of a character.
//
typedef struct mag_data {
  char *lh_actions;  // the movements we've made with our left hand
  char *rh_actions;  // and with our right hand

  char lh_action;    // what action are we taking this time around?
  char rh_action;    // and in our right hand?

  char lh_tapping;   // what magic is our left hand tapping?
  char rh_tapping;   // what magic is our right hand tapping?

  int mana_fire;     // how much mana have we accumulated for various elements?
  int mana_water;    
  int mana_earth;
  int mana_air;

  int barrier_fire;  // how protected are we from fire damage?
  int barrier_water;
  int barrier_earth;
  int barrier_air;

} MAG_DATA;


//
// create a new magic entry
//
MAG_DATA *newMagic(void) {
  MAG_DATA *magic = malloc(sizeof(MAG_DATA));
  bzero(magic, sizeof(MAG_DATA));
  magic->lh_actions = strdup("");
  magic->rh_actions = strdup("");
  magic->lh_action  = MAG_ACTION_NONE;
  magic->rh_action  = MAG_ACTION_NONE;
  magic->lh_tapping = MAG_ACTION_NONE;
  magic->rh_tapping = MAG_ACTION_NONE;
  return magic;
}


//
// Delete a magic entry
//
void deleteMagic(MAG_DATA *magic) {
  if(magic->lh_actions) free(magic->lh_actions);
  if(magic->rh_actions) free(magic->rh_actions);
  free(magic);
}

typedef struct mag_action {
  char      action;
  int        delay;
  char    *ch_mssg;
  char *other_mssg;
} MAG_ACTION;


MAG_ACTION mag_actions[] = {
  { MAG_ACTION_PUSH,   1 SECOND,
    "You thrust your %s palm out in front of you",
    "$n thrusts $s %s palm outwards" },
  { MAG_ACTION_ARC,    1 SECOND,
    "You raise your %s hand up in a large arc",
    "$n arcs $s %s high above $s head" },
  { MAG_ACTION_LOWER,  1 SECOND,
    "You lower your %s hand",
    "$n lowers $s hand" },
  { MAG_ACTION_WIGGLE, 1 SECOND,
    "You wiggle the fingers on your %s hand",
    "$n begins wiggling the fingers on $s %s hand" },
  { MAG_ACTION_SNAP,   1 SECOND,
    "You snap the thumb and forefinger on your %s hand",
    "$n snaps the thumb and forefinger on $s %s hand" },
  { MAG_ACTION_PALM,   1 SECOND,
    "You slowly begin rising your %s hand, palm upwards",
    "$n slowly begins rising $s %s palm upwards" },
  { MAG_ACTION_FINGER, 1 SECOND,
    "You thrust the finger on your %s hand outwards",
    "$n thrusts the finger on $s %s hand outwards" },


  { MAG_ACTION_NONE, 0 SECONDS, "", "" } // sentinel
};


typedef struct mag_effect {
  int   effect;          // what effect does it do?
  int   cost;            // what's the cost in mana?
  char *hand_movement;   // what is the hand movement?
} MAG_EFFECT;


MAG_EFFECT mag_effects_fire[] = {
  { MAG_EFFECT_FIREBOLT,    2,           "sf" }, // snap, finger (point)
  { MAG_EFFECT_NONE,        0,           NULL }  // sentinel
};

MAG_EFFECT mag_effects_water[] = {
  { MAG_EFFECT_ACIDBOLT,    2,           "sf" }, // snap, finger (point)
  { MAG_EFFECT_NONE,        0,           NULL }  // sentinel
};

MAG_EFFECT mag_effects_earth[] = {
  { MAG_EFFECT_STONESPEAR,  2,           "sf" }, // snap, finger (point)
  { MAG_EFFECT_NONE,        0,           NULL }  // sentinel
};

MAG_EFFECT mag_effects_air[] = {
  { MAG_EFFECT_SHOCK,       2,           "sf" }, // snap, finger (point)
  { MAG_EFFECT_NONE,        0,           NULL }  // sentinel
};



//
// get the magic table associated with the mana that has been tapped
//
MAG_EFFECT *get_mag_table(MAG_DATA *magic) {
  if(magic->mana_fire > 0 && 
     magic->mana_water == 0 && magic->mana_earth == 0 && magic->mana_air == 0)
    return mag_effects_fire;
  if(magic->mana_water > 0 && 
     magic->mana_fire == 0 && magic->mana_earth == 0 && magic->mana_air == 0)
    return mag_effects_water;
  if(magic->mana_earth > 0 && 
     magic->mana_water == 0 && magic->mana_fire == 0 && magic->mana_air == 0)
    return mag_effects_earth;
  if(magic->mana_air > 0 && 
     magic->mana_water == 0 && magic->mana_earth == 0 && magic->mana_fire == 0)
    return mag_effects_air;

  return NULL;
}


//
// look in the table to see if we've completed any of the actions it has
//
int check_for_mag_effect(const MAG_EFFECT *mag_effects, 
			 const char *actions) {
  int i;
  int act_len = strlen(actions);

  // go through our list of effects, and search
  for(i = 0; mag_effects[i].effect != MAG_EFFECT_NONE; i++) {
    int move_len = strlen(mag_effects[i].hand_movement);

    // make sure we've performed enough actions
    if(act_len < move_len)
      continue;

    // go in reverse for both of them
    int j;
    char *movement = mag_effects[i].hand_movement;
    for(j = move_len-1; j >= 0; j--)
      if(movement[j] != actions[act_len-move_len+j])
	break;

    // if we got right to the end, we found a match
    if(j == -1)
      break;
  }

  return i;
}

const char *mag_hand_name(char hand) {
  return (hand == MAG_HAND_LEFT ? "left" :
	  (hand == MAG_HAND_RIGHT ? "right" : "BUG"));
}

const char *mag_tap_name(char tap_type) {
  return (tap_type == MAG_ACTION_TAP_FIRE ? "fire" :
	  (tap_type == MAG_ACTION_TAP_WATER ? "water" :
	   (tap_type == MAG_ACTION_TAP_EARTH ? "earth" :
	    (tap_type == MAG_ACTION_TAP_AIR ? "air" : "BUG"))));
}

const char *mag_tap_color(char tap_type) {
  return (tap_type == MAG_ACTION_TAP_FIRE ? "red" :
	  (tap_type == MAG_ACTION_TAP_WATER ? "blue" :
	   (tap_type == MAG_ACTION_TAP_EARTH ? "brown" :
	    (tap_type == MAG_ACTION_TAP_AIR ? "white" : "BUG"))));
}


void do_mag_effect(CHAR_DATA *ch, CHAR_DATA *vict, int effect) {
  switch(effect) {
  case MAG_EFFECT_FIREBOLT:
  case MAG_EFFECT_ACIDBOLT:
  case MAG_EFFECT_STONESPEAR:
  case MAG_EFFECT_SHOCK:
    send_to_char(ch, "You cast a basic spell at %s!\r\n", 
		 (vict ? charGetName(vict) : "nobody"));
    break;

    // nothing
  default: 
    break;
  }
}


//
// Inform the character (and room) that the character has lost his
// connection to a mana source
//
void magic_stop_message(CHAR_DATA *ch, char tap_type) {
  send_to_char(ch, "Your connection with the plane of %s is cut short.\r\n",
	       mag_tap_name(tap_type));
  message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR, 
	  "The %s aura around $n's body wanes.", mag_tap_color(tap_type));
}


//
// Inform the character (and room) that the character has begun tapping mana
//
void tap_start_message(CHAR_DATA *ch, char hand, char tap_type) {
  send_to_char(ch, 
	       "You begin tapping into the plane of %s with your %s hand.\r\n",
	       mag_tap_name(tap_type), mag_hand_name(hand));
  message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	  "$n upraises $s %s palm, and it becomes shrouded in a %s aura.",
	  mag_hand_name(hand), mag_tap_color(tap_type));
}


//
// Inform the character (and room) that the character has begun tapping mana
//
void tap_stop_message(CHAR_DATA *ch, char hand, char tap_type) {
  send_to_char(ch, 
	       "You stop tapping into the plane of %s with your %s hand.\r\n",
	       mag_tap_name(tap_type), mag_hand_name(hand));
  message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	  "$n lowers $s %s palm, and the %s aura around it dims slightly.",
	  mag_hand_name(hand), mag_tap_color(tap_type));
}


//
// return true if the action is mana-tapping-oriented
//
bool mag_action_is_tap(char action) {
  return (action == MAG_ACTION_TAP_FIRE  || action == MAG_ACTION_TAP_AIR   ||
	  action == MAG_ACTION_TAP_WATER || action == MAG_ACTION_TAP_EARTH);
}


//
// Set a character to start tapping in one hand
//
void start_tapping(CHAR_DATA *ch, MAG_DATA *magic, char hand, char action) {
  if(hand == MAG_HAND_LEFT) {
    magic->lh_tapping = action;
    tap_start_message(ch, hand, action);
  }
  else if(hand == MAG_HAND_RIGHT) {
    magic->rh_tapping = action;
    tap_start_message(ch, hand, action);
  }
}


//
// Stop a character tapping in one hand
//
void stop_tapping(CHAR_DATA *ch, MAG_DATA *magic, char hand) {
  if(hand == MAG_HAND_LEFT) {
    tap_stop_message(ch, hand, magic->lh_tapping);
    magic->lh_tapping = MAG_ACTION_NONE;
  }
  else if(hand == MAG_HAND_RIGHT) {
    tap_stop_message(ch, hand, magic->rh_tapping);
    magic->rh_tapping = MAG_ACTION_NONE;
  }
}


//
// Show the message that accompanies a hand gesture
//
void show_mag_action_message(CHAR_DATA *ch, char hand, char action) {
  int i;
  for(i = 0; mag_actions[i].action != MAG_ACTION_NONE; i++) {
    if(action == mag_actions[i].action) {
      message(ch, NULL, NULL, NULL, TRUE, TO_CHAR,
	      mag_actions[i].ch_mssg, mag_hand_name(hand));
      message(ch, NULL, NULL, NULL, TRUE, TO_ROOM | TO_NOTCHAR,
	      mag_actions[i].other_mssg, mag_hand_name(hand));
      break;
    }
  }
}



//*****************************************************************************
//
// implementation of magic.h
//
//*****************************************************************************

void init_magic() {
  // use standard hashing function and comparator
  mag_map = newHashmap(NULL, NULL, 50);
}


//
// Check if the magic is involved in an event. Basically, straight comparison
//
int check_mag_event_involvement(void *thing, MAG_DATA *magic) {
  return (thing == magic);
}


//
// Handle the completion of a magic action, and let everyone know
//
void handle_mag_action(CHAR_DATA *ch, MAG_DATA *magic, 
		       bitvector_t where, char *arg) {
  // do something about seeing if we're tapping mana
  if(mag_action_is_tap(magic->lh_action)) {
    start_tapping(ch, magic, MAG_HAND_LEFT, magic->lh_action);
    // now, free our history ... nothing starts with a tap
    free(magic->lh_actions); magic->lh_actions = strdup("");
  }
  if(mag_action_is_tap(magic->rh_action)) {
    start_tapping(ch, magic, MAG_HAND_RIGHT, magic->rh_action);
    // now, free our history ... nothing starts with a tap
    free(magic->rh_actions); magic->rh_actions = strdup("");
  }

  if(magic->lh_action != MAG_ACTION_NONE)
    show_mag_action_message(ch, MAG_HAND_LEFT,  magic->lh_action);
  if(magic->rh_action != MAG_ACTION_NONE)
    show_mag_action_message(ch, MAG_HAND_RIGHT, magic->rh_action);

  // based on the mana we've accumulated, figure out what table to use
  MAG_EFFECT *table = get_mag_table(magic);

  // now, see if we've created any magical effects
  if(table && magic->lh_action != MAG_ACTION_NONE) {
    int entry = check_for_mag_effect(table, magic->lh_actions);
    do_mag_effect(ch, NULL, table[entry].effect);
  }
  if(table && magic->rh_action != MAG_ACTION_NONE) {
    int entry = check_for_mag_effect(table, magic->rh_actions);
    do_mag_effect(ch, NULL, table[entry].effect);
  }

  magic->lh_action = MAG_ACTION_NONE;
  magic->rh_action = MAG_ACTION_NONE;
}


//
// Stop the action prematurely
//
void interrupt_mag_action(CHAR_DATA *ch, MAG_DATA *magic, 
			  bitvector_t where, char *arg) {
  if(magic->lh_action != MAG_ACTION_NONE) {
    send_to_char(ch, "You stop performing your left hand action.\r\n");
    magic->lh_action = MAG_ACTION_NONE;
  }

  if(magic->rh_action != MAG_ACTION_NONE) {
    send_to_char(ch, "You stop performing your right hand action.\r\n");
    magic->rh_action = MAG_ACTION_NONE;
  }
}


//
// Update the magic status (increasing mana, giving manaburn, etc...) and
// throw it back into the event handler
//
void handle_mag_event(CHAR_DATA *ch, MAG_DATA *magic, const char *arg) {
  // up our mana by 2 and decrease our mana reserves by 1
  // essentially, this results in +1 mana when we are actively tapping
  // a manasource (+3 if we are tapping with both hands) and -1 if we
  // are not tapping.
  magic->mana_fire  += 2*((magic->lh_tapping == MAG_ACTION_TAP_FIRE) + 
			  (magic->rh_tapping == MAG_ACTION_TAP_FIRE));
  magic->mana_water += 2*((magic->lh_tapping == MAG_ACTION_TAP_WATER) +
			  (magic->rh_tapping == MAG_ACTION_TAP_WATER));
  magic->mana_earth += 2*((magic->lh_tapping == MAG_ACTION_TAP_EARTH) +
			  (magic->rh_tapping == MAG_ACTION_TAP_EARTH));
  magic->mana_air   += 2*((magic->lh_tapping == MAG_ACTION_TAP_AIR) +
			  (magic->rh_tapping == MAG_ACTION_TAP_AIR));

  if(magic->mana_fire > 0) {
    magic->mana_fire--;
    if(magic->mana_fire == 0)
      magic_stop_message(ch, MAG_ACTION_TAP_FIRE);
  }

  if(magic->mana_water > 0) {
    magic->mana_water--;
    if(magic->mana_water == 0)
      magic_stop_message(ch, MAG_ACTION_TAP_WATER);
  }

  if(magic->mana_earth > 0) {
    magic->mana_earth--;
    if(magic->mana_water == 0)
      magic_stop_message(ch, MAG_ACTION_TAP_EARTH);
  }

  if(magic->mana_air > 0) {
    magic->mana_air--;
    if(magic->mana_air == 0)
      magic_stop_message(ch, MAG_ACTION_TAP_AIR);
  }


  // send increase messages
  if(magic->lh_tapping == MAG_ACTION_TAP_FIRE || magic->rh_tapping == MAG_ACTION_TAP_FIRE)
    send_to_char(ch, "You tap into a little bit of fire mana.\r\n");
  if(magic->lh_tapping == MAG_ACTION_TAP_WATER || magic->rh_tapping == MAG_ACTION_TAP_WATER)
    send_to_char(ch, "You tap into a little bit of water mana.\r\n");
  if(magic->lh_tapping == MAG_ACTION_TAP_EARTH || magic->rh_tapping == MAG_ACTION_TAP_EARTH)
    send_to_char(ch, "You tap into a little bit of earth mana.\r\n");
  if(magic->lh_tapping == MAG_ACTION_TAP_AIR || magic->rh_tapping == MAG_ACTION_TAP_AIR)
    send_to_char(ch, "You tap into a little bit of air mana.\r\n");


  // check for manaburn -> when people accumulate too much mana
  //***********
  // FINISH ME
  //***********


  // now, put it back in the event queue
  start_event(ch, MAG_EVENT_DELAY, 
	      handle_mag_event, check_mag_event_involvement, 
	      magic, NULL);
}


void interrupt_magic(CHAR_DATA *ch) {
  // see if the character has a magic entry.
  MAG_DATA *magic = mapGet(mag_map, ch);

  // if he does, interrupt all actions involving this magic entry
  if(magic) { 
    interrupt_events_involving(magic);

    // and let the character know
    if(magic->mana_fire > 0)
      magic_stop_message(ch, MAG_ACTION_TAP_FIRE);
    if(magic->mana_water > 0)
      magic_stop_message(ch, MAG_ACTION_TAP_WATER);
    if(magic->mana_earth > 0)
      magic_stop_message(ch, MAG_ACTION_TAP_EARTH);
    if(magic->mana_air > 0)
      magic_stop_message(ch, MAG_ACTION_TAP_AIR);

    // and finally, delete the magic entry
    deleteMagic(magic);
  }
}


void do_magic_action(CHAR_DATA *ch, CHAR_DATA *vict, const char *action) {
  char lh_action = MAG_ACTION_NONE;
  char rh_action = MAG_ACTION_NONE;
  bool lh_acting = FALSE; // have we made a lh/rh action this turn?
  bool rh_acting = FALSE;

  // first, look up our magic information
  MAG_DATA *magic = mapGet(mag_map, ch);

  // if one doesn't exist, create it and add it to our magic map
  // also toss it into the event queue
  if(magic == NULL) {
    magic = newMagic();
    mapPut(mag_map, ch, magic);
    start_event(ch, MAG_EVENT_DELAY, 
		handle_mag_event, check_mag_event_involvement, 
		magic, NULL);
  }

  // parse our action and add it to the magic data
  while(*action) {
    bool one_action_lh = FALSE;
    bool one_action_rh = FALSE;
    char act           = '\0';

    // first, parse the hand(s)
    if(*action == MAG_HAND_LEFT || *action == MAG_HAND_BOTH)
      one_action_lh = TRUE;
    if(*action == MAG_HAND_RIGHT || *action == MAG_HAND_BOTH)
      one_action_rh = TRUE;

    // check if the character has tried two actions with one hand
    if((one_action_lh && lh_acting) || (one_action_rh && rh_acting)) {
      send_to_char(ch,"You can only perform one action with a hand at a time!\r\n");
      return;
    }

    // we didn't parse a hand ... oops!
    if(!one_action_lh && !one_action_rh) {
      send_to_char(ch, "Your hands get all tied, and you fail to us your magic!\r\n");
      return;
    }
    
    action++;
    // now parse the action
    switch(*action) {
    case MAG_ACTION_TAP:       act = MAG_ACTION_TAP;       break;
    case MAG_ACTION_DISMISS:   act = MAG_ACTION_NONE;      break;
    case MAG_ACTION_PUSH:      act = MAG_ACTION_PUSH;      break;
    case MAG_ACTION_ARC:       act = MAG_ACTION_ARC;       break;
    case MAG_ACTION_LOWER:     act = MAG_ACTION_LOWER;     break;
    case MAG_ACTION_WIGGLE:    act = MAG_ACTION_WIGGLE;    break;
    case MAG_ACTION_SNAP:      act = MAG_ACTION_SNAP;      break;
    case MAG_ACTION_PALM:      act = MAG_ACTION_PALM;      break;
    case MAG_ACTION_FINGER:    act = MAG_ACTION_FINGER;    break;
    case MAG_ACTION_TAP_NONE:  act = MAG_ACTION_NONE;      break;
    case MAG_ACTION_TAP_FIRE:  act = MAG_ACTION_TAP_FIRE;  break;
    case MAG_ACTION_TAP_WATER: act = MAG_ACTION_TAP_WATER; break;
    case MAG_ACTION_TAP_EARTH: act = MAG_ACTION_TAP_EARTH; break;
    case MAG_ACTION_TAP_AIR:   act = MAG_ACTION_TAP_AIR;   break;
    default:
      send_to_char(ch, "You try to make a gesture, but it had no magical significance.\r\n");
      return;
    }
    action++;

    // if we're trying to tap something, figure out what action it is
    if(act == MAG_ACTION_TAP) {
      switch(*action) {
      case MAG_TAP_NONE:  act = MAG_ACTION_NONE;      break;
      case MAG_TAP_FIRE:  act = MAG_ACTION_TAP_FIRE;  break;
      case MAG_TAP_WATER: act = MAG_ACTION_TAP_WATER; break;
      case MAG_TAP_EARTH: act = MAG_ACTION_TAP_EARTH; break;
      case MAG_TAP_AIR:   act = MAG_ACTION_TAP_AIR;   break;
      default:
	send_to_char(ch, "You try to tap into a magic source, but fail!\r\n");
	break;
      }
      action++;
    }

    // now that we've got the action, pop it into the hand it belongs in
    // and note what hand we used to perform the action
    if(one_action_lh) {
      char *ptr = magic->lh_actions;
      magic->lh_actions = malloc(sizeof(char) * (strlen(ptr) + 1));
      sprintf(magic->lh_actions, "%s%c", ptr, act);
      free(ptr);
      lh_acting = TRUE;
      lh_action = act;
    }
    if(one_action_rh) {
      char *ptr = magic->rh_actions;
      magic->rh_actions = malloc(sizeof(char) * (strlen(ptr) + 1));
      sprintf(magic->rh_actions, "%s%c", ptr, act);
      free(ptr);
      rh_acting = TRUE;
      rh_action = act;
    }
  }

  // if we're using a hand that we were tapping with, stop the tapping
  if(lh_acting && magic->lh_tapping != MAG_ACTION_NONE)
    stop_tapping(ch, magic, MAG_HAND_LEFT);
  if(rh_acting && magic->rh_tapping != MAG_ACTION_NONE)
    stop_tapping(ch, magic, MAG_HAND_RIGHT);

  magic->lh_action = lh_action;
  magic->rh_action = rh_action;

  // set up an action in the action queue
  if(lh_action != MAG_ACTION_NONE || rh_action != MAG_ACTION_NONE) {
    send_to_char(ch, "Ok.\r\n");
    start_action(ch, MAG_ACTION_DELAY, 1/*ACTION_MENTAL*/, handle_mag_action, 
		 interrupt_mag_action, magic, NULL);
  }
}
