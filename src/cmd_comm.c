//*****************************************************************************
//
// cmd_comm.c
//
// Various commands used in NakedMud(tm) for communicating with other
// characters, and NPCs.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "handler.h"
#include "inform.h"
#include "character.h"
#include "world.h"
#include "dialog.h"


// option modules
#ifdef MODULE_SCRIPTS
#include "scripts/script.h"
#endif


//
// cmd_ask is used to pose a question to another character. Mostly, this is
// intended to be used to carry on dialogs with NPCs. Ask has a local range
// (i.e. you can only ask people in the same room as you questions)
//   usage: ask [person] <about> [question]
//
//   examples:
//     ask bob about cats           ask bob about the topic, "cats"
//     ask jim can I have a salad?  ask jim if you can have a salad
//
COMMAND(cmd_ask)
{
  if(!arg || !*arg)
    send_to_char(ch, "Ask who what?\r\n");
  else {
    bool ask_about = FALSE;
    char name[MAX_BUFFER];
    arg = one_arg(arg, name);
    trim(arg);

    // skip the "about" part in "ask [person] about"
    if(!strncasecmp(arg, "about ", 6)) {
      ask_about = TRUE;
      arg = arg+6;
    }

    if(!*arg)
      send_to_char(ch, "What did you want to ask %s?\r\n", name);
    else {
      CHAR_DATA *tgt = generic_find(ch, name,
				    FIND_TYPE_CHAR,
				    FIND_SCOPE_ROOM | FIND_SCOPE_VISIBLE,
				    FALSE, NULL);

      if(tgt == NULL)
	send_to_char(ch, "Who were you trying to ask a question?\r\n");
      else if(tgt == ch)
	send_to_char(ch, "You have a nice conversation with yourself.\r\n");
      else {
	char other_buf[MAX_BUFFER];
	sprintf(other_buf, "{w$n asks you%s, '%s'{n",
		(ask_about ? " about" : ""), arg);
	message(ch, tgt, NULL, NULL, FALSE, TO_VICT, other_buf);	

	send_to_char(ch, "{wYou ask %s%s, '%s'{n\r\n", 
		     charGetName(tgt), (ask_about ? " about" : ""), arg);

#ifdef MODULE_SCRIPTS
	try_speech_script(ch, tgt, arg);
#endif	

	// see if the NPC has something to say in return
	if(!try_dialog(ch, tgt, arg))
	  message(ch, tgt, NULL, NULL, TRUE, TO_CHAR,
		  "{p$N shrugs at the question.");
      }
    }
  }
}


//
// cmd_tell sends a message to another character. Primarily intended for
// player-player communication. Players can tell other players things even
// if they are not in the same room.
//   usage: tell [person] [mesage]
//
//   examples:
//     tell luke I am your father
//
COMMAND(cmd_tell)
{
  if(!arg || !*arg)
    send_to_char(ch, "Tell who what?\r\n");
  else {
    char name[MAX_BUFFER];
    arg = one_arg(arg, name);
    trim(arg);
    if(!*arg)
      send_to_char(ch, "What did you want to tell %s?\r\n", name);
    else {
      CHAR_DATA *tgt = generic_find(ch, name,
				    FIND_TYPE_CHAR,
				    FIND_SCOPE_WORLD | FIND_SCOPE_VISIBLE,
				    FALSE, NULL);

      if(tgt == NULL)
	send_to_char(ch, "Who were you trying to talk to?\r\n");
      else if(tgt == ch)
	send_to_char(ch, "You have a nice conversation with yourself.\r\n");
      else {
	send_to_char(ch, "{rYou tell %s, '%s'{n\r\n", charGetName(tgt), arg);

	// if we're an NPC, make sure we do colored output
	char *color_arg = arg;
	if(charIsNPC(ch)) {
	  DIALOG_DATA *dialog = worldGetDialog(gameworld, charGetDialog(tgt));
	  if(dialog)
	    color_arg = tagResponses(dialog, arg, "{c", "{r");
	}

	char other_buf[MAX_BUFFER];
	sprintf(other_buf, "{r$n tells you, '%s'{n", arg);
	message(ch, tgt, NULL, NULL, FALSE, TO_VICT, other_buf);

	if(color_arg != arg) 
	  free(color_arg);
      }
    }
  }
}


//
// cmd_chat sends a global message to all of the players currently logged on.
//   usage: chat [message]
//
//   example:
//     chat hello, world!
//
COMMAND(cmd_chat) {
  if (!arg || !*arg) {
    send_to_char(ch, "Chat what?\n\r");
    return;
  }
  communicate(ch, arg, COMM_GLOBAL);
}


//
// cmd_say sends a message to everyone in the same room as you. Say, like ask,
// can trigger NPC dialogs.
//   usage: say [message]
//
//   example:
//     say hello, room!
//
COMMAND(cmd_say) {
  if (!*arg) {
    send_to_char(ch, "Say what?\n\r");
    return;
  }

  char *color_arg = arg;
  if(charIsNPC(ch)) {
    DIALOG_DATA *dialog = worldGetDialog(gameworld, charGetDialog(ch));
    if(dialog)
      color_arg = tagResponses(dialog, arg, "{c", "{y");
  }

  communicate(ch, color_arg, COMM_LOCAL);
  if(color_arg != arg) free(color_arg);
}


//
// NPCs with dialogs will often have something to say when you greet/approach
// then. cmd_greet is a way to get them talking.
//   usage: greet [person]
//
//   examples:
//     greet mayor
//
COMMAND(cmd_greet) {
  if(!arg || !*arg)
    send_to_char(ch, "Whom did you want to greet?\r\n");
  else {
    CHAR_DATA *tgt = generic_find(ch, arg,
				  FIND_TYPE_CHAR,
				  FIND_SCOPE_ROOM | FIND_SCOPE_VISIBLE,
				  FALSE, NULL);

    if(tgt == NULL)
      send_to_char(ch, "Who were you trying to greet?\r\n");
    else if(tgt == ch)
      send_to_char(ch, 
		   "You shake your left hand with your right hand and say, "
		   "'nice to meet you, self'\r\n");
    else {
      message(ch, tgt, NULL, NULL, FALSE, TO_CHAR, 
	      "{wYou greet $N.");
      message(ch, tgt, NULL, NULL, FALSE, TO_VICT, 
	      "{w$n greets you.");
      message(ch, tgt, NULL, NULL, FALSE, TO_ROOM | TO_NOTCHAR | TO_NOTVICT, 
	      "$n greets $N.");
      
      // see if the NPC has something to say in return
      if(charIsNPC(tgt)) {
	DIALOG_DATA *dialog = worldGetDialog(gameworld, charGetDialog(tgt));
	
	// Do we have something to say, back?
	if(dialog && *dialogGetGreet(dialog)) {
	  char *response = tagResponses(dialog,
					dialogGetGreet(dialog),
					"{c", "{p");
	  send_to_char(ch, "{p%s responds, '%s'\r\n",charGetName(tgt),response);
	  free(response);
	}

	else
	  send_to_char(ch, "{p%s does not have anything to say.\r\n",
		       charGetName(tgt));
      }
    }
  }
}


//
// Send a special text message to the room you are in. The message is preceded
// by your name, unless you put a $n somewhere in the text, in which case the
// $n is replaced by your name.
//   usage: emote [message]
//
//   examples:
//     emote does a little dance.
//     emote A gunshot sounds, and $n is laying on the ground, dead.
//
COMMAND(cmd_emote) {
  if(!arg || !*arg)
    send_to_char(ch, "Emote we must, but emote what?\r\n");
  else {
    char buf[MAX_BUFFER];

    // see if a $n is within the argument ... if there is, let the
    // person put his or her name were it's wanted. Otherwise, tag
    // it onto the front of the message
    if(strfind(arg, "$n"))
      strcpy(buf, arg);
    else
      sprintf(buf, "$n %s", arg);

    message(ch, NULL, NULL, NULL, FALSE, TO_ROOM, buf);
  }
}


//
// cmd_gemote is similar to emote, but it sends a global message
//
COMMAND(cmd_gemote) {
  if(!arg || !*arg)
    send_to_char(ch, "Gemote we must, but gemote what?\r\n");
  else {
    char buf[MAX_BUFFER];
    if(strfind(arg, "$n"))
      sprintf(buf, "{bGLOBAL:{c %s", arg);
    else
      sprintf(buf, "{bGLOBAL:{c $n %s", arg);

    message(ch, NULL, NULL, NULL, FALSE, TO_WORLD, buf);
  }
}
