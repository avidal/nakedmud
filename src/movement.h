#ifndef __MOVEMENT_H
#define __MOVEMENT_H
//*****************************************************************************
//
// movement.h
//
// all of the functions assocciated with moving (chars and objects)
//
//*****************************************************************************



//
// Try to move in a specified direction. Returns true if the move was
// successful, and false otherwise. A move might be false if a) there is
// no (noticable) opened exit in the direction, the character is busy with
// some activity (e.g. fighting) that will not let him move, or his position
// is not one that is easily movable in (e.g. sleeping, resting, dead).
// if dir != DIR_NONE, then it is tried. Otherwise, specdir is tried
// as a room-specific direction to exit in.
//
bool try_move(CHAR_DATA *ch, int dir, const char *specdir);

//
// Go through an exit. use dir for display purposes to the room
// (e.g. bob leaves north). If dir == DIR_NONE, no direction info
// is said
//
bool try_exit(CHAR_DATA *ch, EXIT_DATA *exit, int dir);

//
// Try creating a new room in the specified direction, drawn from the
// zone we're currently in.
//
bool try_buildwalk(CHAR_DATA *ch, int dir);

#endif // __MOVEMENT_H
