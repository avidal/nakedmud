//*****************************************************************************
//
// account_handler.c
//
// The login and creation of accounts, and handles all of the account procedures
// load loading and deleting characters. If you are looking to change the char
// creation menu, you will want char_gen.c, not account_handler.c
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "socket.h"
#include "account.h"
#include "character.h"
#include "room.h"
#include "inform.h"
#include "save.h"
#include "char_gen.h"
#include "hooks.h"



// mccp support
const unsigned char do_echo   [] = { IAC, WONT, TELOPT_ECHO,      '\0' };
const unsigned char dont_echo [] = { IAC, WILL, TELOPT_ECHO,      '\0' };

// account input handlers
void account_ask_name       (SOCKET_DATA *sock, char *arg);
void account_ask_password   (SOCKET_DATA *sock, char *arg);
void account_new_password   (SOCKET_DATA *sock, char *arg);
void account_verify_password(SOCKET_DATA *sock, char *arg);
void account_handle_menu    (SOCKET_DATA *sock, char *arg);
void account_new_char       (SOCKET_DATA *sock, char *arg);
void account_change_password(SOCKET_DATA *sock, char *arg);

// and prompts
void account_menu           (SOCKET_DATA *sock);


//
// the head honcho. This is the first handler that a socket enters once it has
// connected to the game. You should use this function to get your login 
// procedure rolling.
void handle_new_connections(SOCKET_DATA *sock, char *arg) {
  account_ask_name(sock, arg);
}



//*****************************************************************************
// implementation of account input handlers
//*****************************************************************************

//
// returns TRUE if the account has a valid name, and FALSE if it does not.
// alphanumeric characters and digits are allows. Min length 4, max length 12.
// first character must be a letter
bool check_account_name(const char *name) {
  int i, len = strlen(name);
  if(len < 3 || len > 12)
    return FALSE;
  if(!isalpha(*name))
    return FALSE;
  for(i = 1; name[i] != '\0'; i++)
    if(!isdigit(name[i]) && !isalpha(name[i]))
      return FALSE;
  return TRUE;
}


//
// Asks for an account name. If this is a new account, then proceed to ask and
// verify the password. Otherwise, ask for an account password and make sure it
// matches the actual password.
void account_ask_name(SOCKET_DATA *sock, char *arg) {
  // make sure we're not still resolving the DNS
  if (socketGetDNSLookupStatus(sock) != TSTATE_DONE) {
    text_to_buffer(sock, 
		   "Making a dns lookup, please have patience.\n\r"
		   "What is your account name? ");
    return;
  }

  // check for a legal account name
  if (!check_account_name(arg)) {
    text_to_buffer(sock, 
		   "Sorry, that's not a legal account name. Your account name must only consist of\r\n"
		   "characters and numbers, and it must be between 4 and 12 characters long. The\r\n"
		   "first character MUST be a letter. Please pick another.\r\n"
		   "What is your name? ");
    return;
  }

  // all is good. See if the account exists already
  else {
    ACCOUNT_DATA *acct = NULL;
    log_string("Account '%s' is trying to connect.", arg);

    // check for new account
    if ( (acct = get_account(arg)) == NULL) {
      // check for lockdown
      if(*mudsettingGetString("lockdown") &&
	 !is_keyword(mudsettingGetString("lockdown"), DFLT_USER_GROUP, FALSE)) {
	text_to_socket(sock, "Sorry, creating new accounts is not allowed at the moment.\r\n");
	close_socket(sock, FALSE);
	return;
      }

      // make sure someone else is not creating an account with this name
      if(account_creating(arg)) {
	text_to_socket(sock, "Someone is already creating an account with "
		       "that name.\r\nTry again: ");
	return;
      }

      // make a new account and give it a name
      acct = newAccount();
      accountSetName(acct, arg);

      // prepare for next step 
      text_to_buffer(sock, "Please enter a new password: ");
      socketReplaceInputHandler(sock, account_new_password, NULL);
    }

    // old account
    else {
      // prepare for next step 
      text_to_buffer(sock, "What is your password? ");
      socketReplaceInputHandler(sock, account_ask_password, NULL);
    }

    // make sure the password entry is not displayed on screen
    text_to_buffer(sock, (char *)dont_echo);

    // attach the account to the socket
    socketSetAccount(sock, acct);
  }
}


//
// Ask for a password, and make sure it matches the account's password. If it
// does not, then dc the socket
void account_ask_password   (SOCKET_DATA *sock, char *arg) {
  // make sure we can start seeing what we type, again
  text_to_buffer(sock, (char *) do_echo);

  // Passwords match. Go to the account menu
  if (compares(crypt(arg, accountGetName(socketGetAccount(sock))), 
	       accountGetPassword(socketGetAccount(sock)))) {
    socketReplaceInputHandler(sock, account_handle_menu, account_menu);
  }
  else {
    text_to_socket(sock, "\007\007Bad password! Disconnecting.\r\n");
    close_socket(sock, FALSE);
  }
}


//
// Ask and verify a new password during account creation
void account_new_password   (SOCKET_DATA *sock, char *arg) {
  // make sure the password is an acceptable length
  if (strlen(arg) < 5 || strlen(arg) > 12) {
    text_to_buffer(sock, 
		   "\r\nBetween 5 and 12 chars please!\n\r"
		   "Please enter a new password: ");
    return;
  }

  // encrypt and set the password
  accountSetPassword(socketGetAccount(sock), 
		     crypt(arg, accountGetName(socketGetAccount(sock))));

  // move onto the next step
  text_to_buffer(sock, "\r\nPlease verify the password: ");
  socketReplaceInputHandler(sock, account_verify_password, NULL);
}


//
// Verify the password during account creation. If there is a password mismatch,
// then restart the password process.
void account_verify_password(SOCKET_DATA *sock, char *arg) {
  if (compares(crypt(arg, accountGetName(socketGetAccount(sock))), 
	       accountGetPassword(socketGetAccount(sock)))) {
    text_to_buffer(sock, (char *)do_echo);

    // account created. Register it, and plop it into the account menu. If it
    // is already created, we're just editing the password. So save changes.
    if(!account_exists(accountGetName(socketGetAccount(sock)))) {
      // run hooks for creating our account for the first time
      hookRun("create_account", 
	      hookBuildInfo("str", accountGetName(socketGetAccount(sock))));
      register_account(socketGetAccount(sock));
    }
    else
      save_account(socketGetAccount(sock));
    socketReplaceInputHandler(sock, account_handle_menu, account_menu);
  }
  else {
    accountSetPassword(socketGetAccount(sock), NULL);
    text_to_buffer(sock, 
		   "\r\nPassword mismatch!\n\r"
		   "Please enter a new password: ");
    socketReplaceInputHandler(sock, account_new_password, NULL);
  }
}

//
// Load up an existing character attached to the account. Enter the game
void account_load_char(SOCKET_DATA *sock, int ch_num) {
  ACCOUNT_DATA *acct = socketGetAccount(sock);
  char *ch_name      = NULL;
  if((ch_name = listGet(accountGetChars(acct), ch_num)) == NULL)
    text_to_buffer(sock, "Invalid choice!\r\n");
  else {
    CHAR_DATA *ch = check_reconnect(ch_name);
    // this character is already in-game. Disconnect them
    if(ch != NULL) { 
      // attach the character
      socketSetChar(sock, ch);
      charSetSocket(ch, sock);

      log_string("%s has reconnected.", charGetName(ch));

      // and let him enter the game. Replace the load character input handler
      // with the one for playing the game.
      socketPushInputHandler(sock, handle_cmd_input, show_prompt);
      text_to_buffer(sock, "You take over a body already in use.\n\r");
    }

    // hmmm... our pfile is missing!!
    else if ((ch = get_player(ch_name)) == NULL)
      text_to_socket(sock, "ERROR: Your pfile is missing!\n\r");

    // everything is OK
    else {
      // attach the new player 
      socketSetChar(sock, ch);
      charSetSocket(ch, sock);

      // make sure the mud isn't locked to this person
      if(*mudsettingGetString("lockdown") &&
	 !bitIsSet(charGetUserGroups(ch), mudsettingGetString("lockdown"))) {
	send_to_char(ch, "You are currently locked out of the mud.\r\n");
	unreference_player(ch);
	socketSetChar(sock, NULL);
	return;
      }
      
      // try putting the character into the game. Pop the input handler
      // if we cannot
      if(try_enter_game(ch)) {
	log_string("%s has entered the game.", charGetName(ch));
	// we're no longer in the creation process... attach the game
	// input handler
	socketPushInputHandler(sock, handle_cmd_input, show_prompt);

	text_to_buffer(sock, bufferString(motd));
	look_at_room(ch, charGetRoom(ch));

	// run entrance hooks
	hookRun("enter", hookBuildInfo("ch rm", ch, charGetRoom(ch)));
      }
      else {
	text_to_buffer(sock, "There was a problem entering the game. Try again later!\r\n");
	// do not extract, just delete. We failed to enter
	// the game, so there is no need to extract from the game.
	unreference_player(socketGetChar(sock));
	socketSetChar(sock, NULL);
      }
    }
  }
}


//
// Change the password on an already-existant account
void account_change_password(SOCKET_DATA *sock, char *arg) {
  if (compares(crypt(arg, accountGetName(socketGetAccount(sock))), 
	       accountGetPassword(socketGetAccount(sock)))) {
    text_to_buffer(sock, "\r\nPlease enter a new password: ");
    socketReplaceInputHandler(sock, account_new_password, NULL);
  }
  else {
    text_to_buffer(sock, "\r\nIncorrect password!\r\n");
    socketReplaceInputHandler(sock, account_handle_menu, account_menu);
  }
}


//
// handle all of the options in the account main menu
void account_handle_menu(SOCKET_DATA *sock, char *arg) {
  // are we trying to load up a character?
  if(isdigit(*arg))
    account_load_char(sock, atoi(arg));

  // we're doing a menu choice
  else switch(toupper(*arg)) {
  case 'Q':
    // quit the menu
    text_to_buffer(sock, "Come back soon!\r\n");
    save_account(socketGetAccount(sock));
    socketPopInputHandler(sock);
    break;

  case 'P':
    // change password
    text_to_buffer(sock, (char *)dont_echo);
    text_to_buffer(sock, "Enter old password: ");
    socketReplaceInputHandler(sock, account_change_password, NULL);
    break;

  case 'N':
    // make a new character
    if(*mudsettingGetString("lockdown") &&
       !is_keyword(mudsettingGetString("lockdown"), DFLT_USER_GROUP, FALSE)) {
      text_to_buffer(sock, "The MUD is currently locked to new players.\r\n");
      return;
    }
    start_char_gen(sock);
    break;

  default:
    text_to_buffer(sock, "Invalid choice!\r\n");
    break;
  }
}


//
// Display a list of all the account's characters to the account.
void display_account_chars(SOCKET_DATA *sock) {
  static char fmt[100];
  int print_room, num_cols = 3, i = 0;
  char *str = NULL;
  LIST_ITERATOR *ch_i =newListIterator(accountGetChars(socketGetAccount(sock)));

  print_room = (80 - 10*num_cols)/num_cols;
  sprintf(fmt, "  {c%%2d{g) %%-%ds%%s", print_room);

  text_to_buffer(sock, "\r\n{wPlay a Character:\r\n");
  ITERATE_LIST(str, ch_i) {
    send_to_socket(sock, fmt, i, str, (i % num_cols == (num_cols - 1) ? 
				       "\r\n" : "   "));
    i++;
  } deleteListIterator(ch_i);

  if(i % num_cols != 0)
    send_to_socket(sock, "\r\n");
}


void account_menu(SOCKET_DATA *sock) {
  // if we have some characters created, display them
  if(listSize(accountGetChars(socketGetAccount(sock))) > 0)
    display_account_chars(sock);

  // additional options? delete? who's online?
  text_to_buffer(sock,
		 "\r\n{wAdditional Options:\r\n"
		 "  {g[{cP{g]assword change\r\n"
		 "  {g[{cN{g]ew character\r\n"
		 "\r\n"
		 "Enter choice, or Q to quit:{n ");
}
