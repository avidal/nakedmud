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
#include "scripts/script.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_FACULTY
#include "faculty/faculty.h"
#endif
#ifdef MODULE_ALIAS
#include "alias/alias.h"
#endif


// our command table list
LIST   **cmd_table = NULL;

typedef struct cmd_data {
  char      * cmd_name;
  char      * sort_by;
  CMD_PTR(cmd_funct);
  int        subcmd;
  int        min_pos;
  int        max_pos;
  int        level;
  bool       mob_ok;     // can NPCs use this command?
  bool       interrupts; // does it interrupt actions?
} CMD_DATA;


void init_commands() {
  cmd_table = malloc(sizeof(LIST *) * 26); // 1 for each letter
  int i;
  for(i = 0; i < 26; i++)
    cmd_table[i] = newList();

  //************************************************************************
  // This is for core functions ONLY! If you have a module that adds new
  // functions to the MUD, they should be added in the init_xxx() function
  // associated with your module.
  //************************************************************************
  add_cmd("north", "n", cmd_move,     DIR_NORTH,    POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("east",  "e", cmd_move,     DIR_EAST,     POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("south", "s", cmd_move,     DIR_SOUTH,    POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("west",  "w", cmd_move,     DIR_WEST,     POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("up",    "u", cmd_move,     DIR_UP,       POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("down",  "d", cmd_move,     DIR_DOWN,     POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("northeast", "na", cmd_move,DIR_NORTHEAST,POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("southeast", "sa",  cmd_move,DIR_SOUTHEAST,POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("southwest", "sb", cmd_move,DIR_SOUTHWEST,POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("northwest", "nb", cmd_move,DIR_NORTHWEST,POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("ne",        "ne", cmd_move,DIR_NORTHEAST,POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("se",        "se", cmd_move,DIR_SOUTHEAST,POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("sw",        "sw", cmd_move,DIR_SOUTHWEST,POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("nw",        "nw", cmd_move,DIR_NORTHWEST,POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );

  // A
  add_cmd("approach",   NULL, cmd_greet,    0, POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("ask",        NULL, cmd_ask,      0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("at",         NULL, cmd_at,       0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, TRUE, FALSE);

  // B
  add_cmd("back",       NULL, cmd_back,     0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("buildwalk",  NULL, cmd_buildwalk, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);

  // C
  add_cmd("chat",       NULL, cmd_chat,     0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("clear",      NULL, cmd_clear,    0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("close",      NULL, cmd_close,    0, POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("commands",   NULL, cmd_commands, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("compress",   NULL, cmd_compress, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, FALSE, FALSE);
  add_cmd("copyover",   NULL, cmd_copyover, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_ADMIN,  FALSE, TRUE);

  // D
  add_cmd("delay",      NULL, cmd_delay,    0, POS_SLEEPING, POS_FLYING,
	  LEVEL_PLAYER, TRUE,  FALSE);
  add_cmd("dig",        NULL, cmd_dig,      0, POS_STANDING, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("dlist",      NULL, cmd_dlist,    0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("ddelete",    NULL, cmd_ddelete,  0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("drop",       NULL, cmd_drop,     0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );

  // E
  add_cmd("emote",      NULL, cmd_emote,    0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd(":",          NULL, cmd_emote,    0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("equipment",  NULL, cmd_equipment,0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);

  // F
  add_cmd("fill",       NULL, cmd_fill,     0, POS_STANDING, POS_FLYING,
	  LEVEL_BUILDER, FALSE, TRUE );

  // G
  add_cmd("gemote",     NULL, cmd_gemote,   0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("give",       NULL, cmd_give,     0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("gossip",     NULL, cmd_chat,     0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("\"",         NULL, cmd_chat,     0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("greet",      NULL, cmd_greet,    0, POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("get",        NULL, cmd_get,      0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("goto",       NULL, cmd_goto,     0, POS_STANDING, POS_FLYING,
	  LEVEL_BUILDER, FALSE, TRUE );

  // I
  add_cmd("inventory",  NULL, cmd_inventory,0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("invis",      NULL, cmd_invis,    0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);

  // L
  add_cmd("look",       "l",  cmd_look,     0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("lock",       NULL,  cmd_lock,    0, POS_STANDING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE);
  add_cmd("land",       NULL, cmd_stand,    0, POS_FLYING,   POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("load",       NULL, cmd_load,     0, POS_SITTING,  POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("linkdead",   NULL, cmd_linkdead, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_ADMIN, FALSE, FALSE);
  add_cmd("lockdown",   NULL, cmd_lockdown, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_ADMIN, FALSE, FALSE);

  // M
  add_cmd("mlist",      NULL, cmd_mlist,    0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("mdelete",    NULL, cmd_mdelete,  0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("more",       NULL, cmd_more,     0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("motd",       NULL, cmd_motd,     0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);

  // O
  add_cmd("olist",      NULL, cmd_olist,    0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("odelete",    NULL, cmd_odelete,  0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("open",       NULL, cmd_open,     0, POS_STANDING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );

  // P
  add_cmd("put",        "p", cmd_put,       0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE,  TRUE );
  add_cmd("page",       NULL, cmd_page,     0, POS_SITTING,  POS_FLYING,
	  LEVEL_BUILDER, TRUE, FALSE);
  add_cmd("purge",      NULL, cmd_purge,    0, POS_SITTING,  POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  
  // Q
  add_cmd("quit",       NULL, cmd_quit,     0, POS_SLEEPING, POS_FLYING,
	  LEVEL_PLAYER, FALSE, TRUE );

  // R
  add_cmd("remove",     NULL, cmd_remove,   0, POS_SITTING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("rlist",      NULL, cmd_rlist,    0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("rdelete",    NULL, cmd_rdelete,  0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("repeat",     NULL, cmd_repeat,   0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_ADMIN, FALSE, FALSE);

  // S
  add_cmd("say",        NULL, cmd_say,      0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("'",          NULL, cmd_say,      0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("save",       NULL, cmd_save,     0, POS_SLEEPING, POS_FLYING,
	  LEVEL_PLAYER, FALSE, FALSE);
  add_cmd("shutdown",   NULL, cmd_shutdown, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_ADMIN, FALSE, TRUE );
  add_cmd("sit",        NULL, cmd_sit,      0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("sleep",      NULL, cmd_sleep,    0, POS_SITTING,  POS_STANDING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("stand",      NULL, cmd_stand,    0, POS_SITTING,  POS_STANDING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("stop",       NULL, cmd_stop,     0, POS_SITTING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  // really, we -should- put this in the scripts module, but there are some
  // very nice functions in builder.c that cmd_sclist uses to print scripts,
  // which wouldn't be accessable from outside of builder.c
  add_cmd("sclist",     NULL, cmd_sclist,   0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("sdelete",    NULL, cmd_scdelete, 0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);

  // T
  add_cmd("take",       NULL, cmd_get,      0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("tell",       NULL, cmd_tell,     0, POS_SLEEPING, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("transfer",   NULL, cmd_transfer, 0, POS_STANDING, POS_FLYING,
	  LEVEL_BUILDER, FALSE, TRUE);

  // U
  add_cmd("unlock",       NULL,  cmd_unlock,    0, POS_STANDING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE);

  // V
  add_cmd("visible",      NULL, cmd_visible,    0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);

  // W
  add_cmd("wake",       NULL, cmd_wake,     0, POS_SLEEPING,  POS_SLEEPING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("wear",       NULL, cmd_wear,     0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, TRUE );
  add_cmd("who",        NULL, cmd_who,      0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);
  add_cmd("wizhelp",    NULL, cmd_wizhelp,  0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
  add_cmd("worn",       NULL, cmd_equipment,0, POS_SITTING,  POS_FLYING,
	  LEVEL_PLAYER, TRUE, FALSE);

  // Z
  add_cmd("zlist",      NULL, cmd_zlist,    0, POS_SITTING,  POS_FLYING,
	  LEVEL_BUILDER, FALSE, TRUE);
  add_cmd("zreset",     NULL, cmd_zreset,   0, POS_UNCONCIOUS, POS_FLYING,
	  LEVEL_BUILDER, FALSE, FALSE);
}


int cmdbucket(const char *cmd) {
  if(!isalpha(*cmd))
    return 0;
  return (tolower(*cmd) - 'a');
}

// compare two command by their sort_by variable
int cmdsortbycmp(const CMD_DATA *cmd1, const CMD_DATA *cmd2) {
  return strcasecmp(cmd1->sort_by, cmd2->sort_by);
}

// compare two commands
int cmdcmp(const CMD_DATA *cmd1, const CMD_DATA *cmd2) {
  return strcasecmp(cmd1->cmd_name, cmd2->cmd_name);
}

// check if the cmd matches the command's cmd_name up to the length of cmd
int is_cmd_abbrev(const char *cmd, const CMD_DATA *entry) {
  return strncasecmp(cmd, entry->cmd_name, strlen(cmd));
}

// check if the string matches the command's cmd_name
int is_cmd(const char *cmd, const CMD_DATA *entry) {
  return strcasecmp(cmd, entry->cmd_name);
}

// find a command
CMD_DATA *find_cmd(const char *cmd, bool abbrev_ok) {
  if(abbrev_ok)
    return listGetWith(cmd_table[cmdbucket(cmd)], cmd, is_cmd_abbrev);
  else
    return listGetWith(cmd_table[cmdbucket(cmd)], cmd, is_cmd);
}

//
// return TRUE if the command already exists.
//
bool cmd_exists(const char *cmd) {
  return (find_cmd(cmd, FALSE) != NULL);
}

//
// remove (and delete) a command
//
void remove_cmd(const char *cmd) {
  CMD_DATA *removed = listRemoveWith(cmd_table[cmdbucket(cmd)], cmd, is_cmd);
  if(removed) {
    if(removed->cmd_name) free(removed->cmd_name);
    if(removed->sort_by)  free(removed->sort_by);
    free(removed);
  }
}

void add_cmd(const char *cmd, const char *sort_by,
	     void *func, int subcmd, int min_pos, int max_pos,
	     int min_level, bool mob_ok, bool interrupts) {
  // if we've already got a command named this, remove it
  remove_cmd(cmd);

  // make our new command data
  CMD_DATA *new_cmd = malloc(sizeof(CMD_DATA));
  new_cmd->cmd_name   = strdup(cmd);
  new_cmd->sort_by    = strdup(sort_by ? sort_by : cmd);
  new_cmd->cmd_funct  = func;
  new_cmd->subcmd     = subcmd;
  new_cmd->min_pos    = min_pos;
  new_cmd->max_pos    = max_pos;
  new_cmd->level      = min_level;
  new_cmd->mob_ok     = mob_ok;
  new_cmd->interrupts = interrupts;

  // and add it in
  listPutWith(cmd_table[cmdbucket(cmd)], new_cmd, cmdsortbycmp);
}


// show the character all of the commands he or she can perform
void show_commands(CHAR_DATA *ch, int min_lev, int max_lev) {
  BUFFER *buf = newBuffer(MAX_BUFFER);
  int i, col = 0;

  // go over all of our buckets
  for(i = 0; i < 26; i++) {
    LIST_ITERATOR *buck_i = newListIterator(cmd_table[i]);
    CMD_DATA *cmd = NULL;

    ITERATE_LIST(cmd, buck_i) {
      if(min_lev > cmd->level || max_lev < cmd->level)
	continue;
      bprintf(buf, "%-20.20s", cmd->cmd_name);
      if (!(++col % 4))
	bufferCat(buf, "\r\n");
    }
    deleteListIterator(buck_i);
  }

  if (col % 4) bprintf(buf, "\r\n");
  text_to_char(ch, bufferString(buf));
  deleteBuffer(buf);
}



//
// make sure the character is in a position where
// this can be performed
// 
bool min_pos_ok(CHAR_DATA *ch, int minpos) {
  if(poscmp(charGetPos(ch), minpos) >= 0)
    return TRUE;
  else {
    switch(charGetPos(ch)) {
    case POS_UNCONCIOUS:
      send_to_char(ch, "You cannot do that while unconcious!\r\n");
      break;
    case POS_SLEEPING:
      send_to_char(ch, "Not while sleeping, you won't!\r\n");
      break;
    case POS_SITTING:
      send_to_char(ch, "You cannot do that while sitting!\r\n");
      break;
    case POS_STANDING:
      // flying is the highest position... we can deduce this message
      send_to_char(ch, "You must be flying to try that.\r\n");
      break;
    case POS_FLYING:
      send_to_char(ch, "That is not possible in any position you can think of.\r\n");
      break;
    default:
      send_to_char(ch, "Your position is all wrong!\r\n");
      log_string("Character, %s, has invalid position, %d.",
		 charGetName(ch), charGetPos(ch));
      break;
    }
    return FALSE;
  }
}


//
// make sure the character is in a position where
// this can be performed
// 
bool max_pos_ok(CHAR_DATA *ch, int minpos) {
  if(poscmp(charGetPos(ch), minpos) <= 0)
    return TRUE;
  else {
    switch(charGetPos(ch)) {
    case POS_UNCONCIOUS:
      send_to_char(ch, "You're still too alive to try that!\r\n");
      break;
    case POS_SLEEPING:
      send_to_char(ch, "Not while sleeping, you won't!\r\n");
      break;
    case POS_SITTING:
      send_to_char(ch, "You cannot do that while sitting!\r\n");
      break;
    case POS_STANDING:
      send_to_char(ch, "You cannot do that while standing.\r\n");
      break;
    case POS_FLYING:
      send_to_char(ch, "You must land first.\r\n");
      break;
    default:
      send_to_char(ch, "Your position is all wrong!\r\n");
      log_string("Character, %s, has invalid position, %d.",
		 charGetName(ch), charGetPos(ch));
      break;
    }
    return FALSE;
  }
}


void handle_cmd_input(SOCKET_DATA *dsock, char *arg) {
  CHAR_DATA *ch;
  if ((ch = socketGetChar(dsock)) == NULL)
    return;
  do_cmd(ch, arg, TRUE, TRUE);
}


void do_cmd(CHAR_DATA *ch, char *arg, bool scripts_ok, bool aliases_ok)  {
  char command[MAX_BUFFER];
  bool found_cmd = FALSE;

  // make sure we've got a command to try
  if(!arg || !*arg)
    return;

  // if we are leading with a non-character, we are trying to do a short-form
  // command (e.g. ' for say, " for gossip). Just take the first character
  // and use the rest as the arg
  if(isalpha(*arg))
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
  // see if it's an alias
  const char *alias = charGetAlias(ch, command);
  if(aliases_ok && alias) {
    char *alias_cmd = expand_alias(alias, arg);
    do_cmd(ch, alias_cmd, scripts_ok, FALSE);
    free(alias_cmd);
    return;
  }
#endif

  // check to see if it's a faculty command
#ifdef MODULE_FACULTY
  if(try_use_faculty(ch, command))
    return;
#endif

  // if we've got a command script, and we're not supposed
  // to follow through with our normal command, return out
  if(scripts_ok && try_command_script(ch, command, arg))
    return;


  // iterate over the commands that would be in our 
  // bucket and find the one that we are trying to use
  LIST_ITERATOR *cmd_i = newListIterator(cmd_table[cmdbucket(command)]);
  CMD_DATA *cmd = NULL;
  ITERATE_LIST(cmd, cmd_i) {
    if (cmd->level > charGetLevel(ch))
      continue;

    if (is_prefix(command, cmd->cmd_name)) {
      found_cmd = TRUE;
      if(min_pos_ok(ch, cmd->min_pos) && max_pos_ok(ch,cmd->max_pos) &&
	 (!charIsNPC(ch) || cmd->mob_ok)) {
	if(cmd->interrupts) {
#ifdef MODULE_FACULTY
	  interrupt_action(ch, FACULTY_ALL);
#else
	  interrupt_action(ch, 1);
#endif
	}
	(cmd->cmd_funct)(ch, cmd->cmd_name, cmd->subcmd, arg);
      }
      break;
    }
  }
  deleteListIterator(cmd_i);



  if (!found_cmd) {
    // check to see if the character is trying to use
    // a special exit command for the room
    if(roomGetExitSpecial(charGetRoom(ch), command)) {
      if(min_pos_ok(ch, POS_STANDING)) {
#ifdef MODULE_FACULTY
	interrupt_action(ch, FACULTY_ALL);
#else
	interrupt_action(ch, 1);
#endif
	try_move(ch, DIR_NONE, command);
      }
    }
    else
      text_to_char(ch, "No such command.\n\r");
  }
}
