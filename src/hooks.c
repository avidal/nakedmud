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



//*****************************************************************************
// implementation of hooks.h
//*****************************************************************************
void init_hooks(void) {
  // make our table of hooks
  hook_table = newHashtable();
}

void hookAdd(const char *type, void *hook) {
  LIST *list = hashGet(hook_table, type);
  if(list == NULL) {
    list = newList();
    hashPut(hook_table, type, list);
  }
  listQueue(list, hook);
}

void hookRun(const char *type, void *actor, void *acted, void *arg) {
  LIST *list = hashGet(hook_table, type);
  if(list != NULL) {
    LIST_ITERATOR *hook_i = newListIterator(list);
    void (* hook)(void *actor, void *acted, void *arg) = NULL;
    ITERATE_LIST(hook, hook_i)
      hook(actor, acted, arg);
    deleteListIterator(hook_i);
  }
}

void hookRemove(const char *type, void *hook) {
  LIST *list = hashGet(hook_table, type);
  if(list != NULL) listRemove(list, hook);
}
