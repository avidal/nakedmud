#ifndef HOOKS_H
#define HOOKS_H
//*****************************************************************************
//
// hooks.h
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

//
// prepare hooks for use
void init_hooks(void);

//
// This function attaches a hook to the game. It will run whenever a signal is
// sent that a hook of "type" should run. Hooks take three arguments, some of
// which may be null depending on the hook:
//   void hook(actor, acted, arg)
//
// actor is the thing taking an action which is causing the hook to execute.
// For instance, a character asking something for an "ask" hook.
// 
// acted is the thing being acted upon. For instance, the person being asked
// a question in an "ask" hook.
//
// arg is some argument to the hook. Its type is determined by the type of
// hook it is. In the example of the "ask" hook, this might be the question
// being asked.
//
// init_hooks must be called before this function can be used.
void hookAdd(const char *type, void *hook);

//
// executes all of the hooks of the given type, with the given arguments.
void hookRun(const char *type, void *actor, void *acted, void *arg);

//
// remove the given hook
void hookRemove(const char *type, void *hook);

#endif // HOOKS_H
