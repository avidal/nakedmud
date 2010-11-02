//*****************************************************************************
//
// login.c
//
// Different muds will want to set up the login process in very different ways;
// as such, I figured it would be good to localize all of the login details to
// a single file for people to tailor to their specific mud. It also makes
// updating to newer versions a bit easier. Your login procedure should be laid
// out here. A basic framework is given, but it could use lots of work. In fact,
// it is crap and could use LOTS of work. I encourage people to write their own
// login procedure and submit their designs to me so I can develop a better
// general method for later versions of the mud.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "socket.h"
#include "character.h"
#include "handler.h"
#include "inform.h"
#include "world.h"
#include "races.h"
#include "save.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/script.h"



// mccp support
const unsigned char do_echo         [] = { IAC, WONT, TELOPT_ECHO,      '\0' };
const unsigned char dont_echo       [] = { IAC, WILL, TELOPT_ECHO,      '\0' };


// input handlers
void state_new_name       (SOCKET_DATA *dsock, char *arg);
void state_ask_password   (SOCKET_DATA *dsock, char *arg);
void state_new_password   (SOCKET_DATA *dsock, char *arg);
void state_verify_password(SOCKET_DATA *dsock, char *arg);
void state_ask_sex        (SOCKET_DATA *dsock, char *arg);
void state_ask_race       (SOCKET_DATA *dsock, char *arg);



//
// the head honcho. This is the first handler that a socket enters once it has
// connected to the game. You should use this function to get your login 
// procedure rolling.
void handle_new_connections(SOCKET_DATA *dsock, char *arg) {
  state_new_name(dsock, arg);
}




//*****************************************************************************
// Contained blow are the functions required for logging on an old character,
// or creating a new character. Hmmm, wouldn't it be nice if players had one 
// account where they could have all their multiple characters listed, as
// opposed to a bunch of different characters? Perhaps this is something to
// consider for later versions of the MUD.
//*****************************************************************************

//
// Ask a new connection for a name. This is the first thing that is called
// whenever a new socket connects to us
void state_new_name(SOCKET_DATA *dsock, char *arg) {
  CHAR_DATA *p_new;

  if (socketGetDNSLookupStatus(dsock) != TSTATE_DONE) {
    text_to_buffer(dsock, 
		   "Making a dns lookup, please have patience.\n\r"
		   "What is your name? ");
    return;
  }
  // check for a legal name
  if (!check_name(arg)) {
    text_to_buffer(dsock, 
		   "Sorry, that's not a legal name, please pick another.\n\r"
		   "What is your name? ");
    return; // break;
  }
  arg[0] = toupper(arg[0]);
  log_string("%s is trying to connect.", arg);


  // Check for a new Player 
  if ((p_new = load_profile(arg)) == NULL) {
    p_new = newChar();

    // give the player it's name 
    charSetName(p_new, arg);

    // and room description. In the future, we may want
    // to consider allowing players to set their own rdescs 
    char rdesc[SMALL_BUFFER];
    sprintf(rdesc, "%s is here.", arg);
    charSetRdesc(p_new, rdesc);

    // prepare for next step 
    text_to_buffer(dsock, "Please enter a new password: ");
    socketReplaceInputHandler(dsock, state_new_password, NULL);
  }
  // old player
  else {
    // prepare for next step 
    text_to_buffer(dsock, "What is your password? ");
    socketReplaceInputHandler(dsock, state_ask_password, NULL);
  }
  text_to_buffer(dsock, (char *)dont_echo);

  // socket <-> player 
  charSetSocket(p_new, dsock);
  socketSetChar(dsock, p_new);
}


//
// Ask a character for a password. This is only called once, when
// the character first creates.
void state_new_password(SOCKET_DATA *dsock, char *arg) {
  if (strlen(arg) < 5 || strlen(arg) > 12) {
    text_to_buffer(dsock, 
		   "\r\nBetween 5 and 12 chars please!\n\r"
		   "Please enter a new password: ");
    return;
  }
  charSetPassword(socketGetChar(dsock), 
		  crypt(arg, charGetName(socketGetChar(dsock))));

  //
  // Why is this here? Is this because of old pfiles that terminated strings
  // with a tilde?
  //   - Geoff, Mar 21/05
  if(strchr(charGetPassword(socketGetChar(dsock)), '~') != NULL) {
      text_to_buffer(dsock, "Illegal password!\n\r"
		            "Please enter a new password: ");
  }
  else {
    text_to_buffer(dsock, "\r\nPlease verify the password: ");
    socketReplaceInputHandler(dsock, state_verify_password, NULL);
  }
}


//
// Verify that the character's new password is correct. This is
// only called once, when the character creates
void state_verify_password(SOCKET_DATA *dsock, char *arg) {
  if (compares(crypt(arg, charGetName(socketGetChar(dsock))), 
	       charGetPassword(socketGetChar(dsock)))) {
    text_to_buffer(dsock, (char *) do_echo);

    // onto the sex check 
    text_to_buffer(dsock, "\r\nWhat is your sex (M/F/N): ");
    socketReplaceInputHandler(dsock, state_ask_sex, NULL);
  }
  else {
    charSetPassword(socketGetChar(dsock), NULL);
    text_to_buffer(dsock, 
		   "\r\nPassword mismatch!\n\r"
		   "Please enter a new password: ");
    socketReplaceInputHandler(dsock, state_new_password, NULL);
  }
}

void list_races(SOCKET_DATA *sock) {
  send_to_socket(sock, "Available races are:\r\n");
  send_to_socket(sock, "%s", raceGetList(TRUE));
  send_to_socket(sock, "\r\n\r\nPlease enter your choice: ");
}

//
// Ask for the character's race. Called each time a character is created
void state_ask_race(SOCKET_DATA *dsock, char *arg) {
  if(!arg || !*arg || !raceIsForPC(arg)) {
    send_to_socket(dsock, "Invalid race! Please try again: ");
    return;
  }    
  else {
    charSetRace(socketGetChar(dsock), arg);
    charResetBody(socketGetChar(dsock));
  }

  // give the character a unique id 
  charSetUID(socketGetChar(dsock), next_char_uid());

  // if it's the first player, set him as the highest level 
  if(charGetUID(socketGetChar(dsock)) == 1)
    charSetLevel(socketGetChar(dsock), MAX_LEVEL);

  // add the character to the game 
  char_to_game(socketGetChar(dsock));

  log_string("New player: %s has entered the game.", 
	     charGetName(socketGetChar(dsock)));
    
  // and into the game 
  // pop the input handler for char creation and add the one for
  // playing the game
  socketReplaceInputHandler(dsock, handle_cmd_input, show_prompt);
  text_to_buffer(dsock, motd);

  // we should do some checks here to make sure the start room exists
  char_to_room(socketGetChar(dsock), worldGetRoom(gameworld, START_ROOM));
  look_at_room(socketGetChar(dsock), charGetRoom(socketGetChar(dsock)));

  // and save him 
  save_player(socketGetChar(dsock));

  // check enterance scripts
  try_enterance_script(socketGetChar(dsock), charGetRoom(socketGetChar(dsock)), NULL);
}

//
// Ask for the character's sex. This will be called each time the
// character creates
void state_ask_sex(SOCKET_DATA *dsock, char *arg) {
  switch(*arg) {
  case 'f':
  case 'F':
    charSetSex(socketGetChar(dsock), SEX_FEMALE);
    break;
  case 'm':
  case 'M':
    charSetSex(socketGetChar(dsock), SEX_MALE);
    break;
  case 'n':
  case 'N':
    charSetSex(socketGetChar(dsock), SEX_NEUTRAL);
    break;
  default:
    text_to_buffer(dsock, "\r\nInvalid sex. Try again (M/F/N) :");
    return;
  }

  // onto the password check 
  socketReplaceInputHandler(dsock, state_ask_race, NULL);
  list_races(dsock);
}


//
// Ask the character for his or her password. This will be called
// every time the character logs on, after the first time when the
// character creates
void state_ask_password(SOCKET_DATA *dsock, char *arg) {
  CHAR_DATA *p_new;

  text_to_buffer(dsock, (char *) do_echo);
  if (compares(crypt(arg, charGetName(socketGetChar(dsock))), 
	       charGetPassword(socketGetChar(dsock))))
  {
    if ((p_new = check_reconnect(charGetName(socketGetChar(dsock)))) != NULL) {
      // we do NOT want to extract, here... the player hasn't even
      // entered the game yet. We just want to DELETE the profile character
      deleteChar(socketGetChar(dsock));

      // attach the new player 
      socketSetChar(dsock, p_new);
      charSetSocket(p_new, dsock);

      log_string("%s has reconnected.", charGetName(socketGetChar(dsock)));

      // and let him enter the game 
      // pop the input handler for char creation and add the one for
      // playing the game
      socketReplaceInputHandler(dsock, handle_cmd_input, show_prompt);
      text_to_buffer(dsock, "You take over a body already in use.\n\r");
    }
    else if ((p_new = load_player(charGetName(socketGetChar(dsock)))) == NULL) {
      text_to_socket(dsock, "ERROR: Your pfile is missing!\n\r");
      // no extract, just delete! We haven't entered the game yet,
      // so there is no need to extract
      deleteChar(socketGetChar(dsock));

      socketSetChar(dsock, NULL);
      close_socket(dsock, FALSE);
      return;
    }
    else {
      // No extract, just delete. We have not entered the game yet,
      // so there is no need to extract from it.
      deleteChar(socketGetChar(dsock));

      // attach the new player 
      socketSetChar(dsock, p_new);
      charSetSocket(p_new, dsock);

      // try putting the character into the game.
      // Close the socket if we fail
      if(try_enter_game(p_new)) {
	log_string("%s has entered the game.", charGetName(p_new));
	// we're no longer in the creation process... attach the game
	// input handler
	socketReplaceInputHandler(dsock, handle_cmd_input, show_prompt);

	text_to_buffer(dsock, motd);
	look_at_room(p_new, charGetRoom(p_new));

	// check enterance scripts
	try_enterance_script(p_new, charGetRoom(p_new), NULL);
      }
      else {
	// do not extract, just delete. We failed to enter
	// the game, so there is no need to extract from the game.
	deleteChar(socketGetChar(dsock));

	socketSetChar(dsock, NULL);
	close_socket(dsock, FALSE);
      }
    }
  }
  else {
    text_to_socket(dsock, "Bad password!\n\r");
    // do not extract, just delete. We have not entered the game
    // yet, so there is no need to extract from the game.
    deleteChar(socketGetChar(dsock));

    socketSetChar(dsock, NULL);
    close_socket(dsock, FALSE);
  }
}
