#ifndef __SAVE_H
#define __SAVE_H
//*****************************************************************************
//
// save.h
//
// functions having to do with saving and loading pfiles and objfiles
//
//*****************************************************************************

void          save_account(ACCOUNT_DATA *account);
ACCOUNT_DATA *load_account(const char   *account);
bool          char_exists (const char   *name);
CHAR_DATA    *load_player (const char   *player);
void          save_player (CHAR_DATA    *ch);

#endif // __SAVE_H
