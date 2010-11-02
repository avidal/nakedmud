//*****************************************************************************
//
// command.c
//
// A structure that holds all of the data tied to player commands. 
// Specifically, the function that is executed and all of the restrictions on
// its execution.
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "mud.h"
#include "utils.h"
#include "action.h"
#include "character.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/scripts.h"
#include "scripts/pyplugs.h"
#include "scripts/pychar.h"



//*****************************************************************************
// local structs, defines, and functions
//*****************************************************************************
struct cmd_data {
  char       *name;
  CMD_PTR(func);
  PyObject *pyfunc;
  int      min_pos;
  int      max_pos;
  char *user_group;
  bool      mob_ok;
  bool  interrupts;
};



//*****************************************************************************
// implementation of command.h
//*****************************************************************************
CMD_DATA *newCmd(const char *name, COMMAND(func), int min_pos, int max_pos,
		 const char *user_group, bool mob_ok, bool interrupts) {
  CMD_DATA *cmd   = malloc(sizeof(CMD_DATA));
  cmd->name       = strdupsafe(name);
  cmd->func       = func;
  cmd->min_pos    = min_pos;
  cmd->max_pos    = max_pos;
  cmd->user_group = strdupsafe(user_group);
  cmd->mob_ok     = mob_ok;
  cmd->interrupts = interrupts;
  cmd->pyfunc     = NULL;
  return cmd;
}

CMD_DATA *newPyCmd(const char *name, void *pyfunc, int min_pos, int max_pos,
		 const char *user_group, bool mob_ok, bool interrupts) {
  CMD_DATA *cmd   = malloc(sizeof(CMD_DATA));
  cmd->name       = strdupsafe(name);
  cmd->func       = NULL;
  cmd->min_pos    = min_pos;
  cmd->max_pos    = max_pos;
  cmd->user_group = strdupsafe(user_group);
  cmd->mob_ok     = mob_ok;
  cmd->interrupts = interrupts;
  cmd->pyfunc     = pyfunc;
  Py_INCREF(cmd->pyfunc);
  return cmd;
}

void deleteCmd(CMD_DATA *cmd) {
  if(cmd->name)       free(cmd->name);
  if(cmd->user_group) free(cmd->user_group);
  if(cmd->pyfunc)     { Py_DECREF(cmd->pyfunc); }
  free(cmd);
}

CMD_DATA *cmdCopy(CMD_DATA *cmd) {
  if(cmd->func)
    return newCmd(cmd->name, cmd->func, cmd->min_pos, cmd->max_pos,
		  cmd->user_group, cmd->mob_ok, cmd->interrupts);
  else
    return newPyCmd(cmd->name, cmd->pyfunc, cmd->min_pos, cmd->max_pos,
		    cmd->user_group, cmd->mob_ok, cmd->interrupts);
}

void cmdCopyTo(CMD_DATA *from, CMD_DATA *to) {
  if(to->name)       free(to->name);
  if(to->user_group) free(to->user_group);
  if(to->pyfunc)     { Py_DECREF(to->pyfunc); }
  *to = *from;
  to->name         = strdup(from->name);
  to->user_group   = strdup(from->user_group);
  if(to->pyfunc)     { Py_INCREF(to->pyfunc); }
}

const char *cmdGetName(CMD_DATA *cmd) {
  return cmd->name;
}

const char *cmdGetUserGroup(CMD_DATA *cmd) {
  return cmd->user_group;
}

int cmdGetMinPos(CMD_DATA *cmd) {
  return cmd->min_pos;
}

int cmdGetMaxPos(CMD_DATA *cmd) {
  return cmd->max_pos;
}

bool cmdGetMobOk(CMD_DATA *cmd) {
  return cmd->mob_ok;
}

bool cmdGetInterrupts(CMD_DATA *cmd) {
  return cmd->interrupts;
}


//
// make sure the character is in a position where this can be performed
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
// make sure the character is in a position where this can be performed
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


bool charTryCmd(CHAR_DATA *ch, CMD_DATA *cmd, char *arg) {
  if(!min_pos_ok(ch, cmd->min_pos) || !max_pos_ok(ch,cmd->max_pos) ||
     (charIsNPC(ch) && !cmd->mob_ok))
    return FALSE;
  else {
    if(cmd->interrupts) {
#ifdef MODULE_FACULTY
      interrupt_action(ch, FACULTY_ALL);
#else
      interrupt_action(ch, 1);
#endif
    }
    if(cmd->func)
      (cmd->func)(ch, cmd->name, arg);
    else {
      PyObject *arglist = Py_BuildValue("Oss", charGetPyFormBorrowed(ch), 
					cmd->name, arg);
      PyObject *retval  = PyEval_CallObject(cmd->pyfunc, arglist);
      // check for an error:
      if(retval == NULL) {
	char *tb = getPythonTraceback();
	if(tb != NULL) {
	  log_string("Error running python command, %s:\r\n%s\r\n", 
		     cmd->name, tb);
	  free(tb);
	}
      }

      // garbage collection
      Py_XDECREF(retval);
      Py_XDECREF(arglist);
    }
    return TRUE;
  }
}
