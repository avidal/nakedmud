//*****************************************************************************
//
// save.c
//
// contains all of the functions for the saving of characters and accounts.
// makes sure these things go into the right directories.
//
//*****************************************************************************

#include "mud.h"
#include "socket.h"
#include "character.h"
#include "account.h"
#include "world.h"
#include "utils.h"
#include "handler.h"
#include "body.h"
#include "object.h"
#include "room.h"
#include "storage.h"



//*****************************************************************************
// local functions
//*****************************************************************************

//
// Just a general function for finding the filename assocciated with the
// given information about a player.
#define FILETYPE_PFILE        0
#define FILETYPE_OFILE        1
#define FILETYPE_ACCOUNT      2
const char *get_save_filename(const char *name, int filetype) {
  static char buf[SMALL_BUFFER];
  static char pname[SMALL_BUFFER];
  int i, size;

  *buf = *pname = '\0';

  *pname = toupper(*name);
  size = strlen(name);
  for (i = 1; i < size; i++)
    pname[i] = tolower(*(name+i));
  pname[i] = '\0';

  switch(filetype) {
  case FILETYPE_ACCOUNT:
    sprintf(buf, "../lib/accounts/%c/%s.acct", *pname, pname);
    break;
  case FILETYPE_PFILE:
    sprintf(buf, "../lib/players/pfiles/%c/%s.pfile", *pname, pname);
    break;
  case FILETYPE_OFILE:
    sprintf(buf, "../lib/players/objfiles/%c/%s.ofile", *pname, pname);
    break;
  default: 
    log_string("ERROR: Tried to write filename for nonexistant char "
	       "file type, %d", filetype);
    break;
  }
  return buf;
}

bool char_exists(const char *name) {
  // there's two ways a character can exists. Either someone is already making
  // a character with that name, or there is a character with that name in
  // storage. We'll check both of these.
  const char *fname = get_save_filename(name, FILETYPE_PFILE);
  FILE *fl = fopen(fname, "r");
  if(fl != NULL) {
    fclose(fl);
    return TRUE;
  }

  bool char_found       = FALSE;
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL;
  ITERATE_LIST(sock, sock_i) {
    CHAR_DATA *ch = socketGetChar(sock);
    if(ch == NULL) continue;
    if(!strcasecmp(charGetName(ch), name)) {
      char_found = TRUE;
      break;
    }
  } deleteListIterator(sock_i);
  return char_found;
}

void save_pfile(CHAR_DATA *ch) {
  STORAGE_SET *set = charStore(ch);
  storage_write(set, get_save_filename(charGetName(ch), FILETYPE_PFILE));
  storage_close(set);
}


void load_ofile(CHAR_DATA *ch) {
  STORAGE_SET *set = storage_read(get_save_filename(charGetName(ch), 
						    FILETYPE_OFILE));
  if(set == NULL)
    return;

  STORAGE_SET   *obj_set = NULL;
  OBJ_DATA          *obj = NULL;

  // inventory first
  STORAGE_SET_LIST *list = read_list(set, "inventory");
  while( (obj_set = storage_list_next(list)) != NULL) {
    obj = objRead(obj_set);
    obj_to_char(obj, ch);
  }

  // and then equipped items
  list = read_list(set, "equipment");
  while( (obj_set = storage_list_next(list)) != NULL) {
    obj = objRead(obj_set);
    if(!try_equip(ch, obj, read_string(obj_set, "equipped")))
      obj_to_char(obj, ch);
  }

  storage_close(set);
}

void save_objfile(CHAR_DATA *ch) {
  STORAGE_SET *set = new_storage_set();
  // write all of the inventory
  store_list(set, "inventory", gen_store_list(charGetInventory(ch), objStore));
  
  // for equipped items, it's not so easy - we also have to record
  // whereabouts on the body the equipment was worn on
  STORAGE_SET_LIST *list = new_storage_list();
  LIST *eq_list = bodyGetAllEq(charGetBody(ch));
  OBJ_DATA *obj = NULL;
  while((obj = listPop(eq_list)) != NULL) {
    STORAGE_SET *eq_set = objStore(obj);
    store_string(set, "equipped", bodyEquippedWhere(charGetBody(ch), obj));
    storage_list_put(list, eq_set);
  }
  deleteList(eq_list);

  store_list(set, "equipment", list);
  storage_write(set, get_save_filename(charGetName(ch), FILETYPE_OFILE));
  storage_close(set);
}



//*****************************************************************************
// implementation of save.h
//*****************************************************************************
CHAR_DATA *load_player(const char *player) {
  STORAGE_SET *set = storage_read(get_save_filename(player, FILETYPE_PFILE));
  CHAR_DATA   *ch  = charRead(set);
  storage_close(set);
  load_ofile(ch);
  return ch;
}

void save_player(CHAR_DATA *ch) {
  if (!ch) return;

  // make sure we have a UID for the character before we start saving
  if(charGetUID(ch) == NOBODY) {
    log_string("ERROR: %s has invalid UID (%d)", charGetName(ch), NOBODY);
    send_to_char(ch, "You have an invalid ID. Please inform a god.\r\n");
    return;
  }

  save_objfile(ch);    // save the player's objects
  save_pfile(ch);      // saves the actual player data
}

void save_account(ACCOUNT_DATA *account) {
  if(!account) return;
  STORAGE_SET *set = accountStore(account);
  storage_write(set, get_save_filename(accountGetName(account),
				       FILETYPE_ACCOUNT));
  storage_close(set);
}

ACCOUNT_DATA *load_account(const char *account) {
  STORAGE_SET   *set = storage_read(get_save_filename(account,
						      FILETYPE_ACCOUNT));
  if(set == NULL)
    return NULL;

  ACCOUNT_DATA *acct = accountRead(set);
  storage_close(set);
  return acct;
}
