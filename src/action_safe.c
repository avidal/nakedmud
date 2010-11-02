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
// Set or delete an alias. If no argument is supplied, all aliases are listed.
//
COMMAND(cmd_alias) {
  // list off all of the aliases
  if(!arg || !*arg) {
    send_to_char(ch, "Current aliases:\r\n");
    if(hashSize(charGetAliases(ch)) == 0)
      send_to_char(ch, "none\r\n");
    else {
      HASH_ITERATOR *alias_i = newHashIterator(charGetAliases(ch));
      const char      *alias = NULL;
      while( (alias = hashIteratorCurrentKey(alias_i)) != NULL) {
	send_to_char(ch, "%-20s %s\r\n", alias, hashIteratorCurrentVal(alias_i));
	hashIteratorNext(alias_i);
      }
      deleteHashIterator(alias_i);
    }
  }
  // otherwise, modify a specific one
  else {
    char alias[SMALL_BUFFER];
    arg = one_arg(arg, alias);
    charSetAlias(ch, alias, arg);
    send_to_char(ch, "Alias %s.\r\n", (arg && *arg ? "set" : "deleted"));
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

  /* make sure we're no longer in the world */
  if(charGetRoom(ch))
    char_from_room(ch);

  charGetSocket(ch)->player = NULL;
  close_socket(charGetSocket(ch), FALSE);
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
