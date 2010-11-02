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
#include "inform.h"
#include "character.h"
#include "world.h"
#include "hooks.h"



//
// cmd_ask is used to pose a question to another character. Mostly, this is
// intended to be used to carry on dialogs with NPCs. Ask has a local range
// (i.e. you can only ask people in the same room as you questions)
//   usage: ask <person> [about] <question>
//
//   examples:
//     ask bob about cats           ask bob about the topic, "cats"
//     ask jim can I have a salad?  ask jim if you can have a salad
COMMAND(cmd_ask) {
  CHAR_DATA *tgt = NULL;
  char *question = NULL;

  if(!parse_args(ch, TRUE, cmd, arg, "ch.room.noself [about] string", 
		 &tgt, &question))
    return;

  mssgprintf(ch, tgt, NULL, NULL, FALSE, TO_VICT, 
	     "{w$n asks you, '%s'{n", question);
  mssgprintf(ch, tgt, NULL, NULL, FALSE, TO_CHAR,
	     "{wYou ask $N, '%s'{n", question);
  hookRun("ask", ch, tgt, arg);
}


//
// cmd_tell sends a message to another character. Primarily intended for
// player-player communication. Players can tell other players things even
// if they are not in the same room.
//   usage: tell <person> <mesage>
//
//   examples:
//     tell luke I am your father
COMMAND(cmd_tell) {
  CHAR_DATA *tgt = NULL;
  char     *mssg = NULL;

  if(!parse_args(ch, TRUE, cmd, arg, "ch.world.noself string", &tgt, &mssg))
    return;

  mssgprintf(ch, tgt, NULL, NULL, FALSE, TO_CHAR,
	     "{rYou tell $N, '%s'{n", mssg);
  mssgprintf(ch, tgt, NULL, NULL, FALSE, TO_VICT, 
	     "{r$n tells you, '%s'{n", mssg);
}


//
// cmd_chat sends a global message to all of the players currently logged on.
//   usage: chat <message>
//
//   example:
//     chat hello, world!
COMMAND(cmd_chat) {
  if (!arg || !*arg)
    send_to_char(ch, "Chat what?\n\r");
  else
    communicate(ch, arg, COMM_GLOBAL);
}


//
// cmd_say sends a message to everyone in the same room as you. Say, like ask,
// can trigger NPC dialogs.
//   usage: say <message>
//
//   example:
//     say hello, room!
COMMAND(cmd_say) {
  if (!*arg)
    send_to_char(ch, "Say what?\n\r");
  else {
    communicate(ch, arg, COMM_LOCAL);
    hookRun("say", ch, NULL, arg);
  }
}


//
// NPCs with dialogs will often have something to say when you greet/approach
// then. cmd_greet is a way to get them talking.
//   usage: greet <person>
//
//   examples:
//     greet mayor
COMMAND(cmd_greet) {
  CHAR_DATA *tgt = NULL;

  if(!parse_args(ch, TRUE, cmd, arg, "ch.room.noself", &tgt))
    return;

  message(ch, tgt, NULL, NULL, FALSE, TO_CHAR, "{wYou greet $N.");
  message(ch, tgt, NULL, NULL, FALSE, TO_VICT, "{w$n greets you.");
  message(ch, tgt, NULL, NULL, FALSE, TO_ROOM, "$n greets $N.");
  hookRun("greet", ch, tgt, NULL);
}


//
// Send a special text message to the room you are in. The message is preceded
// by your name, unless you put a $n somewhere in the text, in which case the
// $n is replaced by your name.
//   usage: emote <message>
//
//   examples:
//     emote does a little dance.
//     emote A gunshot sounds, and $n is laying on the ground, dead.
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

    message(ch, NULL, NULL, NULL, FALSE, TO_ROOM | TO_CHAR, buf);
  }
}


//
// cmd_gemote is similar to emote, but it sends a global message
COMMAND(cmd_gemote) {
  if(!arg || !*arg)
    send_to_char(ch, "Gemote we must, but gemote what?\r\n");
  else if(strfind(arg, "$n"))
    mssgprintf(ch, NULL, NULL, NULL, FALSE, TO_WORLD | TO_CHAR,
	       "{bGLOBAL:{c %s{n", arg);
  else 
    mssgprintf(ch, NULL, NULL, NULL, FALSE, TO_WORLD | TO_CHAR,
	       "{bGLOBAL:{c $n %s{n", arg);
}


//
// Send a message to another character, and also make it beep
COMMAND(cmd_page) {
  CHAR_DATA *tgt = NULL;
  char     *mssg = NULL;

  if(!parse_args(ch, TRUE, cmd, arg, "ch.world.noself string", &tgt, &mssg))
    return;

  send_to_char(ch,  "\007\007You page %s.\r\n", see_char_as(ch, tgt));
  send_to_char(tgt, "\007\007*%s* %s\r\n", see_char_as(tgt, ch), mssg);
}
