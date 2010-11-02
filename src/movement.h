#ifndef MOVEMENT_H
#define MOVEMENT_H
//*****************************************************************************
//
// movement.h
//
// all of the functions assocciated with moving (chars and objects)
//
//*****************************************************************************

//
// Try to move in a specified direction. Returns the exit that was gone through
// if successful. A move might be false if a) there is a no (noticable) opened
// exit in the direction, the character is busy with some activity
// (.e.g fighting) that will not let him move, or his position is not one that
// is easily movable in (e.g. sleeping, resting, dead). The direction can be
// anything. Abbreviations of the normal directions (n, s, e, w) are handled
// properly by this function. Does not send messages to the rooms saying the
// person has entered/exited.
EXIT_DATA *try_move(CHAR_DATA *ch, const char *dir);

//
// Exactly the same as try_move, except this also sends messages saying the
// person has entered/exited.
EXIT_DATA *try_move_mssg(CHAR_DATA *ch, const char *dir);

#endif // MOVEMENT_H
