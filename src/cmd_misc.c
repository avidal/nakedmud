//*****************************************************************************
//
// cmd_misc.c
//
// a collection of miscellaneous commands that come with NakedMud(tm)
//
//*****************************************************************************
#include "mud.h"
#include "character.h"
#include "socket.h"
#include "utils.h"
#include "save.h"
#include "event.h"
#include "action.h"
#include "handler.h"


//
// stop performing the character's current action
//
COMMAND(cmd_stop) {
#ifdef MODULE_FACULTY
  if(!is_acting(ch, FACULTY_ALL))
    send_to_char(ch, "But you're not currently performing an action!\r\n");
  else
    interrupt_action(ch, FACULTY_ALL);
#else
  if(!is_acting(ch, 1))
    send_to_char(ch, "But you're not currently performing an action!\r\n");
  else
    interrupt_action(ch, 1);
#endif
}


//
// clear the screen
//
COMMAND(cmd_clear) {
  send_to_char(ch, "\033[H\033[J");
}


//
// quit the game
//
COMMAND(cmd_quit)
{
  char buf[MAX_BUFFER];

  /* log the attempt */
  sprintf(buf, "%s has left the game.", charGetName(ch));
  log_string(buf);

  save_player(ch);

  // 
  // gotta make sure we have a socket. Who knows...
  // a mobile might be trying to quit
  //
  if(charGetSocket(ch)) {
    SOCKET_DATA *sock = charGetSocket(ch);
    charSetSocket(ch, NULL);
    socketSetChar(sock, NULL);
    close_socket(sock, FALSE);
  }

  extract_mobile(ch);
}


//
// save the character
//
COMMAND(cmd_save)
{
  save_player(ch);
  text_to_char(ch, "Saved.\r\n");
}


//
// the function for executing a delayed command
//
void event_delayed_cmd(CHAR_DATA *ch, void *data, char *cmd) {
  do_cmd(ch, cmd, TRUE, TRUE);
}


//
// Perform a command, but delay its execution by a couple seconds
//
COMMAND(cmd_delay) {
  if(!arg || !*arg) {
    send_to_char(ch, "What did you want to delay, and by how long?\r\n");
    return;
  }

  // how long should we delay, in seconds?
  char time[SMALL_BUFFER];
  arg = one_arg(arg, time);

  // no command to delay
  if(!*arg) {
    send_to_char(ch, "What command did you want to delay?\r\n");
    return;
  }
  if(atoi(time) < 1) {
    send_to_char(ch, "You can only delay commands for positive amounts of time.\r\n");
    return;
  }

  send_to_char(ch, "You delay '%s' for %d seconds.\r\n", arg, atoi(time));
  start_event(ch, atoi(time) SECONDS, event_delayed_cmd, NULL, NULL, arg);
}


//
// An entrypoint into the character's notepad
//
/*
COMMAND(cmd_write) {
  if(!charGetSocket(ch))
    send_to_char(ch, "You cannot use notepad if you have no socket.\r\n");
  else {
    send_around_char(ch, TRUE, "%s pulls out a notepad and begins writing.", 
		     charGetName(ch));
    start_notepad(charGetSocket(ch), "");
  }
}
*/
