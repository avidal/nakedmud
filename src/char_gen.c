//*****************************************************************************
//
// char_gen.c
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

#include "mud.h"
#include "utils.h"
#include "socket.h"
#include "account.h"
#include "character.h"
#include "world.h"
#include "save.h"
#include "races.h"
#include "handler.h"
#include "inform.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/script.h"



//*****************************************************************************
// local functions, defines, structures, and the like
//*****************************************************************************

// functions in the char creation procedure
void char_ask_name(SOCKET_DATA *sock, char *arg);
void char_ask_sex (SOCKET_DATA *sock, char *arg);
void char_ask_race(SOCKET_DATA *sock, char *arg);


//
// List all of the races available during creation
void list_races(SOCKET_DATA *sock) {
  send_to_socket(sock, "Available races are:\r\n");
  send_to_socket(sock, "%s", raceGetList(TRUE));
  send_to_socket(sock, "\r\n\r\nPlease enter your choice: ");
}


//
// Character names must be at least 4 characters long, and no more than 12.
// The name must only be composed of alphanumeric characters
bool check_char_name(const char *name) {
  int i, size = strlen(name);
  if (size < 3 || size > 12)
    return FALSE;
  for (i = 0; i < size; i++)
    if (!isalpha(name[i])) 
      return FALSE;
  return TRUE;
}


//
// Query for the character's name.
void char_ask_name(SOCKET_DATA *sock, char *arg) {
  CHAR_DATA *ch = NULL;

  // check for a legal name
  if(!check_char_name(arg))
    text_to_buffer(sock, 
		   "Sorry, that was an illegal name. Please pick another.\r\n"
		   "What is your name? ");
  // a character with this name already exists
  else if(char_exists(arg))
    text_to_buffer(sock,
		   "A character with this name already exists.\r\n"
		   "What is your name? ");
  // we're OK to make this new character
  else {
    ch = newChar();

    // give the player it's name 
    *arg = toupper(*arg);
    charSetName(ch, arg);

    // and room description. In the future, we may want
    // to consider allowing players to set their own rdescs 
    char rdesc[SMALL_BUFFER];
    sprintf(rdesc, "%s is here.", arg);
    charSetRdesc(ch, rdesc);

    // socket <-> player 
    charSetSocket(ch, sock);
    socketSetChar(sock, ch);

    // prepare for next step 
    text_to_buffer(sock, "What is your sex (M/F/N)? ");
    socketReplaceInputHandler(sock, char_ask_sex, NULL);
  }
}


//
// Ask for the character's sex. 
void char_ask_sex (SOCKET_DATA *sock, char *arg) {
  switch(toupper(*arg)) {
  case 'F':
    charSetSex(socketGetChar(sock), SEX_FEMALE);
    break;
  case 'M':
    charSetSex(socketGetChar(sock), SEX_MALE);
    break;
  case 'N':
    charSetSex(socketGetChar(sock), SEX_NEUTRAL);
    break;
  default:
    text_to_buffer(sock, "\r\nInvalid sex. Try again (M/F/N) :");
    return;
  }

  // onto race choosing
  socketReplaceInputHandler(sock, char_ask_race, NULL);
  list_races(sock);
}


//
// Pick a race for our new character
void char_ask_race(SOCKET_DATA *sock, char *arg) {
  if(!arg || !*arg || !raceIsForPC(arg))
    send_to_socket(sock, "Invalid race! Please try again: ");
  else {
    charSetRace(socketGetChar(sock), arg);
    charResetBody(socketGetChar(sock));

    // give the character a unique id 
    int next_char_uid = mudsettingGetInt("puid") + 1;
    mudsettingSetInt("puid", next_char_uid);
    charSetUID(socketGetChar(sock), next_char_uid);
    
    // if it's the first player, set him as the highest level 
    if(charGetUID(socketGetChar(sock)) == 1)
      charSetLevel(socketGetChar(sock), MAX_LEVEL);

    // add the character to the game 
    char_to_game(socketGetChar(sock));
    
    log_string("New player: %s has entered the game.", 
	       charGetName(socketGetChar(sock)));
    
    // and into the game 
    // pop the input handler for char creation and add the one for
    // playing the game
    socketReplaceInputHandler(sock, handle_cmd_input, show_prompt);
    text_to_buffer(sock, motd);
    
    // we should do some checks here to make sure the start room exists
    char_to_room(socketGetChar(sock), worldGetRoom(gameworld, START_ROOM));
    look_at_room(socketGetChar(sock), charGetRoom(socketGetChar(sock)));
    
    // and save him 
    save_player(socketGetChar(sock));
    
    //************************************************************
    //******                 VERY IMPORTANT                 ******
    //************************************************************
    // add this character to our account list
    accountPutChar(socketGetAccount(sock), charGetName(socketGetChar(sock)));
    save_account(socketGetAccount(sock));

    // check enterance scripts
    try_enterance_script(socketGetChar(sock), charGetRoom(socketGetChar(sock)), NULL);
  }
}



//*****************************************************************************
// implementation of char_gen.c
//*****************************************************************************
void start_char_gen(SOCKET_DATA *sock) {
  text_to_buffer(sock, "What is your character's name? ");
  socketPushInputHandler(sock, char_ask_name, NULL);
}
