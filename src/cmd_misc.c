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
#include "handler.h"


//
// Various preference bits that can be turned on and off
//
COMMAND(cmd_tog_prf) {
  switch(subcmd) {
  case SUBCMD_BUILDWALK:
    charToggleBit(ch, BITFIELD_PRFS, PRF_BUILDWALK);
    send_to_char(ch, "Buildwalk %s.\r\n", 
		 (charIsBitSet(ch, BITFIELD_PRFS, PRF_BUILDWALK) ? "on":"off"));
    break;

  default:
    log_string("ERROR: attempted to toggle prf with subcmd %d on %s, but "
	       "subcmd does not exit!", subcmd, charGetName(ch));
  }
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
    sock->player = NULL;
    charSetSocket(ch, NULL);
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
// compress output
//
COMMAND(cmd_compress)
{
  /* no socket, no compression */
  if (!charGetSocket(ch))
    return;

  /* enable compression */
  if (!charGetSocket(ch)->out_compress)
  {
    text_to_char(ch, "Trying compression.\n\r");
    text_to_buffer(charGetSocket(ch), (char *) compress_will2);
    text_to_buffer(charGetSocket(ch), (char *) compress_will);
  }
  else /* disable compression */
  {
    if (!compressEnd(charGetSocket(ch), charGetSocket(ch)->compressing, FALSE))
    {
      text_to_char(ch, "Failed.\n\r");
      return;
    }
    text_to_char(ch, "Compression disabled.\n\r");
  }
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
