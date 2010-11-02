#ifndef __BUILDER_H
#define __BUILDER_H
//*****************************************************************************
//
// builder.h
//
// various utilities (non-OLC) for builders, such as digging/filling exits, and
// such.
//
//*****************************************************************************



//
// creates an exit in a direction to a specific room
// usage: dig [dir] [room]
//
COMMAND(cmd_dig);

//
// fills in an exit in a specific direction
// usage: fill [dir]
//
COMMAND(cmd_fill);

//
// Load a copy of a specific mob/object
// usage: load [mob | obj] [vnum]
//
COMMAND(cmd_load);

//
// Try creating a new room in the specified direction, drawn from the
// zone we're currently in
//
bool try_buildwalk(CHAR_DATA *ch, int dir);


#endif // __BUILDER_H
