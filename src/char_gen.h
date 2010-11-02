#ifndef CHAR_GEN_H
#define CHAR_GEN_H
//*****************************************************************************
//
// char_gen.h
//
// Contains all of the functions neccessary for generating a new character. 
// Different muds will handle this process in wildly different ways. As such,
// I figured it would be good to localize all of the login details to a single 
// file for people to tailor to their specific mud. It also makes updating to 
// newer versions a bit easier. Your character generation process should be
// laid out here. A basic framework is given, but it could use lots of work. In
// fact, it is crap and could use LOTS of work. I encourage people to write 
// their own login procedure and submit their designs to me so I can develop a 
// better general method for later versions of the mud.
//
//*****************************************************************************

//
// Start the character generation process!
void start_char_gen(SOCKET_DATA *sock);

#endif // CHAR_GEN_H
