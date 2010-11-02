#ifndef SCRIPTS_H
#define SCRIPTS_H
//*****************************************************************************
//
// scripts.h
//
// NakedMud makes extensive use of scripting. It uses scripting to generate
// objects, mobiles, and rooms when they are loaded into the game. There are
// also scripting hooks for these things (commonly referred to as triggers), 
// which allow them to be a bit more dynamic and flavorful in the game. For
// instance, greetings when someone enters a room, repsonses to questions, 
// actions when items are received.. you know... that sort of stuff. 
//
//*****************************************************************************

//
// these includes are needed by anything that deals with scripts. Might as
// well put them in here so people do not always have to include them manually
// One problem we run into is that Python.h needs to define a certain value for
// _POSIX_C_SOURCE. However, it never checks if _POSIX_C_SOURCE has already
// been defined. So, we have to do it here.
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#include <Python.h>
#include <structmember.h>

//
// this is also needed by anything that touches scripts
typedef struct trigger_data TRIGGER_DATA;

//
// called before scripts can be used
void  init_scripts(void);



//*****************************************************************************
// trigger function definitions
//*****************************************************************************

//
// triggers have 4 components: their name, their type, their key, and their
// code. A name is just a short description of what the trigger's purpose is.
// For instance, "bob's greeting trigger". The trigger type determines which
// sort of hook it installs itself as on mobs/chars/objs. The trigger's key is
// it's unique string identifier for lookup in the world databse. And, finally,
// its code is the python script that executes when this trigger is run.
TRIGGER_DATA  *newTrigger(void);
void        deleteTrigger(TRIGGER_DATA *trigger);
STORAGE_SET *triggerStore(TRIGGER_DATA *trigger);
TRIGGER_DATA *triggerRead(STORAGE_SET  *set);
void        triggerCopyTo(TRIGGER_DATA *from, TRIGGER_DATA *to);
TRIGGER_DATA *triggerCopy(TRIGGER_DATA *trigger);

//
// utilities for setting the trigger's fields
void triggerSetName(TRIGGER_DATA *trigger, const char *name);
void triggerSetType(TRIGGER_DATA *trigger, const char *type);
void  triggerSetKey(TRIGGER_DATA *trigger, const char *key);
void triggerSetCode(TRIGGER_DATA *trigger, const char *code);

//
// utilities for getting the trigger's fields
const char   *triggerGetName(TRIGGER_DATA *trigger);
const char   *triggerGetType(TRIGGER_DATA *trigger);
const char    *triggerGetKey(TRIGGER_DATA *trigger);
const char   *triggerGetCode(TRIGGER_DATA *trigger);
BUFFER *triggerGetCodeBuffer(TRIGGER_DATA *trigger);

//
// for getting lists of triggers installed on various things. Returns the
// trigger's key, and not the actual trigger. If an entry is removed from one
// of these lists, it must be freed afterwards.
LIST *charGetTriggers(CHAR_DATA *ch);
LIST *objGetTriggers (OBJ_DATA  *obj);
LIST *roomGetTriggers(ROOM_DATA *room);

//
// adds a trigger to the trigger list. Makes sure it's not a duplicate copy
void triggerListAdd(LIST *list, const char *trigger);

//
// removes a trigger with the given name from the trigger list. Frees the
// name of the removed trigger as needed.
void triggerListRemove(LIST *list, const char *trigger);

//
// if OLC is installed, these functions can be used for handling the editing
// of char/obj/room trigger lists. The functions for actually editing _triggers_
// are hidden, as they should not be needed by any other modules
#ifdef MODULE_OLC2
void trigger_list_menu   (SOCKET_DATA *sock, LIST *triggers);
int  trigger_list_chooser(SOCKET_DATA *sock, LIST *triggers,const char *option);
bool trigger_list_parser (SOCKET_DATA *sock, LIST *triggers, int choice,
			  const char *arg);
#endif // MODULE_OLC2



//*****************************************************************************
// general scripting
//*****************************************************************************

//
// create different sorts of dictionaries, depending on how secure we want them
// to be. Dictionaries must be deleted (Py_DECREF) after being used.
PyObject   *restricted_script_dict(void);
PyObject *unrestricted_script_dict(void);

//
// Runs an arbitrary block of python code using the given dictionary. If the
// script has a locale (i.e. zone) associated with it (for instance, running
// a trigger or a mob proto) locale can be set. Otherwise, locale should be NULL
void run_script(PyObject *dict, const char *script, const char *locale);

//
// returns the locale our script is running in. NULL if no scripts are running,
// and an empty string if there is no locale for the script
const char *get_script_locale(void);

//
// returns true if the last script ran without any errors
bool last_script_ok(void);

//
// This shows the script to the socket, and provides syntax highlighting
void script_display(SOCKET_DATA *sock, const char *script, bool show_line_nums);

//
// Python chokes on certain characters (e.g. \r) ... this fixes that
void format_script_buffer(BUFFER *script);

#endif // SCRIPTS_H
