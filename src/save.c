#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* main header file */
#include "mud.h"
#include "character.h"
#include "world.h"
#include "utils.h"
#include "handler.h"
#include "body.h"
#include "object.h"
#include "room.h"
#include "storage.h"


void save_pfile           ( CHAR_DATA *ch );
void save_objfile         ( CHAR_DATA *ch );
void save_profile         ( CHAR_DATA *ch );


//
// Just a general function for finding the filename assocciated with the
// given information about a player.
//
#define FILETYPE_PROFILE      0
#define FILETYPE_PFILE        1
#define FILETYPE_OFILE        2
const char *get_char_filename(const char *name, int filetype) {
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
  case FILETYPE_PROFILE:
    sprintf(buf, "../lib/players/profiles/%c/%s.profile", *pname, pname);
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
  save_profile(ch);    // saves the players profile
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
  storage_write(set, get_char_filename(charGetName(ch), FILETYPE_OFILE));
  storage_close(set);
}


void save_pfile(CHAR_DATA *ch) {
  STORAGE_SET *set = charStore(ch);
  storage_write(set, get_char_filename(charGetName(ch), FILETYPE_PFILE));
  storage_close(set);
}


void load_ofile(CHAR_DATA *ch) {
  STORAGE_SET *set = storage_read(get_char_filename(charGetName(ch), 
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


CHAR_DATA *load_player(const char *player) {
  STORAGE_SET *set = storage_read(get_char_filename(player, FILETYPE_PFILE));
  CHAR_DATA   *ch  = charRead(set);
  storage_close(set);
  load_ofile(ch);
  return ch;
}


/*
 * This function loads a players profile, and stores
 * it in a mobile_data... DO NOT USE THIS DATA FOR
 * ANYTHING BUT CHECKING PASSWORDS OR SIMILAR.
 */
CHAR_DATA *load_profile(const char *player) {
  STORAGE_SET *set = storage_read(get_char_filename(player, FILETYPE_PROFILE));
  if(set == NULL) return NULL;
  CHAR_DATA    *ch = newChar();
  charSetName(ch,     read_string(set, "name"));
  charSetPassword(ch, read_string(set, "password"));
  charSetUID(ch,       read_int  (set, "uid"));
  storage_close(set);
  return ch;
}


/*
 * This file stores only data vital to load
 * the character, and check for things like
 * password and other such data.
 */
void save_profile(CHAR_DATA *ch) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "name",     charGetName(ch));
  store_string(set, "password", charGetPassword(ch));
  store_int   (set, "uid",      charGetUID(ch));
  storage_write(set, get_char_filename(charGetName(ch), FILETYPE_PROFILE));
  storage_close(set);
}
