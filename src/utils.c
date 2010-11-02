/*
 * This file contains all sorts of utility functions used
 * all sorts of places in the code.
 */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

/* include main header file */
#include "mud.h"
#include "character.h"
#include "object.h"
#include "world.h"
#include "zone.h"
#include "room.h"
#include "exit.h"
#include "socket.h"
#include "utils.h"
#include "save.h"
#include "handler.h"
#include "inform.h"
#include "help.h"
#include "dialog.h"
#include "event.h"
#include "action.h"


// optional modules
#ifdef MODULE_FACULTY
#include "modules/faculty/faculty.h"
#endif
#ifdef MODULE_COMBAT
#include "modules/combat/combat.h"
#endif
#ifdef MODULE_SCRIPTS
#include "modules/scripts/script.h"
#endif

/*
 * Check to see if a given name is
 * legal, returning FALSE if it
 * fails our high standards...
 */
bool check_name(const char *name)
{
  int size, i;

  if ((size = strlen(name)) < 3 || size > 12)
    return FALSE;

  for (i = 0 ;i < size; i++)
    if (!isalpha(name[i])) return FALSE;

  return TRUE;
}

void extract_obj(OBJ_DATA *obj) {
  // make sure there's no events going that involve us
  interrupt_events_involving(obj);

  // make sure we're not attached to anything
  CHAR_DATA *sitter = NULL;
  while( (sitter = listGet(objGetUsers(obj), 0)) != NULL)
    char_from_furniture(sitter);

  OBJ_DATA *content = NULL;
  while( (content = listGet(objGetContents(obj), 0)) != NULL) {
    obj_from_obj(content);
    extract_obj(content);
  }

  if(objGetRoom(obj))
    obj_from_room(obj);
  if(objGetCarrier(obj))
    obj_from_char(obj);
  if(objGetWearer(obj))
    try_unequip(objGetWearer(obj), obj);
  if(objGetContainer(obj))
    obj_from_obj(obj);

  obj_from_game(obj);
  deleteObj(obj);
}


void extract_mobile(CHAR_DATA *ch) {
  // interrupt actions and events associated with us
#ifdef MODULE_FACULTY
  interrupt_action(ch, FACULTY_ALL);
  clear_faculties(ch);
#else
  interrupt_action(ch, 1);
#endif
  interrupt_events_involving(ch);

#ifdef MODULE_COMBAT
  stop_all_targetting(ch);
  stop_combat(ch);
#endif

  // unequip everything the character is wearing
  // and send it to inventory
  unequip_all(ch);
  // extract everything in the character's inventory
  OBJ_DATA *obj = NULL;
  while( (obj = listGet(charGetInventory(ch), 0)) != NULL) {
    obj_from_char(obj);
    extract_obj(obj);
  }

  // make sure we're not attached to anything
  if(charGetFurniture(ch))
    char_from_furniture(ch);
  if(charGetRoom(ch))
    char_from_room(ch);

  // make sure we close down the socket
  if(charGetSocket(ch) != NULL) {
    close_socket(charGetSocket(ch), FALSE);
    charSetSocket(ch, NULL);
  }

  char_from_game(ch);
  deleteChar(ch);
}

void communicate(CHAR_DATA *dMob, char *txt, int range)
{
  switch(range) {
  default:
    bug("Communicate: Bad Range %d.", range);
    return;
  case COMM_LOCAL:  /* everyone in the same room */
    send_to_char(dMob, "{yYou say, '%s'{n\r\n", txt);
    message(dMob, NULL, NULL, NULL, FALSE, TO_ROOM | TO_NOTCHAR,
	    "{y$n says, '%s'", txt);
    try_dialog_all(dMob, roomGetCharacters(charGetRoom(dMob)), txt);
#ifdef MODULE_SCRIPTS
    try_speech_script(dMob, NULL, txt);
#endif
    break;

  case COMM_GLOBAL: /* everyone in the world */
    send_to_char(dMob, "{cYou chat, '%s'{n\r\n", txt);
    message(dMob, NULL, NULL, NULL, FALSE, TO_WORLD | TO_NOTCHAR,
	    "{c$n chats, '%s'", txt);
    break;

  case COMM_LOG: {
    CHAR_DATA *xMob;
    LIST_ITERATOR *mob_i = newListIterator(mobile_list);
    ITERATE_LIST(xMob, mob_i) {
      if (!IS_ADMIN(xMob)) continue;
      send_to_char(xMob, "[LOG: %s]\r\n", txt);
    }
    deleteListIterator(mob_i);
    break;
  }
  }
}


/*
 * Loading of help files, areas, etc, at boot time.
 */
void load_muddata(bool fCopyOver)
{  
  load_helps();
  gameworld = worldLoad(WORLD_PATH);
  if(gameworld == NULL) {
    log_string("ERROR: Could not boot game world.");
    abort();
  }

  /* copyover */
  if (fCopyOver)
    copyover_recover();
}

char *get_time()
{
  static char buf[16];
  char *strtime;
  int i;

  strtime = ctime(&current_time);
  for (i = 0; i < 15; i++)   
    buf[i] = strtime[i + 4];
  buf[15] = '\0';

  return buf;
}

// try entering into the game
// disconnect the socket if we fail.
bool try_enter_game(CHAR_DATA *ch) {
  // find the loadroom
  ROOM_DATA *loadroom = worldGetRoom(gameworld, charGetLoadroom(ch));

  // if the loadroom doesn't exist, 
  // try loading the character to the start room
  if(loadroom == NULL) {
    send_to_char(ch, 
		 "Your loadroom seems to be missing. Attempting to "
		 "load you to the start room.\r\n");
    log_string("ERROR: %s had invalid loadroom, %d", 
	       charGetName(ch), charGetLoadroom(ch));
    loadroom = worldGetRoom(gameworld, START_ROOM);
  }
  
  // we have a loadroom... all is good
  if(loadroom != NULL) {
    // add him to the game
    char_to_game(ch);
    char_to_room(ch, loadroom);
    return TRUE;
  }

  // oops! No rooms seem to exist
  else {
    send_to_char(ch,
		 "The gameworld was not properly loaded. The MUD may "
		 "be going through maintenance. Try again later.\r\n");
    log_string("ERROR: When loading %s, start room did not exist!",
	       charGetName(ch));

    return FALSE;
  }
}


/* Recover from a copyover - load players */
void copyover_recover()
{     
  CHAR_DATA *dMob;
  SOCKET_DATA *dsock;
  FILE *fp;
  char name [100];
  char host[MAX_BUFFER];
  int desc;
      
  log_string("Copyover recovery initiated");
   
  if ((fp = fopen(COPYOVER_FILE, "r")) == NULL)
  {  
    log_string("Copyover file not found. Exitting.");
    exit (1);
  }
      
  /* In case something crashes - doesn't prevent reading */
  unlink(COPYOVER_FILE);
    
  for (;;)
  {  
    fscanf(fp, "%d %s %s\n", &desc, name, host);
    if (desc == -1)
      break;

    dsock = malloc(sizeof(*dsock));
    clear_socket(dsock, desc);
  
    dsock->hostname     =  strdup(host);
    listPut(socket_list, dsock);
 
    /* load player data */
    if ((dMob = load_player(name)) != NULL)
    {
      /* attach to socket */
      charSetSocket(dMob, dsock);
      dsock->player    =  dMob;

      // try putting the character into the game
      // close the socket if we fail.
      try_enter_game(dMob);
    }
    else /* ah bugger */
    {
      close_socket(dsock, FALSE);
      continue;
    }
   
    /* Write something, and check if it goes error-free */
    if (!text_to_socket(dsock, "\n\r <*>  And before you know it, everything has changed  <*>\n\r"))
    { 
      close_socket(dsock, FALSE);
      continue;
    }
  
    /* make sure the socket can be used */
    dsock->bust_prompt    =  TRUE;
    dsock->lookup_status  =  TSTATE_DONE;
    dsock->state          =  STATE_PLAYING;

    /* negotiate compression */
    text_to_buffer(dsock, (char *) compress_will2);
    text_to_buffer(dsock, (char *) compress_will);
  }
  fclose(fp);
}     

CHAR_DATA *check_reconnect(const char *player)
{
  CHAR_DATA *dMob;
  LIST_ITERATOR *mob_i = newListIterator(mobile_list);

  ITERATE_LIST(dMob, mob_i) {
    if (!charIsNPC(dMob) && charIsName(dMob, player)) {
      if (charGetSocket(dMob)) {
        close_socket(charGetSocket(dMob), TRUE);
	charSetSocket(dMob, NULL);
      }
      break;
    }
  }

  deleteListIterator(mob_i);
  return dMob;
}


int   can_see_hidden          ( CHAR_DATA *ch) {
  return 0;
}

int   can_see_invis           ( CHAR_DATA *ch) {
  return 0;
}

bool  can_see_person          ( CHAR_DATA *ch, CHAR_DATA *target) {
  if(ch == target)
    return TRUE;
  if(poscmp(charGetPos(ch), POS_SLEEPING) <= 0)
    return FALSE;

  return TRUE;
}

bool  can_see_obj             ( CHAR_DATA *ch, OBJ_DATA  *target) {
  if(poscmp(charGetPos(ch), POS_SLEEPING) <= 0)
    return FALSE;

  return TRUE;
}

bool  can_see_exit         ( CHAR_DATA *ch, EXIT_DATA *exit) {
  if(poscmp(charGetPos(ch), POS_SLEEPING) <= 0)
    return FALSE;
  if(exitGetHidden(exit) > can_see_hidden(ch))
    return FALSE;
  return TRUE;
}


int count_objs(CHAR_DATA *looker, LIST *list, const char *name, int vnum,
	       bool must_see) {
  LIST_ITERATOR *obj_i = newListIterator(list);
  OBJ_DATA *obj;
  int count = 0;

  ITERATE_LIST(obj, obj_i) {
    if(must_see && !can_see_obj(looker, obj))
      continue;
    // if we have a name, search by it
    if(name && *name && objIsName(obj, name))
      count++;
    // otherwise search by vnum
    else if(!name && objGetVnum(obj) == vnum)
      count++;
  }
  deleteListIterator(obj_i);
  return count;
}


int count_chars(CHAR_DATA *looker, LIST *list, const char *name, int vnum,
		bool must_see) {
  LIST_ITERATOR *char_i = newListIterator(list);
  CHAR_DATA *ch;
  int count = 0;

  ITERATE_LIST(ch, char_i) {
    if(must_see && !can_see_person(looker, ch))
      continue;
    // if we have a name, search by it
    if(name && *name && charIsName(ch, name))
      count++;
    // otherwise, search by vnum
    else if(!name && charGetVnum(ch) == vnum)
      count++;
  }
  deleteListIterator(char_i);

  return count;
}


//
// find the numth occurance of the character with name, "name"
// if name is not supplied, search by vnum
//
CHAR_DATA *find_char(CHAR_DATA *looker, LIST *list, int num, const char *name,
		     int vnum, bool must_see) {
  if(num <= 0)
    return NULL;

  LIST_ITERATOR *char_i = newListIterator(list);
  CHAR_DATA *ch;

  ITERATE_LIST(ch, char_i) {
    if(must_see && !can_see_person(looker, ch))
      continue;
    // if we have a name, search by name
    if(name && *name && charIsName(ch, name))
      num--;
    // otherwise search by vnum
    else if(!name && charGetVnum(ch) == vnum)
      num--;
    if(num == 0)
      break;
  }
  deleteListIterator(char_i);
  return ch;
}


//
// find the numth occurance of the object with name, "name"
// if name is not supplied, search by vnum
//
OBJ_DATA *find_obj(CHAR_DATA *looker, LIST *list, int num, 
		   const char *name, int vnum, bool must_see) {
  if(num == 0)
    return NULL;

  LIST_ITERATOR *obj_i = newListIterator(list);
  OBJ_DATA *obj;

  ITERATE_LIST(obj, obj_i) {
    if(must_see && !can_see_obj(looker, obj))
      continue;
    // if a name is supplied, search by name
    if(name && *name && objIsName(obj, name))
      num--;
    // otherwise, search by vnum
    if(!name && objGetVnum(obj) == vnum)
      num--;
    if(num == 0)
      break;
  }
  deleteListIterator(obj_i);
  return obj;
}


//
// Find all characters in the list with the given name. Or all characters
// if name is NULL
//
LIST *find_all_chars(CHAR_DATA *looker, LIST *list, const char *name,
		     int vnum, bool must_see) {
  LIST_ITERATOR *char_i = newListIterator(list);
  LIST *char_list = newList();
  CHAR_DATA *ch;

  ITERATE_LIST(ch, char_i) {
    if(must_see && !can_see_person(looker, ch))
      continue;
    if(name && (!*name || charIsName(ch, name)))
      listPut(char_list, ch);
    else if(!name && charGetVnum(ch) == vnum)
      listPut(char_list, ch);
  }
  deleteListIterator(char_i);
  return char_list;
}


//
// Returns a llist of all the chars found with a specific name in the list.
// the list must be deleted after use.
//
LIST *find_all_objs(CHAR_DATA *looker, LIST *list, const char *name, 
		    int vnum, bool must_see) {

  LIST_ITERATOR *obj_i = newListIterator(list);
  LIST *obj_list = newList();
  OBJ_DATA *obj;

  ITERATE_LIST(obj, obj_i) {
    if(must_see && !can_see_obj(looker, obj))
      continue;
    if(name && (!*name || objIsName(obj, name)))
      listPut(obj_list, obj);
    else if(!name && objGetVnum(obj) == vnum)
      listPut(obj_list, obj);
  }
  deleteListIterator(obj_i);
  return obj_list;
}

//
// separates a joint item-count (e.g. 2.sword, all.woman) into its
// count and its word
//
void get_count(const char *buf, char *target, int *count) {
  // hmmm... we can probably condense these two checks into one, non?
  if(!strncasecmp(buf, "all.", 4)) {
    sscanf(buf, "all.%s", target);
    *count = COUNT_ALL;
  }
  else if(!strncasecmp(buf, "all", 3)) {
    *target = '\0';
    *count = COUNT_ALL;
  }
  else if(!strstr(buf, ".")) {
    sscanf(buf, "%s", target);
    *count = 1;
  }
  else
    sscanf(buf, "%d.%s", count, target);
}


//
// concats an item count and a target into a single term
//
void print_count(char *buf, const char *target, int count) {
  if(count > 1)               
    sprintf(buf, "%d.%s", count, target);
  else if(count == COUNT_ALL && *target)
    sprintf(buf, "all.%s", target);
  else if(count == COUNT_ALL)
    sprintf(buf, "all");
  else          
    sprintf(buf, "%s", target);
}


//
// Calculates how many letters until we hit the next one
// of a specified type
//
int next_letter_in(const char *string, char marker) {
  int i = 0;
  for(i = 0; string[i] != '\0'; i++) {
    if(string[i] == marker)
      break;
  }
  return i;
}


//
// Calculates how many characters until we hit the next space
//
int next_space_in(char *string) {
  return next_letter_in(string, ' ');
}



void format_string(char **string, int max_width, 
		   unsigned int maxlen, bool indent) {
  char formatted[MAX_BUFFER];
  bool needs_capital = TRUE;
  bool needs_indent  = FALSE; // no indent on the first line, unless
                              // we get the OK from the indent parameter
  int format_i = 0, string_i = 0, col = 0;

  // put in our indent
  if(indent) {
    sprintf(formatted, "   ");
    format_i += 3;
    col      += 3;

    // skip the leading spaces
    while(isspace((*string)[string_i]) && (*string)[string_i] != '\0')
      string_i++;
  }


  for(; (*string)[string_i] != '\0'; string_i++) {

    // we have to put a newline in because
    // the word won't fit on the line
    if(col + next_space_in((*string)+string_i) > max_width-2) {
      formatted[format_i] = '\r'; format_i++;
      formatted[format_i] = '\n'; format_i++;
      col = 0;
    }

    char ch = (*string)[string_i];

    // no spaces on newlines
    if(isspace(ch) && col == 0)
      continue;
    // we will do our own sentance formatting
    else if(needs_capital && isspace(ch))
      continue;
    // delete multiple spaces
    else if(isspace(ch) && format_i > 0 && isspace(formatted[format_i-1]))
      continue;
    // treat newlines as spaces (to separate words on different lines), 
    // since we are creating our own newlines
    else if(ch == '\r' || ch == '\n') {
      // we've already spaced
      if(col == 0)
	continue;
      formatted[format_i] = ' ';
      col++;
    }
    // if someone is putting more than 1 sentence delimiter, we
    // need to catch it so we will still capitalize the next word
    else if(strchr("?!.", ch)) {
      needs_capital = TRUE;
      needs_indent  = TRUE;
      formatted[format_i] = ch;
      col++;
    }
    // see if we are the first letter after the end of a sentence
    else if(needs_capital) {
      // check if indenting will make it so we don't
      // have enough room to print the word. If that's the
      // case, then skip down to a new line instead
      if(col + 2 + next_space_in((*string)+string_i) > max_width-2) {
	formatted[format_i] = '\r'; format_i++;
	formatted[format_i] = '\n'; format_i++;
	col = 0;
      }
      // indent two spaces if we're not at the start of a line 
      else if(needs_indent && string_i-1 >= 0 && (*string)[string_i-1] != '\n'){
	formatted[format_i] = ' '; format_i++;
	formatted[format_i] = ' '; format_i++;
	col += 2;
      }
      // capitalize the first letter on the new word
      formatted[format_i] = toupper(ch);
      needs_capital = FALSE;
      needs_indent  = FALSE;
      col++;
    }
    else {
      formatted[format_i] = ch;
      col++;
    }

    format_i++;
  }

  // tag a newline onto the end of the string if there isn't one already
  if(format_i > 0 && formatted[format_i-1] != '\n') {
    formatted[format_i++] = '\r';
    formatted[format_i++] = '\n';
  }

  formatted[format_i] = '\0';
  free(*string);
  *string = strdup(formatted);
}

//
// counts how many times ch occurs in string
//
int count_letters(const char *string, const char ch, const int strlen) {

  int i, n;
  for(i = n = 0; i < strlen; i++)
    if(string[i] == ch)
      n++;

  return n;
}

//
// trim trailing and leading whitespace
//
void trim(char *string) {
  int len = strlen(string);
  int max = len-1;
  int min = 0;
  int i;

  // kill all whitespace to the right
  for(max = len - 1; max >= 0; max--) {
    if(isspace(string[max]))
      string[max] = '\0';
    else
      break;
  }

  // find our first non-whitespace
  while(isspace(string[min]))
    min++;

  // shift everything to the left
  for(i = 0; i <= max-min; i++)
    string[i] = string[i+min];
  string[i] = '\0';
}


//
// Returns true if "word" is found in the comma-separated list of
// keywords
//
bool is_keyword(const char *keywords, const char *word, bool abbrev_ok) {
  int word_len = strlen(word);
  if(word_len < 1)
    return FALSE;

  // while we haven't reached the end of the string
  while(*keywords != '\0') {
    // skip all spaces and commas
    while(isspace(*keywords) || *keywords == ',')
      keywords = keywords+1;
    // see if we compare to the current keyword
    if(!abbrev_ok && !strncasecmp(keywords, word,next_letter_in(keywords, ',')))
      return TRUE;
    if(abbrev_ok && !strncasecmp(keywords, word, word_len))
      return TRUE;
    // we didn't. skip this current keyword, and find
    // the start of the next keyword or the end of the string
    else while(!(*keywords == '\0' || *keywords == ','))
      keywords = keywords+1;
  }
  return FALSE;
}


//
// Return 0 if the head of the string does not match any of our
// keywords. Returns the length of the keyword if it does match
//
int find_keyword(const char *keywords, const char *string) {
  int i, len = 0, num_keywords = 0;
  char **words = parse_keywords(keywords, &num_keywords);

  // try to find the longest keyword
  for(i = 0; i < num_keywords; i++) {
    int word_len = strlen(words[i]);
    if(!strncasecmp(words[i], string, word_len) && word_len > len)
      len = word_len;
    free(words[i]);
  }
  if(words) free(words);

  return len;
}


//
// return a list of all the unique keywords found in the keywords string
// keywords can be more than one word long (e.g. blue flame) and each
// keyword must be separated by a comma
//
char **parse_keywords(const char *keywords, int *num_keywords) {
  // we assume none to start off with
  *num_keywords = 0;

  // first, we check if the keywords have any non-spaces
  int i;
  bool nonspace_found = FALSE;
  for(i = 0; keywords[i] != '\0'; i++)
    if(!isspace(keywords[i]))
      nonspace_found = TRUE;

  // we didn't find any non-spaces. Return NULL
  if(!nonspace_found)
    return NULL;

  *num_keywords = count_letters(keywords, ',', strlen(keywords)) + 1;
  if(*num_keywords == 0)
    return NULL;

  char **keyword_names = malloc(sizeof(char *) * *num_keywords);

  int keyword_count = 0;
  while(keyword_count < *num_keywords - 1) {
    i = 0;
    // find the endpoint
    while(keywords[i] != ',')
      i++;

    char buf[i+1];
    strncpy(buf, keywords, i); buf[i] = '\0';
    trim(buf); // skip all whitespaces
    keyword_names[keyword_count] = strdup(buf);
    // skip everything we just copied
    keywords = &keywords[i+1];
    keyword_count++;
  }
  // get the last one, and trim it
  keyword_names[*num_keywords - 1] = strdup(keywords);
  trim(keyword_names[*num_keywords - 1]);

  return keyword_names;
}


//
// fill up the buffer with characters until
// a newline is reached, or we hit our critical
// length. Return how many characters were read
//
int fgetline(FILE *file, char *p, int maxlen)
{
  int count = 0;

  while(!feof(file) && count < maxlen - 1) {

    p[count] = fgetc(file);
    if(p[count] == '\n')
      break;

    count++;
  }

  p[count] = '\0';
  return count;
}


//
// returns a random number in the specified range (inclusive)
//
int rand_number(int min, int max) {
  if(min > max) {
    log_string("ERROR: rand_number passed a min (%d) higher than its max (%d).",
	       min, max);
    return 0;
  }

  return min + rand() % (max-min + 1);
}


//
// returns "st", "nd", "rd", or "th", based on the number passed in
//
const char *numth(int num) {
  if(num != 11 && num % 10 == 1)
    return "st";
  else if(num != 12 && num % 10 == 2)
    return "nd";
  else if(num != 13 && num % 10 == 3)
    return "rd";
  else
    return "th";
}


//
// Replace occurances of a with b. return how many occurances were replaced
//
int replace_string(char **string, const char *a, const char *b, bool all) {
  static char buf[MAX_BUFFER * 4];
  char *text = *string;
  int buf_i  = 0, i = 0, found = 0;
  int a_len  = strlen(a), b_len = strlen(b);
  *buf = '\0';

  for(i = 0; text[i] != '\0'; i++) {
    // we've found a match
    if(!strncmp(a, &text[i], a_len)) {
      strcpy(buf+buf_i, b);
      buf_i += b_len;
      i     += a_len - 1;

      found++;
      // print the rest, and exit
      if(!all) {
	strcpy(buf+buf_i, text+i+1);
	buf_i = strlen(buf);
	break;
      }
    }
    else {
      buf[buf_i] = text[i];
      buf_i++;
    }
  }

  buf[buf_i] = '\0';
  free(text);
  *string = strdup(buf);
  return found;
}


//
// Strips all occurances of a word from the string
//
// used to just replace occurances of 'word' with nothing,
// but this doesn't work so well since replace_string will
// replace it, even if the 'word' is embedded inside of another
// word. This is undesirable.
//
//  replace_string(&string, word, "", TRUE);
//
void strip_word(char *string, const char *word) {
  int word_len = strlen(word);
  int i = 0;

  for(i = 0; string[i] != '\0'; i++) {

    // skip all spaces and newline stuff
    while(isspace(string[i]) || string[i] == '\n' || string[i] == '\r')
      i++;

    // we've found a match
    if(!strncmp(string+i, word, word_len) &&
       (string[i+word_len] == '\0' || isspace(string[i+word_len]) ||
	string[i+word_len] == '\n' || string[i+word_len] == '\r')) {

      // count all of the spaces following us, so we can remove them too
      int space_count = 0;
      while(isspace(string[i+word_len+space_count]))
	space_count++;

      // shuffle everything down
      int j = i-1;
      do {
	j++;
	string[j] = string[j+word_len+space_count];
      } while(string[j+word_len+space_count] != '\0');
    }
  }
}

char *tag_keywords(const char *keywords, const char *string,
		   const char *start_tag, const char *end_tag) {
  char buf[strlen(string) * 2];
  int i = 0, j = 0;
  int start_len = strlen(start_tag);
  int end_len   = strlen(end_tag);

  while(*string) {
    int keyword_len;
    // check for a match
    if( (keyword_len = find_keyword(keywords, string)) != 0) {
      for(j = 0; j < start_len; j++)
	buf[i++] = start_tag[j];
      while(keyword_len > 0) {
	buf[i++] = *string;
	string++;
	keyword_len--;
      }
      for(j = 0; j < end_len; j++)
	buf[i++] = end_tag[j];
    }

    // skip ahead one word
    else {
      while(!isspace(*string) && *string != '\0') {
	buf[i++] = *string;
	string++;
      }
      // put in our space
      if(*string != '\0') {
	buf[i++] = *string;
	string++;
      }
    }
  }
  buf[i++] = '\0';//dialogue++;

  return strdup(buf);
}


bitvector_t parse_bits(const char *string) {
  bitvector_t bits = 0;

  while(*string) {
    SET_BIT(bits, (1 << (*string-'a')));
    string = string+1;
  }

  return bits;
}

const char *write_bits(bitvector_t bits) {
  static char buf[SMALL_BUFFER]; // really, we only need about 64...
  int buf_i = 0, i = 0;

  for(i = 0; i < BITS_PER_BITVECTOR; i++)
    if(IS_SET(bits, (1 << i)))
      buf[buf_i++] = 'a'+i;

  buf[buf_i] = '\0';
  return buf;
}


void print_bits(bitvector_t bits, const char **names, char *buf) {
  int i, count = 0;
  for(i = 0; i < BITS_PER_BITVECTOR; i++) {
    if(IS_SET(bits, (1 << i))) {
      count++;
      sprintf(buf, "%s%s%s",
	      (count == 1 ? "" : buf ),
	      (count == 1 ? "" : ", "),
	      names[i]);
    }
  }

  if(count == 0)
    sprintf(buf, "NOBITS");
}

bool  try_dialog(CHAR_DATA *ch, CHAR_DATA *listener, const char *mssg) {
  if(charIsNPC(listener)) {
    DIALOG_DATA *dialog = worldGetDialog(gameworld, charGetDialog(listener));
    RESPONSE_DATA *resp = (dialog ? dialogGetResponse(dialog, mssg):NULL);
    // we've got a response, say it
    if(resp) {
      char *response = tagResponses(dialog,
				    responseGetMessage(resp),
				    "{c", "{p");
      message(ch, listener, NULL, NULL, FALSE, TO_CHAR,
	      "{p$N replies, '%s'", response);
      free(response);
      return TRUE;
    }
  }
  return FALSE;
}

void try_dialog_all(CHAR_DATA *ch, LIST *listeners, const char *mssg) {
  LIST_ITERATOR *list_i = newListIterator(listeners);
  CHAR_DATA   *listener = NULL;

  ITERATE_LIST(listener, list_i) {
    if(ch == listener) continue;
    try_dialog(ch, listener, mssg);
  }
  deleteListIterator(list_i);
}


char *print_list(LIST *list, void *descriptor, void *multi_descriptor) {
  const char             *(* desc_func)(void *) = descriptor;
  const char            *(* multi_desc)(void *) = multi_descriptor;

  if(listSize(list) == 0)
    return strdup("");
  
  char buf[MAX_BUFFER] = "";
  char one_thing[SMALL_BUFFER];
  int i;
  int tot_things = 0;
  int size = listSize(list);
  int counts[size];
  void *things[size];
  void *thing;
  LIST_ITERATOR *thing_i = newListIterator(list);

  for(i = 0; i < size; i++) {
    counts[i] = 0;
    things[i] = NULL;
  }

  // first, collect groups of all the things in the list
  ITERATE_LIST(thing, thing_i) {
    // see if we have one with the current name already
    for(i = 0; i < size; i++) {
      // it's a thing with a new name
      if(things[i] == NULL) {
	things[i] = thing;
	counts[i]++;
	tot_things++;
	break;
      }
      // it has the same name as something else
      else if(!strcmp(desc_func(thing), desc_func(things[i]))) {
	counts[i]++;
	break;
      }
    }
  }
  deleteListIterator(thing_i);


  // now, print everything to the buffer
  for(i = 0; i < tot_things; i++) {
    if(counts[i] == 1)
      sprintf(one_thing, "%s", desc_func(things[i]));
    else if(multi_desc == NULL || !*multi_desc(things[i]))
      sprintf(one_thing, "(%d) %s", counts[i], desc_func(things[i]));
    else
      sprintf(one_thing, multi_desc(things[i]), counts[i]);

    // we're at the last one... we have to print an and
    if(i == tot_things - 1)
      sprintf(buf, "%s%s%s", buf,
	      (i < 1 ? "" : (i == 1 ? " and " : ", and ")),
	      one_thing);
    // regular comma-separated dealy
    else
      sprintf(buf, "%s%s%s", 
	      (i == 0 ? "" : buf), (i == 0 ? "" : ", "), one_thing);
  }

  return strdup(buf);
}


void show_list(CHAR_DATA *ch, LIST *list, 
	       void *descriptor, void *multi_descriptor) {
  const char             *(* desc_func)(void *) = descriptor;
  const char            *(* multi_desc)(void *) = multi_descriptor;

  int i;
  int size = listSize(list);
  int counts[size];
  void *things[size];
  void *thing;
  LIST_ITERATOR *thing_i = newListIterator(list);

  for(i = 0; i < size; i++) {
    counts[i] = 0;
    things[i] = NULL;
  }

  // find a list of all things with unique names
  ITERATE_LIST(thing, thing_i) {
    // see if we have one with the current name already
    for(i = 0; i < size; i++) {
      // it's a thing with a new name
      if(things[i] == NULL) {
	things[i] = thing;
	counts[i]++;
	break;
      }
      // it has the same name as something else
      else if(!strcmp(desc_func(thing), desc_func(things[i]))) {
	counts[i]++;
	break;
      }
    }
  }
  deleteListIterator(thing_i);


  // print out all of the things
  for(i = 0; i < size; i++) {
    // we've run into the end of the list
    if(things[i] == NULL)
      break;
    if(counts[i] == 1)
      send_to_char(ch, "%s\r\n", desc_func(things[i]));
    else if(multi_desc == NULL || !*multi_desc(things[i]))
      send_to_char(ch, "(%d) %s\r\n", counts[i], desc_func(things[i]));
    else {
      char fmt[SMALL_BUFFER];
      sprintf(fmt, "%s\r\n", multi_desc(things[i]));
      send_to_char(ch, fmt, counts[i]);
    }
  }
}


LIST *get_unused_items(CHAR_DATA *ch, LIST *list, bool invis_ok) {
  OBJ_DATA *obj;
  LIST *newlist = newList();
  LIST_ITERATOR *obj_i = newListIterator(list);

  ITERATE_LIST(obj, obj_i) {
    if(listSize(objGetUsers(obj)) != 0)
      continue;
    if(!(invis_ok || can_see_obj(ch, obj)))
      continue;

    listPut(newlist, obj);
  }
  deleteListIterator(obj_i);
  return newlist;
}

LIST *get_used_items(CHAR_DATA *ch, LIST *list, bool invis_ok) {
  OBJ_DATA *obj;
  LIST *newlist = newList();
  LIST_ITERATOR *obj_i = newListIterator(list);

  ITERATE_LIST(obj, obj_i) {
    if(listSize(objGetUsers(obj)) < 1)
      continue;
    if(!(invis_ok || can_see_obj(ch, obj)))
      continue;
    listPut(newlist, obj);
  }
  deleteListIterator(obj_i);
  return newlist;
}

bool has_obj(CHAR_DATA *ch, int vnum) {
  LIST *vis_inv = find_all_objs(ch, charGetInventory(ch), NULL, vnum, TRUE);
  int ret_val = FALSE;
  if(listSize(vis_inv) > 0)
    ret_val = TRUE;
  deleteList(vis_inv);
  return ret_val;
}

const char *custom_prompt(CHAR_DATA *ch) {
  static char prompt[MAX_BUFFER];
  *prompt = '\0';

#ifdef MODULE_COMBAT
  CHAR_DATA *tgt = get_target(ch);
  if(tgt != NULL) {
    sprintf(prompt, 
	    "{bMind %d, Left %d, Right %d, Feet %d, dam %d | %s :: dam %d > {n",
	    get_action_points(ch, FACULTY_MIND),
	    get_action_points(ch, FACULTY_LEFT_HAND),
	    get_action_points(ch, FACULTY_RIGHT_HAND),
	    get_action_points(ch, FACULTY_FEET),
	    get_damage(ch),
	    charGetName(tgt), get_damage(tgt));
  }
  else
#endif
    strcat(prompt, "\r\nprompt> ");

  return prompt;
}
