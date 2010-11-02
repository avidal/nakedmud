//*****************************************************************************
//
// exit.c
//
// exits are structures that keep information about the links between rooms.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "storage.h"
#include "exit.h"

#define EX_CLOSED            (1 << 0)
#define EX_LOCKED            (1 << 1)
// lockable is handled if the exit has a key


struct exit_data {
  char *name;              // what is the name of our door for descriptions?
  char *keywords;          // what keywords can the door be referenced by?
  BUFFER *desc;            // what does a person see when they look at us?

  char *spec_enter;        // the message when we enter from this exit
  char *spec_leave;        // the message when we leave through this exit

  bitvector_t status;      // closable, closed, locked, etc...

  int closable;            // is the exit closable?
  int key;            // what is the vnum of the key?
  int hide_lev;            // how hidden is this exit?
  int pick_lev;            // how hard is it to pick this exit?

  int to;            // where do we exit to?
};



EXIT_DATA *newExit() {
  EXIT_DATA *exit = malloc(sizeof(EXIT_DATA));
  exit->name        = strdup("");
  exit->keywords    = strdup("");
  exit->spec_enter  = strdup("");
  exit->spec_leave  = strdup("");
  exit->desc        = newBuffer(1);
  exit->key         = NOTHING;
  exit->to          = NOWHERE;
  exit->hide_lev    = 0;
  exit->pick_lev    = 0;
  exit->status      = 0;
  exit->closable    = FALSE;
  return exit;
};


void deleteExit(EXIT_DATA *exit) {
  if(exit->name)        free(exit->name);
  if(exit->spec_enter)  free(exit->spec_enter);
  if(exit->spec_leave)  free(exit->spec_leave);
  if(exit->keywords)    free(exit->keywords);
  if(exit->desc)        deleteBuffer(exit->desc);

  free(exit);
};


void exitCopyTo(const EXIT_DATA *from, EXIT_DATA *to) {
  exitSetName     (to, exitGetName(from));
  exitSetKeywords (to, exitGetKeywords(from));
  exitSetDesc     (to, exitGetDesc(from));
  exitSetTo       (to, exitGetTo(from));
  exitSetPickLev  (to, exitGetPickLev(from));
  exitSetHidden   (to, exitGetHidden(from));
  exitSetKey      (to, exitGetKey(from));
  exitSetLocked   (to, exitIsLocked(from));
  exitSetClosed   (to, exitIsClosed(from));
  exitSetClosable (to, exitIsClosable(from));
  exitSetSpecEnter(to, exitGetSpecEnter(from));
  exitSetSpecLeave(to, exitGetSpecLeave(from));
}


EXIT_DATA *exitCopy(const EXIT_DATA *exit) {
  EXIT_DATA *E = newExit();
  exitCopyTo(exit, E);
  return E;
}

EXIT_DATA *exitRead(STORAGE_SET *set) {
  EXIT_DATA *exit = newExit();
  exitSetName(exit,      read_string(set, "name"));
  exitSetKeywords(exit,  read_string(set, "keywords"));
  exitSetDesc(exit,      read_string(set, "desc"));
  exitSetSpecEnter(exit, read_string(set, "enter"));
  exitSetSpecLeave(exit, read_string(set, "leave"));
  exitSetTo(exit,        read_int   (set, "to"));
  exitSetKey(exit,       read_int   (set, "key"));
  exitSetHidden(exit,    read_int   (set, "hide_level"));
  exitSetPickLev(exit,   read_int   (set, "pick_level"));
  exitSetClosable(exit,  read_int   (set, "closable"));
  return exit;
}

STORAGE_SET *exitStore(EXIT_DATA *exit) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "name",       exit->name);
  store_string(set, "keywords",   exit->keywords);
  store_string(set, "desc",       bufferString(exit->desc));
  store_string(set, "enter",      exit->spec_enter);
  store_string(set, "leave",      exit->spec_leave);
  store_int   (set, "to",         exit->to);
  store_int   (set, "key",        exit->key);
  store_int   (set, "hide_level", exit->hide_lev);
  store_int   (set, "pick_level", exit->pick_lev);
  store_int   (set, "closable",   exit->closable);
  return set;
}




//*****************************************************************************
//
// is, get and set functions
//
//*****************************************************************************

bool        exitIsClosable(const EXIT_DATA *exit) {
  return exit->closable;
};

bool        exitIsClosed(const EXIT_DATA *exit) {
  return IS_SET(exit->status, EX_CLOSED);
};

bool        exitIsLocked(const EXIT_DATA *exit) {
  return IS_SET(exit->status, EX_LOCKED);
};

bool        exitIsName(const EXIT_DATA *exit, const char *name) {
  return is_keyword(exit->keywords, name, TRUE);
}

int         exitGetHidden(const EXIT_DATA *exit) {
  return exit->hide_lev;
};

int         exitGetPickLev(const EXIT_DATA *exit) {
  return exit->pick_lev;
};

int    exitGetKey(const EXIT_DATA *exit) {
  return exit->key;
};

int   exitGetTo(const EXIT_DATA *exit) {
  return exit->to;
};

const char *exitGetName(const EXIT_DATA *exit) {
  return exit->name;
};

const char *exitGetKeywords(const EXIT_DATA *exit) {
  return exit->keywords;
};

const char *exitGetDesc(const EXIT_DATA *exit) {
  return bufferString(exit->desc);
};

const char *exitGetSpecEnter(const EXIT_DATA *exit) {
  return exit->spec_enter;
};

const char *exitGetSpecLeave(const EXIT_DATA *exit) {
  return exit->spec_leave;
};

BUFFER *exitGetDescBuffer(const EXIT_DATA *exit) {
  return exit->desc;
}

void        exitSetClosable(EXIT_DATA *exit, bool closable) {
  exit->closable = (closable != 0);
};

void        exitSetClosed(EXIT_DATA *exit, bool closed) {
  if(closed)    SET_BIT(exit->status, EX_CLOSED);
  else          REMOVE_BIT(exit->status, EX_CLOSED);
};

void        exitSetLocked(EXIT_DATA *exit, bool locked) {
  if(locked)    SET_BIT(exit->status, EX_LOCKED);
  else          REMOVE_BIT(exit->status, EX_LOCKED);
};

void        exitSetKey(EXIT_DATA *exit, int key) {
  exit->key = key;
};

void        exitSetHidden(EXIT_DATA *exit, int hide_lev) {
  exit->hide_lev = hide_lev;
};

void        exitSetPickLev(EXIT_DATA *exit, int pick_lev) {
  exit->pick_lev = pick_lev;
};

void        exitSetTo(EXIT_DATA *exit, int room) {
  exit->to = room;
};

void        exitSetName(EXIT_DATA *exit, const char *name) {
  if(exit->name) free(exit->name);
  if(name)       exit->name = strdup(name);
  else           exit->name = strdup("");
};

void        exitSetKeywords(EXIT_DATA *exit, const char *keywords) {
  if(exit->keywords) free(exit->keywords);
  if(keywords)       exit->keywords = strdup(keywords);
  else               exit->keywords = strdup("");
};

void        exitSetDesc(EXIT_DATA *exit, const char *desc) {
  bufferClear(exit->desc);
  bufferCat(exit->desc, (desc ? desc : ""));
};

void        exitSetSpecEnter(EXIT_DATA *exit, const char *enter) {
  if(exit->spec_enter)  free(exit->spec_enter);
  if(enter)             exit->spec_enter = strdup(enter);
  else                  exit->spec_enter = strdup("");
};

void        exitSetSpecLeave(EXIT_DATA *exit, const char *leave) {
  if(exit->spec_leave)  free(exit->spec_leave);
  if(leave)             exit->spec_leave = strdup(leave);
  else                  exit->spec_leave = strdup("");
};
