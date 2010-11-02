#ifndef MUD_H
#define MUD_H
//*****************************************************************************
//
// mud.h
//
// this is the main header file for the MUD. All MUD-related source files should
// include this header.
//
//*****************************************************************************
#include <zlib.h>
#include <pthread.h>
#include <arpa/telnet.h>
#include "wrapsock.h"



//*****************************************************************************
//
// if you've installed a new module, you  need to put a define in here to let
// the rest of the MUD know that you've installed the module.
//
//*****************************************************************************
#define MODULE_OLC
#define MODULE_TIME
#define MODULE_SCRIPTS
#define MODULE_ALIAS
#define MODULE_CHAR_VARS
#define MODULE_SOCIALS


//
// Two modules that I am working on... together, they work similar to KaVir's
// Gladiator Pits. Expect to see them for download in the near future.
//   - Geoff Hollis, Dec26/04
//
//#define MODULE_FACULTY
//#define MODULE_COMBAT
//



//*****************************************************************************
//
// To avoid having to write some bulky structure names, we've typedefed a
// bunch of shortforms for commonly used datatypes. If you make a new datatype
// that is used lots, put a typedef for it in here.
//
//*****************************************************************************
typedef struct socket_data                SOCKET_DATA;
typedef struct char_data                  CHAR_DATA;  
typedef struct help_data                  HELP_DATA;
typedef struct lookup_data                LOOKUP_DATA;
typedef struct list                       LIST;
typedef struct list_iterator              LIST_ITERATOR;
typedef struct hashtable                  HASHTABLE;
typedef struct hashtable_iterator         HASH_ITERATOR;
typedef struct hashmap                    HASHMAP;
typedef struct map_iterator               MAP_ITERATOR;
typedef struct datatable                  DATATABLE;
typedef struct storage_set                STORAGE_SET;
typedef struct storage_set_list           STORAGE_SET_LIST;

typedef struct property_table             PROPERTY_TABLE;
typedef struct property_table_iterator    PROPERTY_TABLE_ITERATOR;

typedef struct buffer_type                BUFFER;

typedef struct script_set_data            SCRIPT_SET;
typedef struct edesc_data                 EDESC_DATA;
typedef struct edesc_set_data             EDESC_SET;
typedef struct response_data              RESPONSE_DATA;
typedef struct dialog_data                DIALOG_DATA;

typedef struct script_data                SCRIPT_DATA;
typedef struct world_data                 WORLD_DATA;
typedef struct zone_data                  ZONE_DATA;
typedef struct room_data                  ROOM_DATA;
typedef struct exit_data                  EXIT_DATA;
typedef struct object_data                OBJ_DATA;
typedef struct shop_data                  SHOP_DATA;
typedef struct body_data                  BODY_DATA;
typedef struct reset_data                 RESET_DATA;
typedef struct olc_data                   OLC_DATA;

typedef int                               shop_vnum;
typedef int                               room_vnum;
typedef int                               zone_vnum;
typedef int                               mob_vnum;
typedef int                               obj_vnum;
typedef int                               script_vnum;
typedef int                               dialog_vnum;
typedef long                              bitvector_t;

/* define simple types */
#ifndef __cplusplus
typedef  unsigned char     bool;
#endif
typedef  short int         sh_int;
typedef  unsigned char     byte;


// these appear in so many places, we might as well just add 'em to our header
// here so we don't have to include them everywhere else. Only catch is we
// have to include them after all of our typedefs so the header files can use
// the typedefs.
#include "property_table.h"
#include "list.h"
#include "hashmap.h"
#include "hashtable.h"




/************************
 * Standard definitions *
 ************************/

/* define TRUE and FALSE */
#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE    1
#endif

#define eTHIN   0
#define eBOLD   1

/* A few globals */
#define PULSES_PER_SECOND    10                   /* must divide 1000 : 4, 5 or 8 works */
#define SECOND              * PULSES_PER_SECOND   /* used for figuring out how many pulses in a second*/
#define SECONDS             SECOND                /* same as above */
#define MINUTE              * 60 SECONDS          /* one minute */
#define MINUTES             MINUTE
#define MAX_INPUT_LEN       512                   /* max length of a string someone can input */
#define SMALL_BUFFER       1024
#define MAX_BUFFER         8192                   /* seems like a decent amount         */
#define MAX_HELP_ENTRY     (MAX_BUFFER)
#define MAX_SCRIPT         16384                  /* max length of a script */
#define MAX_OUTPUT         8192                   /* well shoot me if it isn't enough   */
#define FILE_TERMINATOR    "EOF"                  /* end of file marker                 */
#define COPYOVER_FILE      "../txt/copyover.dat"  /* tempfile to store copyover data    */
#define EXE_FILE           "../src/NakedMud"      /* the name of the mud binary         */
#define DEFAULT_PORT       4000                   /* the default port we run on */

// if it is safe to run the game loop in a separate
// thread (this makes it so we can run scripts in a separate
// thread as well) then keep this on. Otherwise, comment out
// this line
//#define MUD_THREADABLE

/* Connection States */
#define STATE_CLOSED           0  /* should always be the last state
                                     The above comment is incorrect. GH */
#define STATE_PLAYING          1
#define STATE_OLC              2
#define STATE_TEXT_EDITOR      3 
#define STATE_NEW_NAME         4
#define STATE_NEW_PASSWORD     5
#define STATE_VERIFY_PASSWORD  6
#define STATE_ASK_PASSWORD     7
#define STATE_ASK_SEX          8
#define STATE_ASK_RACE         9


/* Thread States */
#define TSTATE_LOOKUP          0  /* Socket is in host_lookup        */
#define TSTATE_DONE            1  /* The lookup is done.             */
#define TSTATE_WAIT            2  /* Closed while in thread.         */
#define TSTATE_CLOSED          3  /* Closed, ready to be recycled.   */

/* player levels */
#define LEVEL_PLAYER           1  // All of the normal players
#define LEVEL_BUILDER          2  // Players who have building rights
#define LEVEL_SCRIPTER         3  // Players who have building+scripting rights
#define LEVEL_ADMIN            4  // The people who oversee the MUD
#define MAX_LEVEL      LEVEL_ADMIN

/* Communication Ranges */
#define COMM_LOCAL             0  /* same room only                  */
#define COMM_GLOBAL            1  /* all over the game               */
#define COMM_LOG              10  /* admins only                     */


/* cmd_tog_prf */
#define SUBCMD_BUILDWALK       0


#define NOWHERE        (-1)
#define NOTHING        (-1)
#define NOBODY         (-1)

#define SOMWHERE        "somewhere"
#define SOMETHING       "something"
#define SOMEONE         "someone"
#define NOTHING_SPECIAL "you see nothing special."

#define START_ROOM      100

#define WORLD_PATH     "../lib/world"

/******************************
 * End of standard definitons *
 ******************************/



/******************************
 * New structures             *
 ******************************/

/* the actual structures */
struct help_data
{
  HELP_DATA     * next;
  time_t          load_time;
  char          * keyword;
  char          * text;
};

struct lookup_data
{
  SOCKET_DATA       * dsock;   /* the socket we wish to do a hostlookup on */
  char           * buf;     /* the buffer it should be stored in        */
};


#define CMD_PTR(name)      void (* name)(CHAR_DATA *ch, const char *cmd, \
					 int subcmd, char *arg)
#define COMMAND(name)      void name(CHAR_DATA *ch, const char *cmd, \
				     int subcmd, char *arg)
void init_commands();
void show_commands(CHAR_DATA *ch, int min_lev, int max_lev);
void remove_cmd   (const char *cmd);
void add_cmd      (const char *cmd, const char *sort_by, void *func, 
	           int subcmd, int min_pos, int max_pos,
	           int min_level, bool mob_ok, bool interrupts);
void remove_cmd   (const char *cmd);


struct buffer_type
{
  char   * data;        /* The data                      */
  int      len;         /* The current len of the buffer */
  int      size;        /* The allocated size of data    */
};

/******************************
 * End of new structures      *
 ******************************/



/***************************
 * Global Variables        *
 ***************************/

extern  LIST           *object_list;
extern  PROPERTY_TABLE *obj_table;      /* same contents as object_list, but
					   arranged by uid (unique ID)        */
extern  LIST           *socket_list;
extern  LIST           *socket_free;
extern  LIST           *mobile_list;
extern  PROPERTY_TABLE *mob_table;      /* same contents as mobile_list, but
					   arranged by uid (unique ID)        */
extern  HELP_DATA   *   help_list;      /* the linked list of help files      */
extern  const struct    typCmd tabCmd[];/* the command table                  */
extern  bool            shut_down;      /* used for shutdown                  */
extern  int             mudport;        /* What port are we running on?       */
extern  char        *   greeting;       /* the welcome greeting               */
extern  char        *   motd;           /* the MOTD help file                 */
extern  int             control;        /* boot control socket thingy         */
extern  time_t          current_time;   /* let's cut down on calls to time()  */

extern WORLD_DATA    *   gameworld;     // the world of the game

/*************************** 
 * End of Global Variables *
 ***************************/



/***********************
 *    MCCP support     *
 ***********************/

extern const unsigned char compress_will[];
extern const unsigned char compress_will2[];

#define TELOPT_COMPRESS       85
#define TELOPT_COMPRESS2      86
#define COMPRESS_BUF_SIZE   8192

/***********************
 * End of MCCP support *
 ***********************/



/***********************************
 * Prototype function declerations *
 ***********************************/

#define  buffer_new(size)             __buffer_new     ( size)
#define  buffer_strcat(buffer,text)   __buffer_strcat  ( buffer, text )

char  *crypt                  ( const char *key, const char *salt );


/* interpret.c */
void  handle_cmd_input        ( SOCKET_DATA *dsock, char *arg );

//
// Some command scripts may want to re-force a character to
// perform the command. In that case, scripts_ok can be
// set to FALSE so that the command script doesn't re-run
//
void  do_cmd                  ( CHAR_DATA *ch, char *arg, 
				bool scripts_ok, bool aliases_ok);

/* io.c */
void    log_string            ( const char *txt, ... ) __attribute__ ((format (printf, 1, 2)));
void    bug                   ( const char *txt, ... ) __attribute__ ((format (printf, 1, 2)));
time_t  last_modified         ( char *helpfile );
char   *read_file             ( const char *file );
char   *read_help_entry       ( const char *helpfile);
char   *fread_line            ( FILE *fp );                 /* pointer        */
char   *fread_string          ( FILE *fp );                 /* allocated data */
char   *fread_word            ( FILE *fp );                 /* pointer        */
int     fread_number          ( FILE *fp );                 /* just an integer*/
long    fread_long            ( FILE *fp );                 /* a long integer */

/* strings.c */
char   *one_arg               ( char *fStr, char *bStr );
void    arg_num               ( const char *from, char *to, int num); 
bool    compares              ( const char *aStr, const char *bStr );
bool    is_prefix             ( const char *aStr, const char *bStr );
char   *capitalize            ( char *txt );
char   *strfind               (char *txt, char *sub);
BUFFER *__buffer_new          ( int size );
void    __buffer_strcat       ( BUFFER *buffer, const char *text );
void    buffer_free           ( BUFFER *buffer );
void    buffer_clear          ( BUFFER *buffer );
int     bprintf               ( BUFFER *buffer, char *fmt, ... ) __attribute__ ((format (printf, 2, 3)));
const char *buffer_string     ( BUFFER *buffer );
void    buffer_format         ( BUFFER *buffer, bool indent );

/* mccp.c */
bool  compressStart     ( SOCKET_DATA *dsock, unsigned char teleopt );
bool  compressEnd       ( SOCKET_DATA *dsock, unsigned char teleopt, bool forced );

/* mud.c */
void init_mud_settings();
void save_mud_settings();
int next_char_uid();

/* socket.c */
#define NUM_LINES_PER_PAGE  22
void  page_string           ( SOCKET_DATA *dsock, const char *string);
void  page_continue         ( SOCKET_DATA *dsock);
void  page_back             ( SOCKET_DATA *dsock);

/* help.c */
bool check_help(CHAR_DATA *dMob, char *helpfile);

/*******************************
 * End of prototype declartion *
 *******************************/

#endif  /* MUD_H */
