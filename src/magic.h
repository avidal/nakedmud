#ifndef __MAGIC_H
#define __MAGIC_H
//*****************************************************************************
//
// magic.h
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

#define MAG_ACTION_NONE       '\0' // used to signify that no action is taken

#define MAG_HAND_LEFT         'l'  // perform action with left hand
#define MAG_HAND_RIGHT        'r'  // perform action with right hand
#define MAG_HAND_BOTH         'b'  // perform action with both hands
#define MAG_HAND_CLAP         'c'  // special marker - clap hands together

#define MAG_ACTION_TAP        't'  // tap into a specified energy source
#define MAG_ACTION_TAP_NONE   'N'  // tap nothing (dismiss)
#define MAG_ACTION_TAP_FIRE   'F'  // tap fire
#define MAG_ACTION_TAP_WATER  'W'  // tap water
#define MAG_ACTION_TAP_EARTH  'E'  // tap earth
#define MAG_ACTION_TAP_AIR    'A'  // tap air

#define MAG_ACTION_DISMISS    'd'  // dismiss the magic being tapped in hand
#define MAG_ACTION_PUSH       'p'  // push hand outward
#define MAG_ACTION_ARC        'a'  // raise hand upwards
#define MAG_ACTION_LOWER      'l'  // lower hand downwards
#define MAG_ACTION_WIGGLE     'w'  // wiggle fingers
#define MAG_ACTION_SNAP       's'  // snap fingers
#define MAG_ACTION_PALM       'u'  // upraised palm
#define MAG_ACTION_FINGER     'f'  // pointed finger

#define MAG_TAP_NONE          'n'
#define MAG_TAP_FIRE          'f'  
#define MAG_TAP_WATER         'w'
#define MAG_TAP_EARTH         'e'  
#define MAG_TAP_AIR           'a'


//      EFFECT                                 HAND MOVEMENT      WHEN TAPPING
#define MAG_EFFECT_NONE               (-1)
#define MAG_EFFECT_FIREBOLT             0   // s-f                fire
#define MAG_EFFECT_ACIDBOLT             1   // s-f                water
#define MAG_EFFECT_STONESPEAR           2   // s-f                earth
#define MAG_EFFECT_SHOCK                3   // s-f                air


//
// initialize the magic system
//
void init_magic();


//
// Interrupt the character's magic info
//
void interrupt_magic(CHAR_DATA *ch);


//
// perform an action. If an effect results, then return which effect it is.
// Actions have a syntax to them which must be followed. It goes:
//   hand, action, element (if the action is to tap).
//
// More than one action can be performed at a time, if they were done on
// separate hands.
//
// Here are some example, valid actions:
//   lw                           wiggle left fingers
//   rtf                          tap into fire with the right hand
//   ba                           arc both hands upwards
//   lara                         arc both hands upwards
//   ltwru                        tap water with left hand, raise right palm up
//   ltwrd                        tap water w/ left hand, dismiss magic in right
//   ltwrtn                       same as above
//
//int do_magic_action(CHAR_DATA *ch, const char *action);



//
// Make the character perform the specified action on the victim
//
//void do_mag_effect(CHAR_DATA *ch, CHAR_DATA *vict, int effect);


//
// The entrypoint into the magic system
//
COMMAND(cmd_cast);

#endif // __MAGIC_H
