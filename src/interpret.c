/*
 * This file handles command interpreting
 */
#include <sys/types.h>
#include <stdio.h>

/* include main header file */
#include "mud.h"
#include "utils.h"
#include "character.h"
#include "socket.h"
#include "room.h"
#include "movement.h"    // for try_move_special
#include "commands.h"
#include "action.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/scripts.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_FACULTY
#include "faculty/faculty.h"
#endif
#ifdef MODULE_ALIAS
#include "alias/alias.h"
#endif



//*****************************************************************************
// local variables and functions
//*****************************************************************************
NEAR_MAP *cmd_table = NULL;


void init_commands() {
  cmd_table = newNearMap();

  //***************************************************************************
  // This is for core functions ONLY! If you have a module that adds new
  // functions to the MUD, they should be added in the init_xxx() function
  // associated with your module.
  //***************************************************************************
  add_cmd("north", "n", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("east",  "e", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("south", "s", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("west",  "w", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("up",    "u", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("down",  "d", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("northeast", "na", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("southeast", "sa",  cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("southwest", "sb", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("northwest", "nb", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("ne",        "ne", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("se",        "se", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("sw",        "sw", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("nw",        "nw", cmd_move, POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );

  // A
  add_cmd("approach",   NULL, cmd_greet,    POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("ask",        NULL, cmd_ask,      POS_SITTING,  POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("at",         NULL, cmd_at,       POS_UNCONCIOUS, POS_FLYING,
	  "builder", TRUE, FALSE);

  // B
  add_cmd("back",       NULL, cmd_back,     POS_UNCONCIOUS, POS_FLYING,
	  "player", TRUE, FALSE);

  // C
  add_cmd("chat",       NULL, cmd_chat,     POS_UNCONCIOUS, POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("clear",      NULL, cmd_clear,    POS_UNCONCIOUS, POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("close",      NULL, cmd_close,    POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("commands",   NULL, cmd_commands, POS_UNCONCIOUS, POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("compress",   NULL, cmd_compress, POS_UNCONCIOUS, POS_FLYING,
	  "player", FALSE, FALSE);
  add_cmd("copyover",   NULL, cmd_copyover, POS_UNCONCIOUS, POS_FLYING,
	  "admin",  FALSE, TRUE);

  // D
  add_cmd("delay",      NULL, cmd_delay,    POS_SLEEPING, POS_FLYING,
	  "player", TRUE,  FALSE);
  add_cmd("drop",       NULL, cmd_drop,     POS_SITTING,  POS_FLYING,
	  "player", TRUE, TRUE );

  // E
  add_cmd("emote",      NULL, cmd_emote,    POS_SITTING,  POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd(":",          NULL, cmd_emote,    POS_SITTING,  POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("equipment",  NULL, cmd_equipment,POS_SITTING,  POS_FLYING,
	  "player", TRUE, FALSE);

  // F
  add_cmd("force",      NULL, cmd_force,    POS_STANDING, POS_FLYING,
	  "admin",   FALSE, FALSE);

  // G
  add_cmd("gemote",     NULL, cmd_gemote,   POS_UNCONCIOUS, POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("give",       NULL, cmd_give,     POS_SITTING,  POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("gossip",     NULL, cmd_chat,     POS_UNCONCIOUS, POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("\"",         NULL, cmd_chat,     POS_UNCONCIOUS, POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("greet",      NULL, cmd_greet,    POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("get",        NULL, cmd_get,      POS_SITTING,  POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("goto",       NULL, cmd_goto,     POS_STANDING, POS_FLYING,
	  "builder", FALSE, TRUE );
  add_cmd("groupcmds",  NULL, cmd_groupcmds,POS_UNCONCIOUS, POS_FLYING,
	  "player", FALSE, FALSE);

  // I
  add_cmd("inventory",  NULL, cmd_inventory,POS_SITTING,  POS_FLYING,
	  "player", TRUE, FALSE);

  // L
  add_cmd("look",       "l",  cmd_look,     POS_SITTING,  POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("lock",       NULL,  cmd_lock,    POS_STANDING,  POS_FLYING,
	  "player", TRUE, TRUE);
  add_cmd("land",       NULL, cmd_stand,    POS_FLYING,   POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("load",       NULL, cmd_load,     POS_SITTING,  POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("linkdead",   NULL, cmd_linkdead, POS_UNCONCIOUS, POS_FLYING,
	  "admin", FALSE, FALSE);
  add_cmd("lockdown",   NULL, cmd_lockdown, POS_UNCONCIOUS, POS_FLYING,
	  "admin", FALSE, FALSE);

  // M
  add_cmd("mlist",      NULL, cmd_mlist,    POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("mdelete",    NULL, cmd_mdelete,  POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("mrename",    NULL, cmd_mrename,  POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("more",       NULL, cmd_more,     POS_UNCONCIOUS, POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("motd",       NULL, cmd_motd,     POS_UNCONCIOUS, POS_FLYING,
	  "player", TRUE, FALSE);

  // O
  add_cmd("olist",      NULL, cmd_olist,    POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("odelete",    NULL, cmd_odelete,  POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("orename",    NULL, cmd_orename,  POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("open",       NULL, cmd_open,     POS_STANDING, POS_FLYING,
	  "player", TRUE, TRUE );

  // P
  add_cmd("put",        "p", cmd_put,       POS_SITTING,  POS_FLYING,
	  "player", TRUE,  TRUE );
  add_cmd("page",       NULL, cmd_page,     POS_SITTING,  POS_FLYING,
	  "builder", TRUE, FALSE);
  add_cmd("purge",      NULL, cmd_purge,    POS_SITTING,  POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("pulserate",  NULL, cmd_pulserate,POS_UNCONCIOUS, POS_FLYING,
	  "admin", FALSE, FALSE);
  
  // Q
  add_cmd("quit",       NULL, cmd_quit,     POS_SLEEPING, POS_FLYING,
	  "player", FALSE, TRUE );

  // R
  add_cmd("rreload",    NULL, cmd_rreload,  POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("remove",     NULL, cmd_remove,   POS_SITTING, POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("repeat",     NULL, cmd_repeat,   POS_UNCONCIOUS, POS_FLYING,
	  "admin", FALSE, FALSE);
  add_cmd("rlist",      NULL, cmd_rlist,    POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("rdelete",    NULL, cmd_rdelete,  POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
  add_cmd("rrename",    NULL, cmd_rrename,  POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);

  // S
  add_cmd("say",        NULL, cmd_say,      POS_SITTING,  POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("'",          NULL, cmd_say,      POS_SITTING,  POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("save",       NULL, cmd_save,     POS_SLEEPING, POS_FLYING,
	  "player", FALSE, FALSE);
  add_cmd("shutdown",   NULL, cmd_shutdown, POS_UNCONCIOUS, POS_FLYING,
	  "admin", FALSE, TRUE );
  add_cmd("sit",        NULL, cmd_sit,      POS_SITTING,  POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("sleep",      NULL, cmd_sleep,    POS_SITTING,  POS_STANDING,
	  "player", TRUE, FALSE);
  add_cmd("stand",      NULL, cmd_stand,    POS_SITTING,  POS_STANDING,
	  "player", TRUE, TRUE );
  add_cmd("stop",       NULL, cmd_stop,     POS_SITTING, POS_FLYING,
	  "player", TRUE, FALSE);

  // T
  add_cmd("take",       NULL, cmd_get,      POS_SITTING,  POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("tell",       NULL, cmd_tell,     POS_SLEEPING, POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("transfer",   NULL, cmd_transfer, POS_STANDING, POS_FLYING,
	  "builder", FALSE, TRUE);

  // U
  add_cmd("unlock",       NULL,  cmd_unlock,    POS_STANDING,  POS_FLYING,
	  "player", TRUE, TRUE);

  // W
  add_cmd("wake",       NULL, cmd_wake,     POS_SLEEPING,  POS_SLEEPING,
	  "player", TRUE, TRUE );
  add_cmd("wear",       NULL, cmd_wear,     POS_SITTING,  POS_FLYING,
	  "player", TRUE, TRUE );
  add_cmd("who",        NULL, cmd_who,      POS_UNCONCIOUS, POS_FLYING,
	  "player", TRUE, FALSE);
  add_cmd("worn",       NULL, cmd_equipment,POS_SITTING,  POS_FLYING,
	  "player", TRUE, FALSE);

  // Z
  add_cmd("zlist",      NULL, cmd_zlist,    POS_SITTING,  POS_FLYING,
	  "builder", FALSE, TRUE);
  add_cmd("zreset",     NULL, cmd_zreset,   POS_UNCONCIOUS, POS_FLYING,
	  "builder", FALSE, FALSE);
}


bool cmd_exists(const char *cmd) {
  return nearMapKeyExists(cmd_table, cmd);
}

void remove_cmd(const char *cmd) {
  CMD_DATA *old_cmd = nearMapRemove(cmd_table, cmd);
  if(old_cmd) deleteCmd(old_cmd);
}

void add_cmd(const char *cmd, const char *sort_by, COMMAND(func),
	     int min_pos, int max_pos, const char *user_group, 
	     bool mob_ok, bool interrupts) {
  // if we've already got a command named this, remove it
  remove_cmd(cmd);

  // add in the new command
  nearMapPut(cmd_table, cmd, sort_by, 
	     newCmd(cmd, func, min_pos, max_pos, 
		    user_group, mob_ok, interrupts));
}


void add_py_cmd(const char *cmd, const char *sort_by, void *pyfunc,
	     int min_pos, int max_pos, const char *user_group, 
	     bool mob_ok, bool interrupts) {
  // if we've already got a command named this, remove it
  remove_cmd(cmd);

  // add in the new command
  nearMapPut(cmd_table, cmd, sort_by, 
	     newPyCmd(cmd, pyfunc, min_pos, max_pos, 
		    user_group, mob_ok, interrupts));
}


// show the character all of the commands in the specified group(s).
void show_commands(CHAR_DATA *ch, const char *user_groups) {
  BUFFER           *buf = newBuffer(MAX_BUFFER);
  NEAR_ITERATOR *near_i = newNearIterator(cmd_table);
  const char    *abbrev = NULL;
  CMD_DATA         *cmd = NULL;
  int               col = 0;

  // go over all of our buckets
  ITERATE_NEARMAP(abbrev, cmd, near_i) {
    if(is_keyword(user_groups, cmdGetUserGroup(cmd), FALSE)) {
      bprintf(buf, "%-13.13s", cmdGetName(cmd));
      if (!(++col % 6))
	bufferCat(buf, "\r\n");
    }
  } deleteNearIterator(near_i);

  // tag on our last newline if neccessary, and show commands
  if (col % 6) bprintf(buf, "\r\n");
  text_to_char(ch, bufferString(buf));
  deleteBuffer(buf);
}


// tries to pull a usable command from the near-table and use it. Returns
// TRUE if a usable command was found (even if it failed) and false otherwise.
bool try_use_cmd_table(CHAR_DATA *ch, NEAR_MAP *table, const char *command, 
		       char *arg, bool abbrev_ok) {
  if(abbrev_ok == FALSE) {
    CMD_DATA *cmd = nearMapGet(table, command, FALSE);
    if(cmd == NULL || !is_keyword(bitvectorGetBits(charGetUserGroups(ch)),
				  cmdGetUserGroup(cmd), FALSE))
      return FALSE;
    else {
      charTryCmd(ch, cmd, arg); 
      return TRUE;
    }
  }
  else {
    // try to look up the possible commands
    LIST *cmd_list = nearMapGetAllMatches(table, command);
    bool cmd_found = FALSE;
    if(cmd_list != NULL) {
      LIST_ITERATOR *cmd_i = newListIterator(cmd_list);
      CMD_DATA        *cmd = NULL;
      ITERATE_LIST(cmd, cmd_i) {
	if(is_keyword(bitvectorGetBits(charGetUserGroups(ch)), 
		      cmdGetUserGroup(cmd), FALSE)) {
	  charTryCmd(ch, cmd, arg); 
	  cmd_found = TRUE;
	  break;
	}
      } deleteListIterator(cmd_i);
      deleteList(cmd_list);
    }
    return cmd_found;
  }
}


void handle_cmd_input(SOCKET_DATA *dsock, char *arg) {
  CHAR_DATA *ch;
  if ((ch = socketGetChar(dsock)) == NULL)
    return;
  do_cmd(ch, arg, TRUE);
}


void do_cmd(CHAR_DATA *ch, char *arg, bool aliases_ok)  {
  char command[MAX_BUFFER];

  // make sure we've got a command to try
  if(!arg || !*arg)
    return;

  // if we are leading with a non-character, we are trying to do a short-form
  // command (e.g. ' for say, " for gossip). Just take the first character
  // and use the rest as the arg
  if(isalpha(*arg) || isdigit(*arg))
    arg = one_arg(arg, command);
  else {
    *command     = *arg;
    *(command+1) = '\0';
    arg++;
    // and skip all spaces
    while(isspace(*arg))
      arg++;
  }

#ifdef MODULE_ALIAS
  if(aliases_ok && try_alias(ch, command, arg))
    return;
#endif

  // check to see if it's a faculty command
#ifdef MODULE_FACULTY
  if(try_use_faculty(ch, command))
    return;
#endif

  // first try room commands then world commands
  if(!charGetRoom(ch) || 
     !try_use_cmd_table(ch,roomGetCmdTable(charGetRoom(ch)),command,arg,FALSE))
    if(!try_use_cmd_table(ch, cmd_table, command, arg, TRUE))
      text_to_char(ch, "No such command.\r\n");
}
