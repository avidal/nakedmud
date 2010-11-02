//*****************************************************************************
//
// mud.c
//
// a bunch of variables and settings associated with the functioning, limits,
// and such of the MUD.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "storage.h"


#define MUD_DATA       "../lib/muddata"

int top_char_uid = 0;


void init_mud_settings() {
  STORAGE_SET *set = storage_read(MUD_DATA);
  top_char_uid     = read_int(set, "puid");
}

void save_mud_settings() {
  STORAGE_SET *set = storage_read(MUD_DATA);
  store_int(set, "puid", top_char_uid);
  storage_write(set, MUD_DATA);
  storage_close(set);
}

int next_char_uid() {
  int uid = ++top_char_uid;
  save_mud_settings();
  return uid;
}
