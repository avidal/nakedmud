#ifndef __HELP_H
#define __HELP_H
//*****************************************************************************
//
// help.h
//
// has all of the functions needed for interacting with the helpfile system.
//
//*****************************************************************************

void        load_helps        ();
void        reload_helps      ();
HELP_DATA  *get_help          (const char *keyword);
void        add_help          (HELP_DATA *help);


HELP_DATA  *newHelpEntry      (const char *keywords, const char *desc);
void        helpEntryCopyTo   (HELP_DATA *from, HELP_DATA *to);
HELP_DATA  *helpEntryCopy     (HELP_DATA *help);
void        show_help         (CHAR_DATA *ch, HELP_DATA *help);
char      **helpEntryDescPtr  (HELP_DATA *help);

void        setHelpEntryDesc  (HELP_DATA *help);

#endif // __HELP_H
