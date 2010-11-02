#ifndef __UTILS_H
#define __UTILS_H
//*****************************************************************************
//
// utils.h
//
// macros and definitions of miscellaneous utilities
//
//*****************************************************************************



//*****************************************************************************
// Utilities for numbers
//*****************************************************************************
#define MAX_INT               214743647
#define PI                    3.14159265
#define e                     2.71828182

#define UMIN(a, b)	      ((a) < (b) ? (a) : (b))
#define MIN(a, b)             ((a) < (b) ? (a) : (b))
#define MAX(a, b)             ((a) < (b) ? (b) : (a))


//
// return a random number pulled from a uniform distribution 
// between the two bounds
int rand_number(int min, int max);

//
// Returns a random number between 0 and 1. Numbers are evenly distributed
double rand_percent(void);

//
// Return a random number pulled from N(0, 1)
double rand_gaussian(void);

//
// Returns a sigmoid transformation of the specified number
double sigmoid(double val);

//
// return the rd, th, nd, for a number
const char *numth(int num);



//*****************************************************************************
// Utilities for characters
//*****************************************************************************
#define IS_ADMIN(ch)          ((charGetLevel(ch)) > LEVEL_PLAYER ? TRUE : FALSE)

#define HIMHER(ch)            (charGetSex(ch) == SEX_MALE ? "him" : \
			       (charGetSex(ch) == SEX_FEMALE ? "her" : "it"))

#define HISHERS(ch)           (charGetSex(ch) == SEX_MALE ? "his" : \
			       (charGetSex(ch) == SEX_FEMALE ? "her" : "its"))

#define HESHE(ch)             (charGetSex(ch) == SEX_MALE ? "he" : \
			       (charGetSex(ch) == SEX_FEMALE ? "she" : "it"))

bool  can_see_char      ( CHAR_DATA *ch, CHAR_DATA *target);
bool  can_see_obj         ( CHAR_DATA *ch, OBJ_DATA  *target);
bool  can_see_exit        ( CHAR_DATA *ch, EXIT_DATA *exit);
bool  try_enter_game      ( CHAR_DATA *ch);
int   can_see_hidden      ( CHAR_DATA *ch);
int   can_see_invis       ( CHAR_DATA *ch);

//
// returns the target's name if the ch can see the target,
// and returns SOMEONE/SOMETHING otherwise.
const char *see_char_as (CHAR_DATA *ch, CHAR_DATA *target);
const char *see_obj_as  (CHAR_DATA *ch, OBJ_DATA  *target);

bool  try_dialog          (CHAR_DATA *ch, CHAR_DATA *listener,const char *mssg);
void  try_dialog_all      (CHAR_DATA *ch, LIST *listeners, const char *mssg);

void     show_prompt(SOCKET_DATA *socket);
const char *custom_prompt (CHAR_DATA *ch);



//*****************************************************************************
// Utilities for bit manipulations and bitvectors
//*****************************************************************************
#define BITS_PER_BITVECTOR                   32

#define IS_SET(flag,bit)        ((flag) & (bit))
#define SET_BIT(var,bit)        ((var) |= (bit))
#define REMOVE_BIT(var,bit)    ((var) &= ~(bit))
#define TOGGLE_BIT(var,bit)     ((var) ^= (bit))

bitvector_t parse_bits(const char *string);
const char *write_bits(bitvector_t bits);
void        print_bits(bitvector_t bits, const char **names, char *buf);



//*****************************************************************************
// String utilities.
//*****************************************************************************
#define CLEAR_SCREEN       "\033[H\033[J"
#define AN(string)         (strchr("AEIOU", toupper(*string)) ? "an" : "a")
#define strdupsafe(string) strdup(string ? string : "")

char **parse_keywords     (const char *keywords, int *num_keywords);
bool is_keyword           (const char *keywords, const char *word, 
			   bool abbrev_ok);
int  find_keyword         (const char *keywords, const char *string);
void add_keyword          (char **keywords_ptr, const char *word);
void remove_keyword       (char *keywords, const char *word);
void trim                 (char *string);
void strip_word           (char *string, const char *word);
int  replace_string       (char **string, const char *a, const char *b, 
			   bool all);
char *tag_keywords        (const char *keywords, const char *string,
			   const char *start_tag, const char *end_tag);
void format_string        (char **ptr_string, int max_width, 
		          unsigned int maxlen, bool indent);
int  count_letters        (const char *string, const char ch, const int strlen);
int  count_occurences     (const char *string, const char *word);
char *line_start          (char *string, int line);
int  fgetline             (FILE *file, char *p, int maxlen);
void center_string        (char *buf, const char *string, int linelen, 
			   int buflen, bool border);
int next_space_in         (char *string);
int next_letter_in        (const char *string, char marker);
int string_hash           (const char *key);



//*****************************************************************************
// utilities for game functioning
//*****************************************************************************
void  extract_obj          ( OBJ_DATA *obj);
void  extract_mobile       ( CHAR_DATA *ch );

// Make sure a certain procedure is called when we are extracting an npc/char
void  add_extract_obj_func ( void (* func)(OBJ_DATA *));
void  add_extract_mob_func ( void (* func)(CHAR_DATA *));

char *get_time             ( void );
void  communicate          ( CHAR_DATA *dMob, char *txt, int range );
void  load_muddata         ( void );
CHAR_DATA  *check_reconnect( const char *player );



//*****************************************************************************
// misc utils
//*****************************************************************************

//
// returns TRUE if ch1 belongs to more user groups than ch2. By "more", we mean
// that ch2's user groups must be a subset of ch1's
bool charHasMoreUserGroups(CHAR_DATA *ch1, CHAR_DATA *ch2);


//
// a function that returns the argument passed into it
void *identity_func(void *data);

//
// checks to see if a file or directory exists
bool file_exists(const char *fname);
bool dir_exists (const char *dname);

// iterate across all the elements in a list
#define ITERATE_LIST(val, it) \
  for(val = listIteratorCurrent(it); val != NULL; val = listIteratorNext(it))

// iterate across all the elements in a hashtable
#define ITERATE_HASH(key, val, it) \
  for(key = hashIteratorCurrentKey(it), val = hashIteratorCurrentVal(it); \
      key != NULL; \
      hashIteratorNext(it), \
      key = hashIteratorCurrentKey(it), val = hashIteratorCurrentVal(it))

// iterate across all of the elements in a map
#define ITERATE_MAP(key, val, it) \
  for(key = mapIteratorCurrentKey(it), val = mapIteratorCurrentVal(it); \
      key != NULL; \
      mapIteratorNext(it), \
      key = mapIteratorCurrentKey(it), val = mapIteratorCurrentVal(it))

// iterate across all the elements in a set
#define ITERATE_SET(elem, it) \
  for(elem = setIteratorCurrent(it); \
      elem != NULL; \
      setIteratorNext(it), elem = setIteratorCurrent(it))




//
// count how many objects are in the list, that meet our critereon.
// If name is not NULL, we search by name. Otherwise, we search by vnum.
// must_see TRUE, a looker must be supplied. Same thing goes for count_letters
int   count_objs   (CHAR_DATA *looker, LIST *list, const char *name, int vnum,
		    bool must_see);
int   count_chars  (CHAR_DATA *looker, LIST *list, const char *name, int vnum,
		    bool must_see);
CHAR_DATA *find_char(CHAR_DATA *looker, LIST *list, int num, const char *name,
		     int vnum, bool must_see);
OBJ_DATA *find_obj (CHAR_DATA *looker, LIST *list, int num, const char *name,
		    int vnum, bool must_see);
LIST *find_all_chars(CHAR_DATA *looker, LIST *list, const char *name, int vnum,
		     bool must_see);
LIST *find_all_objs(CHAR_DATA *looker, LIST *list, const char *name, int vnum, 
		    bool must_see);

// in the various find() routines, it may sometimes arise that someone
// wants to find multiple things (e.g. all.cookies, all.women). This is
// the numeric marker to represent all.
#define COUNT_ALL       (-1)

//
// split a string into it's target and count number.
//   e.g. all.cookie splits into cookie and all
//        2.woman splits into woman and 2
void get_count(const char *buf, char *target, int *count);

//
// reverse of get_count
void print_count(char *buf, const char *target, int count);


//
// return a comma-separated list of the list elements, as represented
// by the descriptor function. If there is more than one element in the list,
// the last element is separated by an "and" as well. If there are just two
// elements, then the last comma isn't used. If one element has the same
// descriptor as another element, they are printed as a group - either by
// the description given from multi_descriptor, or with a number before the
// regular descriptor representing how many of the things are present.
//
//  e.g. a man, a woman, a dog, and a child.
//       a man and a woman
//       (5) a man
//       a group of 5 men
char *print_list(LIST *list, void *descriptor, void *multi_descriptor);


//
// descriptor is a pointer to the function that gets a copy of the
// thing's description.
//
// multi-descriptor is a pointer to the function that gets a copy of the
// thing's description if there is more than 1 copy of it in the list.
//
// vnum_getter is a pointer to a function that returns the thing's vnum for
// displaying to players of builder level and above. vnum_getter can be NULL 
// to display no vnums.
void show_list(CHAR_DATA *ch, LIST *list, void *descriptor, 
	       void *multi_descriptor, void *vnum_getter);


//
// Return a list of all items in the current list that do not have
// people sitting on or at them. The returned list must be deleted
// after use.
LIST *get_unused_items(CHAR_DATA *ch, LIST *list, bool invis_ok);


//
// The opposite of get_unused_items
LIST *get_used_items(CHAR_DATA *ch, LIST *list, bool invis_ok);


//
// returns true if the character has an object with the
// specified vnum in his inventory. Returns false otherwise.
// useful fo checking keys
bool has_obj(CHAR_DATA *ch, int vnum);

#endif // __UTILS_H
