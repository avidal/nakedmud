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
// creates a "middle-man" between hooks and the variables passed in when 
// hookRun is called. The supplied function should take a LIST of hooks to
// be run, and a VA_LIST of arguments. It should parse the arguments out of
// VA_LIST and call each hook in the LIST with the arguments found in VA_LIST.
void hook_add_handler(const char *type, 
		      void(* handler)(LIST *hooks, va_list args));

//
// some handlers for use by outside modules not wanting to write their own hook
// handlers. Each assumes a specific number of (void *) pointer arguments, and
// no return value. If you want to support arguments of other sizes (like ints,
// bools, etc) or would like your hooks to be able to return values, you will
// have to write your own handler.
void hook_handler_0_args(LIST *hooks, va_list args);
void  hook_handler_1_arg(LIST *hooks, va_list args);
void hook_handler_2_args(LIST *hooks, va_list args);
void hook_handler_3_args(LIST *hooks, va_list args);
void hook_handler_4_args(LIST *hooks, va_list args);
void hook_handler_5_args(LIST *hooks, va_list args);
void hook_handler_6_args(LIST *hooks, va_list args);

//
// This function attaches a hook to the game. It will run whenever a signal is
// sent that a hook of "type" should run. A hook is a function. The number and
// types of arguments that the function should take, as well as the type of 
// return value the hook should supply depends on the type of hook it's added as
void hookAdd(const char *type, void *hook);

//
// executes all of the hooks of the given type, with the given arguments. This
// is sort of a sloppy way to provide hooks with arguments, but it's the best
// I've come up with so far.
void hookRun(const char *type, ...);

//
// remove the given hook
void hookRemove(const char *type, void *hook);

#endif // HOOKS_H
