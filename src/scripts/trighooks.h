#ifndef TRIGHOOKS_H
#define TRIGHOOKS_H
//*****************************************************************************
//
// trighooks.h
//
// Triggers attach on to rooms, objects, and mobiles as hooks. When a hook
// event occurs, all of the triggers of the right type will run. This header is
// just to allow scripts to initialize the hooks into the game. The init 
// function here should not be touched by anything other than scripts.c
//
//*****************************************************************************

//
// called when init_scripts() is run
void init_trighooks(void);

#endif // TRIGHOOKS_H

