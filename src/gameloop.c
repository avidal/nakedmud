//*****************************************************************************
//
// gameloop.c
//
// contains the entrypoint for the MUD, plus various state-handling functions.
//
//*****************************************************************************

#include "mud.h"
#include "socket.h"
#include "world.h"
#include "character.h"
#include "object.h"
#include "utils.h"
#include "save.h"
#include "handler.h"
#include "inform.h"
#include "text_editor.h"
#include "log.h"
#include "races.h"
#include "action.h"
#include "event.h"
#include "auxiliary.h"
#include "storage.h"


// optional modules
#ifdef MODULE_FACULTY
#include "modules/faculty/faculty.h"
#endif
#ifdef MODULE_COMBAT
#include "modules/combat/combat.h"
#endif
#ifdef MODULE_TIME
#include "modules/time/mudtime.h"
#endif
#ifdef MODULE_SCRIPTS
#include "modules/scripts/script.h"
#endif
#ifdef MODULE_OLC
#include "modules/olc/olc.h"
#endif


/* mccp support */
const unsigned char do_echo         [] = { IAC, WONT, TELOPT_ECHO,      '\0' };
const unsigned char dont_echo       [] = { IAC, WILL, TELOPT_ECHO,      '\0' };


/* local procedures */
void game_loop    ( int control );
bool gameloop_end = FALSE;

/* intialize shutdown state */
bool shut_down    = FALSE;
int  control;

/* what port are we running on? */
int mudport       = -1;

/* global variables */
WORLD_DATA *gameworld = NULL;   /*  the gameworld, and ll the prototypes */
LIST * object_list = NULL;      /*  the list of all existing objects */
LIST * socket_list = NULL;      /*  the list of active sockets */
LIST * socket_free = NULL;      /*  the list of free sockets */
LIST * mobile_list = NULL;      /*  the list of existing mobiles */
PROPERTY_TABLE *mob_table = NULL;/* a table of mobs by UID, for quick lookup */
PROPERTY_TABLE *obj_table = NULL;


/*
 * This is where it all starts, nothing special.
 */
int main(int argc, char **argv)
{
  int i;
  bool fCopyOver = FALSE;

  /************************************************************/
  /*                      PARSE OPTIONS                       */
  /************************************************************/
  for(i = 1; i < argc-1; i++) {
    if(!strcasecmp(argv[i], "-copyover")) {
      fCopyOver = TRUE;
      control = atoi(argv[++i]);
    }
    else {
      char buf[SMALL_BUFFER];
      sprintf(buf, "Invalid argument, %s (#%d)\r\n", argv[i], i);
      perror(buf);
      return 1;
    }
  }

  // port number not supplied... just use default
  if(i > argc-1)
    mudport = DEFAULT_PORT;
  else if(i == argc-1)
    mudport = atoi(argv[argc-1]);
  else {
    perror("Arguments were not parsed properly. Could not start MUD.\r\n");
    return 1;
  }



  /************************************************************/
  /*               INITIALIZE WHAT WE NEED TO                 */
  /************************************************************/

  /* create our global lists */
  object_list = newList();
  socket_list = newList();
  socket_free = newList();
  mobile_list = newList();
  // if you've got more than 12000 or so mobs in your
  // game at a time, you may want to increase this 
  // value by a bit
  mob_table   = newPropertyTable(charGetUID, 3000);
  obj_table   = newPropertyTable(objGetUID,  3000);

  /* note that we are booting up */
  log_string("Program starting.");

  /* seed the random number generator */
  srand(time(0));

  /* get the current time */
  current_time = time(NULL);

  /* initialize our mud settings, such as top character id */
  log_string("Initializing MUD settings.");
  init_mud_settings();

  /* set up the stuff for auxiliary data */
  log_string("Preparing auxiliary data for usage.");
  init_auxiliaries();

  /* initialize our command table */
  log_string("Initializing command table.");
  init_commands();

  /* initialize actions */
  log_string("Initializing action handler.");
  init_actions();

  /* initialize events */
  log_string("Initializing event handler.");
  init_events();

  // change to the lib directory
  //log_string("Changing to lib directory.");
  //chdir("../lib");

#ifdef MODULE_SCRIPTS
  /* initialize the scripting language */
  log_string("Initializing scripts.");
  init_scripts();
#endif

#ifdef MODULE_OLC
  /* initialize online creation */
  log_string("Initializing OLC.");
  init_olc();
#endif

#ifdef MODULE_TIME
  log_string("Initializing game time.");
  init_time();
#endif

  /* initialize faculties */
#ifdef MODULE_FACULTY
  log_string("Initializing faculties.");
  init_faculties();
#endif

  /* initialize combat if we have faculties */
#ifdef MODULE_COMBAT
  log_string("Initializing combat.");
  init_combat();
#endif

  /* initialize the socket */
  if (!fCopyOver) {
    log_string("Initializing sockets.");
    control = init_socket();
  }

  /* load all of the logs */
  log_string("Initializing logging system.");
  init_logs();

  /* load all external data */
  log_string("Logging world and reconnecting copyover sockets.");
  load_muddata(fCopyOver);

  // force-pulse everything once
  log_string("Force-resetting world");
  worldForceReset(gameworld);

  // main game loop
  log_string("Entering game loop");
  game_loop(control);

#ifdef MODULE_SCRIPTS
  // stop the scripts
  finalize_scripts();
#endif

  // close down the socket
  close(control);

  // terminated without errors
  log_string("Program terminated without errors.");

  return 0;
}


int num_updates = 0;
//
// let all of our actions and events know that time has gone by.
// Also, give a chance of doing some resetting in the game.
//
void update_handler()
{
  // increment the number of updates we've done
  num_updates++;

  // pulse actions and events -> one pulse
  pulse_actions(1);
  pulse_events(1);

  // pulse world
  // we don't want to be on the same schedule as
  // everything else, that updates every PULSES_PER_SECOND. 
  // We want to be on a schedule that updates every minute or so.
  if(num_updates % PULSES_PER_SECOND * 60 == 0)
    worldPulse(gameworld);
}



void game_loop(int control)   
{
  SOCKET_DATA *dsock;
  static struct timeval tv;
  struct timeval last_time, new_time;
  extern fd_set fSet;
  fd_set rFd;
  long secs, usecs;
  LIST_ITERATOR *sock_i = newListIterator(socket_list);

  /* set this for the first loop */
  gettimeofday(&last_time, NULL);

  /* clear out the file socket set */
  FD_ZERO(&fSet);

  /* add control to the set */
  FD_SET(control, &fSet);

  /* copyover recovery */
  ITERATE_LIST(dsock, sock_i)
    FD_SET(dsock->control, &fSet);

  /* do this untill the program is shutdown */
  while (!shut_down)
  {
    /* set current_time */
    current_time = time(NULL);

    /* copy the socket set */
    memcpy(&rFd, &fSet, sizeof(fd_set));

    /* wait for something to happen */
    if (select(FD_SETSIZE, &rFd, NULL, NULL, &tv) < 0)
      continue;

    /* check for new connections */
    if (FD_ISSET(control, &rFd))
    {
      struct sockaddr_in sock;
      unsigned int socksize;
      int newConnection;

      socksize = sizeof(sock);
      if ((newConnection = accept(control, (struct sockaddr*) &sock, &socksize)) >=0)
        new_socket(newConnection);
    }

    /* poll sockets in the socket list */
    listIteratorReset(sock_i);

    // We cannot use ITERATE_LIST, because there is a chance we
    // will remove the current element we are on from the list,
    // making it impossible to carry on in the iterator.
    //    ITERATE_LIST(dsock, sock_i) {
    while( (dsock = listIteratorCurrent(sock_i)) != NULL) {
      listIteratorNext(sock_i);

      /*
       * Close sockects we are unable to read from.
       */
      if (FD_ISSET(dsock->control, &rFd) && !read_from_socket(dsock)) {
        close_socket(dsock, FALSE);
        continue;
      }

      /* Ok, check for a new command */
      next_cmd_from_buffer(dsock);

      /* Is there a new command pending ? */
      // I switched to a true/false variable instead of checking if the
      // first char was \0 so that people in OLC and in scripts can easily
      // put newlines down when they need to.
      if (dsock->cmd_read)
      {
        /* figure out how to deal with the incoming command */
        switch(dsock->state)
        {
	default:
	  bug("Descriptor in bad state.");
	  break;
	case STATE_NEW_NAME:
	case STATE_NEW_PASSWORD:
	case STATE_VERIFY_PASSWORD:
	case STATE_ASK_PASSWORD:
	case STATE_ASK_SEX:
	case STATE_ASK_RACE:
	  handle_new_connections(dsock, dsock->next_command);
	  break;
	case STATE_PLAYING:
	  handle_cmd_input(dsock, dsock->next_command);
	  break;
#ifdef MODULE_OLC
	case STATE_OLC:
	  olc_loop(dsock, dsock->next_command);
	  break;
#endif
	case STATE_TEXT_EDITOR:
	  text_editor_loop(dsock, dsock->next_command);
	  break;
        }
        dsock->next_command[0] = '\0';
	dsock->cmd_read = FALSE;
      }

      /* if the player quits or get's disconnected */
      if (dsock->state == STATE_CLOSED) continue;

      /* Send all new data to the socket and close it if any errors occour */
      if (!flush_output(dsock))
        close_socket(dsock, FALSE);
    }

    /* call the top-level update handler */
    update_handler();

    /*
     * Here we sleep out the rest of the pulse, thus forcing
     * SocketMud(tm) (NakedMud) to run at PULSES_PER_SECOND pulses each second.
     */
    gettimeofday(&new_time, NULL);

    // get the time right now, and calculate how long we should sleep
    usecs = (int) (last_time.tv_usec -  new_time.tv_usec) + 1000000 / PULSES_PER_SECOND;
    secs  = (int) (last_time.tv_sec  -  new_time.tv_sec);

    //
    // Now we make sure that 0 <= usecs < 1.000.000
    //
    while (usecs < 0)
    {
      usecs += 1000000;
      secs  -= 1;
    }
    while (usecs >= 1000000)
    {
      usecs -= 1000000;
      secs  += 1;
    }

    // if secs < 0 we don't sleep, since we have encountered a laghole
    if (secs > 0 || (secs == 0 && usecs > 0))
    {
      struct timeval sleep_time;

      sleep_time.tv_usec = usecs;
      sleep_time.tv_sec  = secs;

      if (select(0, NULL, NULL, NULL, &sleep_time) < 0)
        continue;
    }

    /* wait out the rest of the pulse */
    /*
    while(TRUE) {
      gettimeofday(&new_time, NULL);
      secs  = (int) (new_time.tv_sec - last_time.tv_sec);
      usecs = (int) (new_time.tv_usec - last_time.tv_usec);

      // check to see if we've gone over time time that is in PULSES_PER_SECOND
      if(secs * 1000000 + usecs >= 1000000 / PULSES_PER_SECOND)
	break;
    }
    */

    /* reset the last time we where sleeping */
    gettimeofday(&last_time, NULL);

    /* recycle sockets */
    recycle_sockets();
  }
}



//*****************************************************************************
//
// STATE HANDLER AND RELATED FUNCTIONS
//
// Below this point are functions to do with handling a socket's state
// (e.g. get new name, password, character creation stuff, etc...)
//
//*****************************************************************************

//
// Ask a new connection for a name. This is the first thing that is called
// whenever a new socket connects to us
//
void state_new_name(SOCKET_DATA *dsock, char *arg) {
  CHAR_DATA *p_new;

  if (dsock->lookup_status != TSTATE_DONE) {
    text_to_buffer(dsock, 
		   "Making a dns lookup, please have patience.\n\r"
		   "What is your name? ");
    return;
  }
  if (!check_name(arg)) /* check for a legal name */ {
    text_to_buffer(dsock, 
		   "Sorry, that's not a legal name, please pick another.\n\r"
		   "What is your name? ");
    return;//    break;
  }
  arg[0] = toupper(arg[0]);
  log_string("%s is trying to connect.", arg);


  /* Check for a new Player */
  if ((p_new = load_profile(arg)) == NULL) {
    p_new = newChar();

    /* give the player it's name */
    charSetName(p_new, arg);

    /* and room description. In the future, we may want
       to consider allowing players to seet their own rdescs */
    char rdesc[SMALL_BUFFER];
    sprintf(rdesc, "%s is here.", arg);
    charSetRdesc(p_new, rdesc);

    /* prepare for next step */
    text_to_buffer(dsock, "Please enter a new password: ");
    dsock->state = STATE_NEW_PASSWORD;
  }
  else /* old player */ {
    /* prepare for next step */
    text_to_buffer(dsock, "What is your password? ");
    dsock->state = STATE_ASK_PASSWORD;
  }
  text_to_buffer(dsock, (char *) dont_echo);

  /* socket <-> player */
  charSetSocket(p_new, dsock);
  dsock->player = p_new;
}


//
// Ask a character for a password. This is only called once, when
// the character first creates.
//
void state_new_password(SOCKET_DATA *dsock, char *arg) {
  int i;

  if (strlen(arg) < 5 || strlen(arg) > 12) {
    text_to_buffer(dsock, 
		   "\r\nBetween 5 and 12 chars please!\n\r"
		   "Please enter a new password: ");
    return;
  }
  charSetPassword(dsock->player, crypt(arg, charGetName(dsock->player)));

  for (i = 0; i < strlen(charGetPassword(dsock->player)); i++) {
    if (charGetPassword(dsock->player)[i] == '~') {
      text_to_buffer(dsock,
		     "Illegal password!\n\r"
		     "Please enter a new password: ");
      return;
    }
  }

  text_to_buffer(dsock, "\r\nPlease verify the password: ");
  dsock->state = STATE_VERIFY_PASSWORD;
}


//
// Verify that the character's new password is correct. This is
// only called once, when the character creates
//
void state_verify_password(SOCKET_DATA *dsock, char *arg) {
  if (compares(crypt(arg, charGetName(dsock->player)), 
	       charGetPassword(dsock->player))) {
    text_to_buffer(dsock, (char *) do_echo);

    /* onto the sex check */
    dsock->state = STATE_ASK_SEX;
    text_to_buffer(dsock, "\r\nWhat is your sex (M/F/N): ");
  }
  else {
    charSetPassword(dsock->player, NULL);
    text_to_buffer(dsock, 
		   "\r\nPassword mismatch!\n\r"
		   "Please enter a new password: ");
    dsock->state = STATE_NEW_PASSWORD;
  }
}

void list_races(SOCKET_DATA *sock) {
  int i;
  send_to_socket(sock, "Available races are:\r\n");
  for(i = 0; i < NUM_RACES; i++) {
    if(!raceIsForPC(i))
      continue;
    send_to_socket(sock, "%s%s", 
		   (i == 0 ? "" : ", "),
		   raceGetName(i));
  }
  send_to_socket(sock, "\r\n\r\nPlease enter your choice: ");
}

//
// Ask for the character's race. Called each time a character is created
//
void state_ask_race(SOCKET_DATA *dsock, char *arg) {
  int racenum = raceGetNum(arg);

  if(racenum == RACE_NONE || racenum >= NUM_RACES || !raceIsForPC(racenum)) {
    send_to_socket(dsock, "Invalid race! Please try again: ");
    return;
  }
  else {
    charSetRace(dsock->player, racenum);
    charResetBody(dsock->player);
  }

  /* give the character a unique id */
  charSetUID(dsock->player, next_char_uid());

  /* if it's the first player, set him as the highest level */
  if(charGetUID(dsock->player) == 1)
    charSetLevel(dsock->player, MAX_LEVEL);

  /* add the character to the game */
  char_to_game(dsock->player);

  log_string("New player: %s has entered the game.", 
	     charGetName(dsock->player));
    
  /* and into the game */
  dsock->state = STATE_PLAYING;
  text_to_buffer(dsock, motd);

  // we should do some checks here to make sure the start room exists
  char_to_room(dsock->player, worldGetRoom(gameworld, START_ROOM));
  look_at_room(dsock->player, charGetRoom(dsock->player));

#ifdef MODULE_SCRIPTS
  // check enterance scripts
  try_enterance_script(dsock->player, charGetRoom(dsock->player),
		       NULL, NULL);
#endif
}

//
// Ask for the character's sex. This will be called each time the
// character creates
//
void state_ask_sex(SOCKET_DATA *dsock, char *arg) {
  switch(*arg) {
  case 'f':
  case 'F':
    charSetSex(dsock->player, SEX_FEMALE);
    break;
  case 'm':
  case 'M':
    charSetSex(dsock->player, SEX_MALE);
    break;
  case 'n':
  case 'N':
    charSetSex(dsock->player, SEX_NEUTRAL);
    break;
  default:
    text_to_buffer(dsock, "\r\nInvalid sex. Try again (M/F/N) :");
    return;
  }

  /* onto the password check */
  dsock->state = STATE_ASK_RACE;
  list_races(dsock);
}


//
// Ask the character for his or her password. This will be called
// every time the character logs on, after the first time when the
// character creates
//
void state_ask_password(SOCKET_DATA *dsock, char *arg) {
  CHAR_DATA *p_new;

  text_to_buffer(dsock, (char *) do_echo);
  if (compares(crypt(arg, charGetName(dsock->player)), 
	       charGetPassword(dsock->player)))
  {
    if ((p_new = check_reconnect(charGetName(dsock->player))) != NULL) {
      // we do NOT want to extract, here... the player hasn't even
      // entered the game yet. We just want to DELETE the profile character
      deleteChar(dsock->player);

      /* attach the new player */
      dsock->player = p_new;
      charSetSocket(p_new, dsock);

      log_string("%s has reconnected.", charGetName(dsock->player));

      /* and let him enter the game */
      dsock->state = STATE_PLAYING;
      text_to_buffer(dsock, "You take over a body already in use.\n\r");
    }
    else if ((p_new = load_player(charGetName(dsock->player))) == NULL) {
      text_to_socket(dsock, "ERROR: Your pfile is missing!\n\r");
      // no extract, just delete! We haven't entered the game yet,
      // so there is no need to extract
      deleteChar(dsock->player);

      dsock->player = NULL;
      close_socket(dsock, FALSE);
      return;
    }
    else {
      // No extract, just delete. We have not entered the game yet,
      // so there is no need to extract from it.
      deleteChar(dsock->player);

      /* attach the new player */
      dsock->player = p_new;
      charSetSocket(p_new, dsock);

      // try putting the character into the game.
      // Close the socket if we fail
      if(try_enter_game(p_new)) {
	log_string("%s has entered the game.", charGetName(p_new));
	dsock->state = STATE_PLAYING;
	text_to_buffer(dsock, motd);
	look_at_room(p_new, charGetRoom(p_new));

#ifdef MODULE_SCRIPTS
	// check enterance scripts
	try_enterance_script(p_new, charGetRoom(p_new),
			     NULL, NULL);
#endif
      }
      else {
	// do not extract, just delete. We failed to enter
	// the game, so there is no need to extract from the game.
	deleteChar(dsock->player);

	dsock->player = NULL;
	close_socket(dsock, FALSE);
      }
    }
  }
  else {
    text_to_socket(dsock, "Bad password!\n\r");
    // do not extract, just delete. We have not entered the game
    // yet, so there is no need to extract from the game.
    deleteChar(dsock->player);

    dsock->player = NULL;
    close_socket(dsock, FALSE);
  }
}


void handle_new_connections(SOCKET_DATA *dsock, char *arg) {
  switch(dsock->state) {
  case STATE_NEW_NAME:
    state_new_name(dsock, arg);
    break;

  case STATE_NEW_PASSWORD:
    state_new_password(dsock, arg);
    break;
    
  case STATE_VERIFY_PASSWORD:
    state_verify_password(dsock, arg);
    break;
    
  case STATE_ASK_PASSWORD:
    state_ask_password(dsock, arg);
    break;

  case STATE_ASK_SEX:
    state_ask_sex(dsock, arg);
    break;

  case STATE_ASK_RACE:
    state_ask_race(dsock, arg);
    break;

  default:
    bug("Handle_new_connections: Bad state.");
    break;
  }
}
