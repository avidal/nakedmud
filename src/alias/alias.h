#ifndef __ALIAS_H
#define __ALIAS_H
//*****************************************************************************
//
// alias.h
//
// Aliases allow a player to set up one command as another (e.g. "eat bread"
// as "food").
//
//*****************************************************************************

//
// This must be put at the top of mud.h so the rest of the MUD knows that
// we've got the alias module installed
// #define MODULE_ALIAS
//

//
// prepare aliases for use
//
void init_aliases();

const char  *charGetAlias  (CHAR_DATA *ch, const char *alias);
void         charSetAlias  (CHAR_DATA *ch, const char *alias, const char *cmd);

//
// expand an alias's parameters with the arguments provided. Returned
// string must be freed after being used.
//
char *expand_alias(const char *alias, const char *arg);

//
// list all of a character's aliases to him
//
COMMAND(cmd_alias);


#endif // __ALIAS_H
