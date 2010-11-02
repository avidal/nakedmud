//*****************************************************************************
//
// hooks.c
//
// Hooks are chunks of code that attach on to parts of game code, but aren't
// really parts of the game code. Hooks can be used for a number of things. For
// instance: processing room descriptions by outside modules before they are
// displayed to a character, running triggers when certain events happen, or
// perhaps logging how many time a room is entered or exited. We would probably
// not want to hard-code any of these things into the core of the mud if they
// are fairly stand-alone. So instead, we write hooks that attach into the game
// and execute when certain events happen.
//
// Often events that will execute hooks are set off by someone or something
// taking an action. Thus, all hooks take 3 arguments (actor, acted, arg) to
// make it easy to handle these cases. These 3 arguments do not need to be used
// for all hooks, however.
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "hooks.h"



//*****************************************************************************
// local functions, variables, and definitions
//*****************************************************************************

// the table of all our installed hooks
HASHTABLE *hook_table = NULL;

// a table of all our handlers for running hooks
HASHTABLE *hook_handler_table = NULL;



//*****************************************************************************
// implementation of hooks.h
//*****************************************************************************
void init_hooks(void) {
  // make our required tables
  hook_handler_table = newHashtable();
  hook_table         = newHashtable();

  // set up our basic types of hooks
  hook_add_handler("shutdown",             hook_handler_0_args);
  hook_add_handler("create_account",       hook_handler_1_arg);
  hook_add_handler("create_player",        hook_handler_1_arg);
  hook_add_handler("enter",                hook_handler_2_args);
  hook_add_handler("exit",                 hook_handler_3_args);
  hook_add_handler("ask",                  hook_handler_3_args);
  hook_add_handler("say",                  hook_handler_2_args);
  hook_add_handler("greet",                hook_handler_2_args);
  hook_add_handler("obj_to_game",          hook_handler_1_arg);
  hook_add_handler("char_to_game",         hook_handler_1_arg);
  hook_add_handler("room_to_game",         hook_handler_1_arg);
  hook_add_handler("obj_from_game",        hook_handler_1_arg);
  hook_add_handler("char_from_game",       hook_handler_1_arg);
  hook_add_handler("room_from_game",       hook_handler_1_arg);
  hook_add_handler("get",                  hook_handler_2_args);
  hook_add_handler("give",                 hook_handler_3_args);
  hook_add_handler("drop",                 hook_handler_2_args);
  hook_add_handler("wear",                 hook_handler_2_args);
  hook_add_handler("remove",               hook_handler_2_args);
  hook_add_handler("reset",                hook_handler_1_arg);
  hook_add_handler("open_door",            hook_handler_2_args);
  hook_add_handler("open_obj",             hook_handler_2_args);
  hook_add_handler("close_door",           hook_handler_2_args);
  hook_add_handler("close_obj",            hook_handler_2_args);
}

void hook_add_handler(const char *type, 
		      void (* handler)(LIST *hooks, va_list args)) {
  hashPut(hook_handler_table, type, handler);
}

void hookAdd(const char *type, void *hook) {
  LIST *list = hashGet(hook_table, type);
  if(list == NULL) {
    list = newList();
    hashPut(hook_table, type, list);
  }
  listQueue(list, hook);
}

void hookRun(const char *type, ...) {
  LIST *list = hashGet(hook_table, type);
  void (* handler)(LIST *hooks, va_list args) = 
    hashGet(hook_handler_table, type);
  if(list != NULL && handler != NULL) {
    va_list args;
    va_start(args, type);
    handler(list, args);
    va_end(args);
  }
}

void hookRemove(const char *type, void *hook) {
  LIST *list = hashGet(hook_table, type);
  if(list != NULL) listRemove(list, hook);
}

void hook_handler_0_args(LIST *hooks, va_list args) {
  LIST_ITERATOR *hook_i = newListIterator(hooks);
  void   (* hook)(void) = NULL;
  ITERATE_LIST(hook, hook_i) {
    hook();
  } deleteListIterator(hook_i);
}

void hook_handler_1_arg(LIST *hooks, va_list args) {
  LIST_ITERATOR *hook_i = newListIterator(hooks);
  void *arg1 = va_arg(args, void *);
  void (* hook)(void *) = NULL;
  ITERATE_LIST(hook, hook_i) {
    hook(arg1);
  } deleteListIterator(hook_i);
}

void hook_handler_2_args(LIST *hooks, va_list args) {
  LIST_ITERATOR *hook_i = newListIterator(hooks);
  void *arg1 = va_arg(args, void *);
  void *arg2 = va_arg(args, void *);
  void (* hook)(void *, void *) = NULL;
  ITERATE_LIST(hook, hook_i) {
    hook(arg1, arg2);
  } deleteListIterator(hook_i);
}

void hook_handler_3_args(LIST *hooks, va_list args) {
  LIST_ITERATOR *hook_i = newListIterator(hooks);
  void *arg1 = va_arg(args, void *);
  void *arg2 = va_arg(args, void *);
  void *arg3 = va_arg(args, void *);
  void (* hook)(void *, void *, void *) = NULL;
  ITERATE_LIST(hook, hook_i) {
    hook(arg1, arg2, arg3);
  } deleteListIterator(hook_i);
}

void hook_handler_4_args(LIST *hooks, va_list args) {
  LIST_ITERATOR *hook_i = newListIterator(hooks);
  void *arg1 = va_arg(args, void *);
  void *arg2 = va_arg(args, void *);
  void *arg3 = va_arg(args, void *);
  void *arg4 = va_arg(args, void *);
  void (* hook)(void *, void *, void *, void *) = NULL;
  ITERATE_LIST(hook, hook_i) {
    hook(arg1, arg2, arg3, arg4);
  } deleteListIterator(hook_i);
}

void hook_handler_5_args(LIST *hooks, va_list args) {
  LIST_ITERATOR *hook_i = newListIterator(hooks);
  void *arg1 = va_arg(args, void *);
  void *arg2 = va_arg(args, void *);
  void *arg3 = va_arg(args, void *);
  void *arg4 = va_arg(args, void *);
  void *arg5 = va_arg(args, void *);
  void (* hook)(void *, void *, void *, void *, void *) = NULL;
  ITERATE_LIST(hook, hook_i) {
    hook(arg1, arg2, arg3, arg4, arg5);
  } deleteListIterator(hook_i);
}

void hook_handler_6_args(LIST *hooks, va_list args) {
  LIST_ITERATOR *hook_i = newListIterator(hooks);
  void *arg1 = va_arg(args, void *);
  void *arg2 = va_arg(args, void *);
  void *arg3 = va_arg(args, void *);
  void *arg4 = va_arg(args, void *);
  void *arg5 = va_arg(args, void *);
  void *arg6 = va_arg(args, void *);
  void (* hook)(void *, void *, void *, void *, void *, void *) = NULL;
  ITERATE_LIST(hook, hook_i) {
    hook(arg1, arg2, arg3, arg4, arg5, arg6);
  } deleteListIterator(hook_i);
}
