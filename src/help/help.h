#ifndef __HELP_H
#define __HELP_H
//*****************************************************************************
//
// help.h
//
// This helpfile system has been set up to be amenable to allowing players to
// edit helpfiles without having to worry about malicious users. 
//
//*****************************************************************************

//
// This must be put at the top of mud.h so the rest of the MUD knows that
// we've got the help module installed
// #define MODULE_HELP
//


//
// prepare helpfiles for use
//
void init_help();


//
// Make changes to the content of the helpfile. The old info is backed up.
// To protect against malicious changes. update_help can be used to create
// new helpfiles as well.
//
void update_help(const char *editor, const char *keyword, const char *info);


//
// Get the content of the helpfile. Return NULL if none exists.
//
const char *get_help(const char *keyword);


//
// unlink a keyword from the help info. If no more links are left to the
// helpfile, the helpfile is deleted.
//
void unlink_help(const char *keyword);


//
// link a new keyword to a helpfile
//
void link_help(const char *keyword, const char *old_word);

#endif // __HELP_H
