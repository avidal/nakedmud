#ifndef __SAVE_H
#define __SAVE_H
//*****************************************************************************
//
// save.h
//
// functions having to do with saving and loading pfiles and objfiles
//
//*****************************************************************************



void        save_player ( CHAR_DATA *ch );
CHAR_DATA  *load_player ( const char *player );
CHAR_DATA  *load_profile( const char *player );

#endif // __SAVE_H
