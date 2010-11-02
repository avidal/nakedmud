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
#include "dialog.h"
#include "event.h"
#include "action.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/script.h"



void  add_extract_obj_func (void (* func)(OBJ_DATA *)) {
  listQueue(extract_obj_funcs, func);
}

void  add_extract_mob_func (void (* func)(CHAR_DATA *)) {
  listQueue(extract_mob_funcs, func);
}

void extract_obj(OBJ_DATA *obj) {
  // go through all of our extraction functions
  LIST_ITERATOR *ex_i = newListIterator(extract_obj_funcs);
  void (* ex_func)(OBJ_DATA *) = NULL;
  ITERATE_LIST(ex_func, ex_i)
    ex_func(obj);
  deleteListIterator(ex_i);

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
  // go through all of our extraction functions
  LIST_ITERATOR *ex_i = newListIterator(extract_mob_funcs);
  void (* ex_func)(CHAR_DATA *) = NULL;
  ITERATE_LIST(ex_func, ex_i)
    ex_func(ch);
  deleteListIterator(ex_i);

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

  case COMM_LOCAL: {  /* everyone in the same room */
    char other_buf[MAX_BUFFER];
    sprintf(other_buf, "{y$n says, '%s'{n", txt);
    send_to_char(dMob, "{yYou say, '%s'{n\r\n", txt);
    message(dMob, NULL, NULL, NULL, FALSE, TO_ROOM | TO_NOTCHAR, other_buf);
    try_dialog_all(dMob, roomGetCharacters(charGetRoom(dMob)), txt);
    try_speech_script(dMob, NULL, txt);
    break;
  }

  case COMM_GLOBAL: { /* everyone in the world */
    char other_buf[MAX_BUFFER];
    sprintf(other_buf, "{c$n chats, '%s'{n", txt);
    send_to_char(dMob, "{cYou chat, '%s'{n\r\n", txt);
    message(dMob, NULL, NULL, NULL, FALSE, TO_WORLD | TO_NOTCHAR, other_buf);
    break;
  }

  case COMM_LOG:
    send_to_level(LEVEL_ADMIN, "[LOG: %s]\r\n", txt);
    break;
  }
}


/*
 * load the world and its inhabitants, as well as other misc game data
 */
void load_muddata() {  
  gameworld = worldLoad(WORLD_PATH);
  if(gameworld == NULL) {
    log_string("ERROR: Could not boot game world.");
    abort();
  }

  greeting = read_file("../lib/txt/greeting");
  motd     = read_file("../lib/txt/motd");
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

bool  can_see_char          ( CHAR_DATA *ch, CHAR_DATA *target) {
  if(ch == target)
    return TRUE;
  if(poscmp(charGetPos(ch), POS_SLEEPING) <= 0)
    return FALSE;
  if(charGetImmInvis(target) > charGetLevel(ch))
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

const char *see_char_as (CHAR_DATA *ch, CHAR_DATA *target) {
  if(can_see_char(ch, target))
    return charGetName(target);
  return SOMEONE;
}

const char *see_obj_as  (CHAR_DATA *ch, OBJ_DATA  *target) {
  if(can_see_obj(ch, target))
    return objGetName(target);
  return SOMETHING;
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
    if(must_see && !can_see_char(looker, ch))
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
    if(must_see && !can_see_char(looker, ch))
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
    if(must_see && !can_see_char(looker, ch))
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


//
// just a generic function for hashing a string. This could be 
// sped up tremendously if it's performance becoming a problem.
int string_hash(const char *key) {
  const int BASE = 2;
  int base = 1;
  int hvalue = 0;
  for (; *key; key++) {
    base *= BASE;
    hvalue += tolower(*key) * base;
  }
  return (hvalue < 0 ? hvalue * -1 : hvalue);
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
// counts how many times word occurs in string. Assumes strlen(word) >= 1
//
int count_occurences(const char *string, const char *word) {
  int count = 0, i = 0, word_len = strlen(word);
  for(; string[i] != '\0'; i++) {
    if(!strncmp(string, word, word_len)) {
      count++;
      i += word_len - 1;
    }
  }
  return count;
}

//
// return a pointer to the start of the line num (lines are ended with \n's).
// return NULL if the line does not exist
//
char *line_start(char *string, int line) {
  // skip forward to the appropriate line
  int i, count = 1;

  // are we looking for the start?
  if(line == 1) return string;

  for(i = 0; string[i] != '\0'; i++) {
    if(string[i] == '\n')
      count++;
    if(count == line)
      return string+i+1;
  }

  return NULL;
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
    // figure out the length of the current keyword
    int keyword_len = next_letter_in(keywords, ',');

    // see if we compare to the current keyword
    if(!abbrev_ok && !strncasecmp(keywords, word, keyword_len) &&
       keyword_len == word_len)
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
  for(i = 0; keywords[i] != '\0'; i++) {
    if(!isspace(keywords[i])) {
      nonspace_found = TRUE;
      break;
    }
  }

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
// If the keyword does not already exist, add it to the keyword list
// keywords may be freed and re-built in the process, to make room
//
void add_keyword(char **keywords_ptr, const char *word) {
  // if it's already a keyword, do nothing
  if(!is_keyword(*keywords_ptr, word, FALSE)) {
    char buf[MAX_BUFFER];
    // copy everything over
    strcpy(buf, *keywords_ptr);
    // print the new word
    strcat(buf, ", ");
    strcat(buf, word);
    // free the old string
    free(*keywords_ptr);
    // copy the new one
    *keywords_ptr = strdup(buf);
  }
}


//
// go through the keywords and if word is found, remove it
//
void remove_keyword(char *keywords, const char *word) {
  int i, key_i = 0, num_keywords = 0;
  char **words = parse_keywords(keywords, &num_keywords);

  // clear the current list... we will rebuild it
  *keywords = '\0';

  // go through and add them all back into keywords. If we
  // ever encounter word, then leave it out
  for(i = 0; i < num_keywords; i++) {
    if(strcasecmp(words[i], word) != 0) {
      key_i += sprintf(keywords+key_i, "%s", words[i]);
      if(i < num_keywords-1 && strcasecmp(words[i+1], word) != 0)
	key_i += sprintf(keywords+key_i, ", ");
    }
    free(words[i]);
  }
  free(words);
}


//
// print the string on the buffer, centered for a line of length linelen.
// if border is TRUE, put a border to either side of the string.
//
void center_string(char *buf, const char *string, int linelen, int buflen, 
		   bool border) {
  static char fmt[32];
  int str_len = strlen(string);
  int spaces  = (linelen - str_len)/2;

  if(border) {
    int i, buf_i = 0;
    sprintf(fmt, "{g%%-%ds[{c", spaces-2);
    buf_i  = snprintf(buf, buflen, fmt, " ");
    // replace all of the spaces with -
    for(i = 0; buf[i] != '\0'; i++) if(buf[i] == ' ') buf[i] = '-';

    buf_i += snprintf(buf+buf_i, buflen-buf_i, " %s ", string);
    sprintf(fmt, "{g]%%-%ds\r\n", 
	    spaces-2 + (((linelen-str_len) % 2) == 1 ? 1 : 0));

    i = buf_i;
    buf_i += snprintf(buf+buf_i, buflen-buf_i, fmt, " ");
    // replace all of the spaces with -
    for(; buf[i] != '\0'; i++) if(buf[i] == ' ') buf[i] = '-';
  }
  else {
    int buf_i = 0;
    sprintf(fmt, "%%-%ds", spaces);
    buf_i  = snprintf(buf, buflen, fmt, " ");
    buf_i += snprintf(buf+buf_i, buflen-buf_i, "%s\r\n", string);
  }
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

int rand_number(int min, int max) {
  if(min > max) {
    log_string("ERROR: rand_number passed a min (%d) higher than its max (%d).",
	       min, max);
    return 0;
  }

  return min + rand() % (max-min + 1);
}

double rand_percent(void) {
  double rnd = rand_number(0, RAND_MAX);
  return rnd / (double)RAND_MAX;
}

double gaussian(void) {
  return sqrt(-2.0 * log(rand_percent())) * cos(2.0 * PI * rand_percent());
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
      send_to_char(ch, "{p%s replies, '%s'\r\n", charGetName(listener), response);
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


void show_list(CHAR_DATA *ch, LIST *list, void *descriptor, 
	       void *multi_descriptor, void *vnum_getter) {
  const char             *(* desc_func)(void *) = descriptor;
  const char            *(* multi_desc)(void *) = multi_descriptor;
  int                    (*  vnum_func)(void *) = vnum_getter;

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
    else {
      char vnum_buf[20];
      if(charGetLevel(ch) < LEVEL_BUILDER || vnum_func == NULL)
	*vnum_buf = '\0';
      else
	sprintf(vnum_buf, "[%d] ", vnum_func(things[i]));

      if(counts[i] == 1)
	send_to_char(ch, "%s%s\r\n", vnum_buf, desc_func(things[i]));
      else if(multi_desc == NULL || !*multi_desc(things[i]))
	send_to_char(ch, "%s(%d) %s\r\n", vnum_buf, 
		     counts[i], desc_func(things[i]));
      else {
	char fmt[SMALL_BUFFER];
	sprintf(fmt, "%s%s\r\n", vnum_buf, multi_desc(things[i]));
	send_to_char(ch, fmt, counts[i]);
      }
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

void *identity_func(void *data) {
  return data;
}

void show_prompt(SOCKET_DATA *socket) {
  text_to_buffer(socket, custom_prompt(socketGetChar(socket)));
}

const char *custom_prompt(CHAR_DATA *ch) {
  static char prompt[MAX_BUFFER];
  *prompt = '\0';
  strcat(prompt, "\r\nprompt> ");    
  return prompt;
}

