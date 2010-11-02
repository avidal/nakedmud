#ifndef __INFORM_H
#define __INFORM_H
//*****************************************************************************
//
// inform.h
//
// These are all of the functions that are used for listing off processed
// information about things (e.g. room descriptions + contents, shopping
// lists, etc...)
//
//*****************************************************************************



//********************
// Use with message()
//********************
#define TO_ROOM 	(1 << 0) // everyone in the room of ch
#define TO_VICT		(1 << 1) // just to the victim
#define TO_NOTVICT	(1 << 2) // never send the message to vict
#define TO_CHAR		(1 << 3) // to the character
#define TO_NOTCHAR      (1 << 4) // never send the message to char
#define TO_WORLD        (1 << 5) // combines with flags above, but
                                 // changes scope to entire world

//
// Send a message out
//
// Converts the following symbols:
//  $c = ch name
//  $C = vict name
//  $m = him/her of char
//  $M = him/her of vict
//  $s = his/hers of char
//  $S = his/hers of vict
//  $e = he/she of char
//  $E = he/she of vict
//
//  $o = obj name
//  $O = vobj name
//  $a = a/an of obj
//  $A = a/an of vobj
//
void message(CHAR_DATA *ch,  CHAR_DATA *vict,
	     OBJ_DATA  *obj, OBJ_DATA  *vobj,
	     int hide_nosee, bitvector_t range, 
	     const char *format, ...);


//
// send a message to everyone outdoors
//
void  send_outdoors(const char *format, ...);


//
// send a message to a list of characters
//
void  send_to_list (LIST *list, const char *format, ...);


//
// send a message to a specific character
//
void  send_to_char (CHAR_DATA *ch, const char *format, ...);


//
// Shows a list of items to a character. If it's furniture that is being
// used, and show_used_furniture is FALSE, then skip the object.
//
void list_contents(CHAR_DATA *ch, LIST *contents, bool show_used_furniture,
		   const char *descriptor(OBJ_DATA *));


//
// Shows a list of characters to another character
// if the character is in the list, he is not shown to himself.
// If the character is using furniture, and show_furniture_users is FALSE,
// skip the character.
//
void list_chars(CHAR_DATA *ch, LIST *chars, bool show_furniture_users);


//
// list furniture in a special format that displays the people sitting
// on it as well. Skip all objects that do not have people sitting on/at them
//
void list_used_furniture(CHAR_DATA *ch, LIST *contents);

//
// Show the exits that the room has
//
void list_room_exits(CHAR_DATA *ch, ROOM_DATA *room);


//
// Shows the exit to the character
//
void look_at_exit(CHAR_DATA *ch, EXIT_DATA *exit);


//
// Shows the room to the character
//
void look_at_room(CHAR_DATA *ch, ROOM_DATA *room);


//
// Shows one character to another
//
void look_at_char(CHAR_DATA *ch, CHAR_DATA *vict);


//
// Shows an object to a person
//
void look_at_obj(CHAR_DATA *ch, OBJ_DATA *obj);


//
// Show a body to the character
//
void show_body(CHAR_DATA *ch, BODY_DATA *body);

#endif // __INFORM_H
