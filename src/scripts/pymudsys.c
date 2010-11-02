//*****************************************************************************
//
// pymudsys.c
//
// A set of system level commands and variables that may be needed by python,
// but which are not neccessarily needed by scripts.
//
//*****************************************************************************
#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../utils.h"
#include "../character.h"
#include "../socket.h"
#include "../save.h"
#include "../handler.h"
#include "../account.h"

#include "pymudsys.h"
#include "scripts.h"
#include "pyplugs.h"
#include "pychar.h"
#include "pyaccount.h"
#include "pysocket.h"


//******************************************************************************
// optional modules
//******************************************************************************
#ifdef MODULE_HELP2
#include "../help2/help.h"
#endif


//*****************************************************************************
// local variables and functions
//*****************************************************************************
// a list of methods to add to the mudsys module
LIST *pymudsys_methods = NULL;



//*****************************************************************************
// mudsys methods
//*****************************************************************************
PyObject *mudsys_set_sys_val(PyObject *self, PyObject *args) {
  char *key, *val;
  if(!PyArg_ParseTuple(args, "ss", &key, &val)) {
    PyErr_Format(PyExc_TypeError, "Provide a string key and value");
    return NULL;
  }

  mudsettingSetString(key, val);
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_get_sys_val(PyObject *self, PyObject *args) {
  char *key;
  if(!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, "Provide a string key");
    return NULL;
  }

  return Py_BuildValue("s", mudsettingGetString(key));
}

PyObject *mudsys_shutdown(PyObject *self, PyObject *args) {
  shut_down = TRUE;
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_copyover(PyObject *self, PyObject *args) {
  do_copyover();
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_create_account(PyObject *self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }

  if(account_exists(name))
    return Py_BuildValue("O", Py_None);
  if(account_creating(name))
    return Py_BuildValue("O", Py_None);
  ACCOUNT_DATA *acct = newAccount();
  accountSetName(acct, name);
  return Py_BuildValue("O", accountGetPyFormBorrowed(acct));
}

PyObject *mudsys_create_player(PyObject *Self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }

  if(player_exists(name))
    return Py_BuildValue("O", Py_None);
  if(player_creating(name))
    return Py_BuildValue("O", Py_None);
  CHAR_DATA *ch = newChar();
  charSetName(ch, name);

  // give the character a unique id 
  int next_char_uid = mudsettingGetInt("puid") + 1;
  mudsettingSetInt("puid", next_char_uid);
  charSetUID(ch, next_char_uid);
      
  // if it's the first player, give him all priviledges
  if(charGetUID(ch) == 1)
    bitSet(charGetUserGroups(ch),
	   "admin, builder, scripter, player, playtester");

  char_exist(ch);
  return Py_BuildValue("O", charGetPyFormBorrowed(ch));
}

PyObject *mudsys_player_exists(PyObject *self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }
  return Py_BuildValue("i", player_exists(name));
}

PyObject *mudsys_account_exists(PyObject *self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }
  return Py_BuildValue("i", account_exists(name));
}

PyObject *mudsys_player_creating(PyObject *self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }
  return Py_BuildValue("i", player_creating(name));
}

PyObject *mudsys_account_creating(PyObject *self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }
  return Py_BuildValue("i", account_creating(name));
}

//
// registers a player into the system
PyObject *mudsys_do_register(PyObject *Self, PyObject *args) {
  PyObject      *val = NULL;
  CHAR_DATA      *ch = NULL;
  ACCOUNT_DATA *acct = NULL;

  if(!PyArg_ParseTuple(args, "O", &val)) {
    PyErr_Format(PyExc_TypeError, 
		 "A char or account to be registered must be supplied.");
    return NULL;
  }

  if(PyChar_Check(val)) {
    if((ch = PyChar_AsChar(val)) == NULL) {
      PyErr_Format(PyExc_StandardError,
		   "Tried to register nonexistant player, %d.",
		   PyChar_AsUid(val));
      return NULL;
    }
    if(!charIsNPC(ch))
      register_player(ch);
  }
  else if(PyAccount_Check(val)) {
    if((acct = PyAccount_AsAccount(val)) == NULL) {
      PyErr_Format(PyExc_StandardError,
		   "Tried to register nonexistant account.");
      return NULL;
    }
    register_account(acct);
  }
  else { 
    PyErr_Format(PyExc_TypeError, 
		 "Only players and accounts may be registered.");
    return NULL;
  }

  return Py_BuildValue("i", 1);
}

//
// saves a player to disk
PyObject *mudsys_do_save(PyObject *self, PyObject *args) {
  PyObject      *val = NULL;
  CHAR_DATA      *ch = NULL;
  ACCOUNT_DATA *acct = NULL;

  if(!PyArg_ParseTuple(args, "O", &val)) {
    PyErr_Format(PyExc_TypeError, "A character or account must be supplied.");
    return NULL;
  }

  if(PyChar_Check(val)) {
    if( (ch = PyChar_AsChar(val)) == NULL) {
      PyErr_Format(PyExc_StandardError,
		   "Tried to save nonexistant character, %d.",
		   PyChar_AsUid(val));
      return NULL;
    }
    // only save registered characters
    if(!player_exists(charGetName(ch)))
      return Py_BuildValue("i", 0);
    if(!charIsNPC(ch))
      save_player(ch);
  }
  else if(PyAccount_Check(val)) {
    if( (acct = PyAccount_AsAccount(val)) == NULL) {
      PyErr_Format(PyExc_StandardError, "Tried to save nonexistant account.");
      return NULL;
    }
    // only save registered players
    if(!account_exists(accountGetName(acct)))
      return Py_BuildValue("i", 0);
    save_account(acct);
  }
  else {
    PyErr_Format(PyExc_TypeError, "Only characters and accounts may be saved.");
    return NULL;
  }

  return Py_BuildValue("i", 1);
}

//
// attaches an account to a socket
PyObject *mudsys_attach_account_socket(PyObject *self, PyObject *args) {
  PyObject   *pyacct = NULL;
  PyObject   *pysock = NULL;
  ACCOUNT_DATA *acct = NULL;
  SOCKET_DATA  *sock = NULL;

  if(!PyArg_ParseTuple(args, "OO", &pyacct, &pysock)) {
    PyErr_Format(PyExc_TypeError,"An account and socket to be attached must be supplied.");
    return NULL;
  }

  if(!PyAccount_Check(pyacct)) {
    PyErr_Format(PyExc_TypeError, "First argument must be an account.");
    return NULL;
  }
  if(!PySocket_Check(pysock)) {
    PyErr_Format(PyExc_TypeError, "Second argument must be a socket.");
    return NULL;
  }

  if( (acct = PyAccount_AsAccount(pyacct)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach nonexistant account.");
    return NULL;
  }
  if( (sock = PySocket_AsSocket(pysock)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach nonexistant socket, %d.",PySocket_AsUid(pysock));
    return NULL;
  }

  // first, do any nessessary detaching for our old character and old socket
  if(socketGetAccount(sock) != NULL && 
     account_exists(accountGetName(socketGetAccount(sock))))
    unreference_account(socketGetAccount(sock));
  socketSetAccount(sock, acct);
  if(account_exists(accountGetName(acct)))
    reference_account(acct);
  return Py_BuildValue("i", 1);
}

//
// attaches a character to a socket
PyObject *mudsys_attach_char_socket(PyObject *self, PyObject *args) {
  PyObject    *pych = NULL;
  PyObject  *pysock = NULL;
  CHAR_DATA     *ch = NULL;
  SOCKET_DATA *sock = NULL;

  if(!PyArg_ParseTuple(args, "OO", &pych, &pysock)) {
    PyErr_Format(PyExc_TypeError,"A character and socket to be attached must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "First argument must be a character.");
    return NULL;
  }
  if(!PySocket_Check(pysock)) {
    PyErr_Format(PyExc_TypeError, "Second argument must be a socket.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }
  if( (sock = PySocket_AsSocket(pysock)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach nonexistant socket, %d.",PySocket_AsUid(pysock));
    return NULL;
  }

  // first, do any nessessary detaching for our old character and old socket
  if(socketGetChar(sock) != NULL)
    charSetSocket(socketGetChar(sock), NULL);
  if(charGetSocket(ch) != NULL)
    socketSetChar(charGetSocket(ch), NULL);
  charSetSocket(ch, sock);
  socketSetChar(sock, ch);
  return Py_BuildValue("i", 1);
}

//
// detaches a character from its socket
PyObject *mudsys_detach_char_socket(PyObject *self, PyObject *args) {
  PyObject    *pych = NULL;
  CHAR_DATA     *ch = NULL;
  SOCKET_DATA *sock = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError,"A character to be detached must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may be detached.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to detach nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }

  sock = charGetSocket(ch);
  if(sock != NULL)
    socketSetChar(sock, NULL);
  charSetSocket(ch, NULL);
  return Py_BuildValue("i", 1);
}

//
// quits a character from the game
PyObject *mudsys_do_quit(PyObject *self, PyObject *args) {
  PyObject *pych = NULL;
  CHAR_DATA  *ch = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError,"A character to be quitted must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may be quitted.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to quit nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }

  if(!charIsNPC(ch) && charGetSocket(ch)) {
    SOCKET_DATA *sock = charGetSocket(ch);
    charSetSocket(ch, NULL);
    socketSetChar(sock, NULL);
    socketPopInputHandler(sock);
    extract_mobile(ch);
  }

  return Py_BuildValue("i", 1);
}

//
// loads an account from disk
PyObject *mudsys_load_account(PyObject *self, PyObject *args) {
  char *name = NULL;
  
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "An account name must be supplied.");
    return NULL;
  }

  // newPyAccount increases the reference count on the account. We can
  // unreference it after we've created the Python wrapper for it
  ACCOUNT_DATA *acct = get_account(name);
  if(acct == NULL)
    return Py_BuildValue("O", Py_None);
  PyObject *retval = Py_BuildValue("O", accountGetPyFormBorrowed(acct));
  unreference_account(acct);
  return retval;
}

//
// loads a character from disk
PyObject *mudsys_load_char(PyObject *self, PyObject *args) {
  char *name = NULL;
  
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A character name must be supplied.");
    return NULL;
  }

  CHAR_DATA *ch = get_player(name);
  if(ch == NULL)
    return Py_BuildValue("O", Py_None);

  char_exist(ch);
  return Py_BuildValue("O", charGetPyFormBorrowed(ch));
}

//
// tries to put the player into the game
PyObject *mudsys_try_enter_game(PyObject *self, PyObject *args) {
  PyObject *pych = NULL;
  CHAR_DATA  *ch = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError,"A character to enter game must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may enter game.");
    return NULL;
  }

  if((ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried enter game with nonexistant character, %d.",
		 PyChar_AsUid(pych));
    return NULL;
  }

  // only enter game if we're not already in the game
  if(listIn(mobile_list, ch))
    return Py_BuildValue("i", 0);
  else
    return Py_BuildValue("i", try_enter_game(ch));
}

//
// disconnects a character from its socket
PyObject *mudsys_do_disconnect(PyObject *self, PyObject *args) {
  PyObject *pych = NULL;
  CHAR_DATA  *ch = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError,"A character to be dc'd must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may be dc'd.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to dc nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }

  if(charGetSocket(ch)) {
    SOCKET_DATA *sock = charGetSocket(ch);
    charSetSocket(ch, NULL);
    socketSetChar(sock, NULL);
    close_socket(sock, FALSE);
  }

  return Py_BuildValue("i", 1);
}

PyObject *mudsys_password_matches(PyObject *self, PyObject *args) {
  PyObject   *pyacct = NULL;
  char          *pwd = NULL;
  ACCOUNT_DATA *acct = NULL;

  if(!PyArg_ParseTuple(args, "Os", &pyacct, &pwd)) {
    PyErr_Format(PyExc_TypeError, "an account and password must be supplied.");
    return NULL;
  }

  if(!PyAccount_Check(pyacct)) {
    PyErr_Format(PyExc_TypeError, "only accounts may have passwords checked.");
    return NULL;
  }

  if( (acct = PyAccount_AsAccount(pyacct)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to check password for nonexistant account.");
    return NULL;
  }

  return Py_BuildValue("i", compares(crypt(pwd, accountGetName(acct)), 
				     accountGetPassword(acct)));
}

PyObject *mudsys_set_password(PyObject *self, PyObject *args) {
  PyObject   *pyacct = NULL;
  char          *pwd = NULL;
  ACCOUNT_DATA *acct = NULL;

  if(!PyArg_ParseTuple(args, "Os", &pyacct, &pwd)) {
    PyErr_Format(PyExc_TypeError, "an account and password must be supplied.");
    return NULL;
  }

  if(!PyAccount_Check(pyacct)) {
    PyErr_Format(PyExc_TypeError, "only accounts may have passwords set.");
    return NULL;
  }

  if( (acct = PyAccount_AsAccount(pyacct)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to set password for nonexistant account.");
    return NULL;
  }

  accountSetPassword(acct, crypt(pwd, accountGetName(acct)));
  return Py_BuildValue("i", 1);
}


//
// add a new command to the mud, via a python script or module. Takes in a
// command name, a sort_by command, the function, a minimum and maximum 
// position in the form of strings, a level, and boolean values for whether the
// command can be performed by mobiles, and whether it interrupts actions.
PyObject *mudsys_add_cmd(PyObject *self, PyObject *args) {
  PyObject *func = NULL;
  char *name  = NULL, *sort_by = NULL, *group = NULL;
  bool interrupts = FALSE;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "szOsb", &name, &sort_by, &func,
  			&group, &interrupts)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not add new command. Improper arguments supplied");
    return NULL;
  }

  // make sure it's a function object, and check its documentation to see if
  // we can add it as a helpfile
  if(PyFunction_Check(func)) {
    add_py_cmd(name, sort_by, func, group, interrupts);
#ifdef MODULE_HELP2
    if(get_help(name,FALSE) == NULL && 
       ((PyFunctionObject*)func)->func_doc != NULL &&
       PyString_Check(((PyFunctionObject*)func)->func_doc)) {
      BUFFER *buf = newBuffer(1);
      bufferCat(buf, PyString_AsString(((PyFunctionObject*)func)->func_doc));
      bufferFormat(buf, SCREEN_WIDTH, 0);
      if(bufferLength(buf) > 0)
	add_help(name, bufferString(buf), group, NULL, FALSE);
    }
#endif
  }

  return Py_BuildValue("O", Py_None);
}

//
// adds a check prior to the command's execution. If any check returns false,
// the command fails. Checks are assumed to tell people why their command
// failed.
PyObject *mudsys_add_cmd_check(PyObject *self, PyObject *args) {
  PyObject *func = NULL;
  char    *name  = NULL;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "sO", &name, &func)) {
    PyErr_Format(PyExc_TypeError, 
	       "Could not add new command check. Improper arguments supplied.");
    return NULL;
  }

  // add the command to the game
  add_py_cmd_check(name, func);
  return Py_BuildValue("O", Py_None);
}

//
// removes a command from the game
PyObject *mudsys_remove_cmd(PyObject *self, PyObject *args) {
  char *name = NULL;
  // parse all of the values
  if (!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "function requires string argument.");
    return NULL;
  }
  remove_cmd(name);
  return Py_BuildValue("O", Py_None);
}


PyObject *mudsys_handle_cmd_input(PyObject *self, PyObject *args) {
  PyObject  *pysock = NULL;
  SOCKET_DATA *sock = NULL;
  char         *cmd = NULL;

  if(!PyArg_ParseTuple(args, "Os", &pysock, &cmd)) {
    PyErr_Format(PyExc_TypeError, "A socket and command must be supplied.");
    return NULL;
  }

  if(!PySocket_Check(pysock)) {
    PyErr_Format(PyExc_TypeError, "A socket must be supplied.");
    return NULL;
  }

  if( (sock = PySocket_AsSocket(pysock)) == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to run command for nonexistent socket.");
    return NULL;
  }

  handle_cmd_input(sock, cmd);
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_show_prompt(PyObject *self, PyObject *args) {
  PyObject  *pysock = NULL;
  SOCKET_DATA *sock = NULL;

  if(!PyArg_ParseTuple(args, "O", &pysock)) {
    PyErr_Format(PyExc_TypeError, "A socket must be supplied.");
    return NULL;
  }

  if(!PySocket_Check(pysock)) {
    PyErr_Format(PyExc_TypeError, "A socket must be supplied.");
    return NULL;
  }

  if( (sock = PySocket_AsSocket(pysock)) == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to show prompt to nonexistent socket.");
    return NULL;
  }

  show_prompt(sock);
  return Py_BuildValue("i", 1);
}



//*****************************************************************************
// MudSys module
//*****************************************************************************
void PyMudSys_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pymudsys_methods == NULL) pymudsys_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pymudsys_methods, def);
}


PyMODINIT_FUNC
init_PyMudSys(void) {
  // add all of our methods
  PyMudSys_addMethod("do_shutdown", mudsys_shutdown, METH_VARARGS,
		     "shuts the mud down.");
  PyMudSys_addMethod("do_copyover", mudsys_copyover, METH_VARARGS,
		     "performs a copyover on the mud.");
  PyMudSys_addMethod("sys_setval", mudsys_set_sys_val, METH_VARARGS,
		     "sets a system value on the mud.");
  PyMudSys_addMethod("sys_getval", mudsys_get_sys_val, METH_VARARGS,
		     "returns a system value on the mud.");
  PyMudSys_addMethod("player_exists", mudsys_player_exists, METH_VARARGS,
		     "returns whether a player with the name exists.");
  PyMudSys_addMethod("account_exists", mudsys_account_exists, METH_VARARGS,
		     "returns whether an account with the name exists.");
  PyMudSys_addMethod("player_creating", mudsys_player_creating, METH_VARARGS,
		     "returns whether a player with the name is creating.");
  PyMudSys_addMethod("account_creating", mudsys_account_creating, METH_VARARGS,
		     "returns whether an account with the name is creating.");
  PyMudSys_addMethod("do_register", mudsys_do_register, METH_VARARGS,
		     "register a player to disk, and with an account.");
  PyMudSys_addMethod("load_char", mudsys_load_char, METH_VARARGS,
		     "load a characer from disk");
  PyMudSys_addMethod("load_account", mudsys_load_account, METH_VARARGS,
		     "load an account from disk");
  PyMudSys_addMethod("try_enter_game", mudsys_try_enter_game, METH_VARARGS,
		     "Tries to put the character into the game world");
  PyMudSys_addMethod("do_save", mudsys_do_save, METH_VARARGS,
		     "save a character to disk");
  PyMudSys_addMethod("do_quit", mudsys_do_quit, METH_VARARGS,
		     "quit a character from game");
  PyMudSys_addMethod("attach_account_socket",mudsys_attach_account_socket, 
		     METH_VARARGS, "attaches an account and socket.");
  PyMudSys_addMethod("attach_char_socket", mudsys_attach_char_socket, 
		     METH_VARARGS, "attaches a char and socket.");
  PyMudSys_addMethod("detach_char_socket", mudsys_detach_char_socket,
		     METH_VARARGS, "detachs a char from a socket.");
  PyMudSys_addMethod("do_disconnect", mudsys_do_disconnect, METH_VARARGS,
		     "disconnects a character from its socket");
  PyMudSys_addMethod("password_matches", mudsys_password_matches, METH_VARARGS,
		     "returns whether or not the password matches the account's"
		     " password.");
  PyMudSys_addMethod("set_password", mudsys_set_password, METH_VARARGS,
		     "sets an account's password.");
  PyMudSys_addMethod("add_cmd", mudsys_add_cmd, METH_VARARGS,
		     "Add a new command to the game.");
  PyMudSys_addMethod("add_cmd_check", mudsys_add_cmd_check, METH_VARARGS,
		     "Add a new check prior to a command running.");
  PyMudSys_addMethod("remove_cmd", mudsys_remove_cmd, METH_VARARGS,
		     "Removes a command from the game.");
  PyMudSys_addMethod("handle_cmd_input", mudsys_handle_cmd_input, METH_VARARGS,
		     "the default input handler for character commands.");
  PyMudSys_addMethod("show_prompt", mudsys_show_prompt, METH_VARARGS,
		     "the default character prompt. Can be replaced in Python "
		     "by assigning a new prompt to the same named variable.");
  PyMudSys_addMethod("create_account", mudsys_create_account, METH_VARARGS,
		     "creates a new account by name. Must be registered "
		     "after fully created.");
  PyMudSys_addMethod("create_player", mudsys_create_player, METH_VARARGS,
		     "creates a new player by name. Must be registered "
		     "after fully created.");

  Py_InitModule3("mudsys", makePyMethods(pymudsys_methods),
		 "The mudsys module, for all MUD system utils.");
}
