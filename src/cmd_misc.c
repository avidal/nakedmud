//*****************************************************************************
//
// cmd_misc.c
//
// a collection of miscellaneous commands that come with NakedMud(tm)
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "character.h"
#include "socket.h"
#include "save.h"
#include "event.h"
#include "action.h"



//
// stop performing the character's current action
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
COMMAND(cmd_clear) {
  send_to_char(ch, "\033[H\033[J");
}


//
// quit the game
COMMAND(cmd_quit) {
  // log the attempt
  log_string("%s has left the game.", charGetName(ch));
  save_player(ch);

  // 
  // gotta make sure we have a socket. Who knows...
  // a mobile might be trying to quit
  if(charGetSocket(ch)) {
    SOCKET_DATA *sock = charGetSocket(ch);
    charSetSocket(ch, NULL);
    socketSetChar(sock, NULL);
    socketPopInputHandler(sock);
  }

  extract_mobile(ch);
}


//
// save the character
COMMAND(cmd_save) {
  save_player(ch);
  text_to_char(ch, "Saved.\r\n");
}


//
// the function for executing a delayed command
void event_delayed_cmd(CHAR_DATA *ch, void *data, char *cmd) {
  do_cmd(ch, cmd, TRUE);
}


//
// Perform a command, but delay its execution by a couple seconds
COMMAND(cmd_delay) {
  int       secs = 0;
  char *to_delay = NULL;

  if(!parse_args(ch, TRUE, cmd, arg, "int string", &secs, &to_delay))
    return;

  if(secs < 1)
    send_to_char(ch, "You can only delay commands for positive amounts of time.\r\n");
  else {
    send_to_char(ch, "You delay '%s' for %d seconds.\r\n", to_delay, secs);
    start_event(ch, secs SECONDS, event_delayed_cmd, NULL, NULL, to_delay);
  }
}

//
// Displays the MOTD to the character
COMMAND(cmd_motd) {
  // only bother sending it if we have a socket. And then page it, incase
  // the motd is especially long.
  if(charGetSocket(ch))
    page_string(charGetSocket(ch), bufferString(motd));
}
