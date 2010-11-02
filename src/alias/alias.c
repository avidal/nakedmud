//*****************************************************************************
//
// alias.c
//
// Aliases allow a player to set up one command as another (e.g. "eat bread"
// as "food").
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../auxiliary.h"
#include "../storage.h"
#include "../character.h"

#include "alias.h"



//*****************************************************************************
// auxiliary data
//*****************************************************************************
typedef struct alias_aux_data {
  HASHTABLE *aliases;
} ALIAS_AUX_DATA;


ALIAS_AUX_DATA *
newAliasAuxData() {
  ALIAS_AUX_DATA *data = malloc(sizeof(ALIAS_AUX_DATA));
  //
  // Hashtables can take up lots of storage space. Because of this, let's
  // not create any tables until it's actually needed. This will cut down
  // on lots of memory usage w.r.t. NPCs who do not use aliases
  //  data->aliases        = newHashtable();
  //
  data->aliases        = NULL;
  return data;
}


void
deleteAliasAuxData(ALIAS_AUX_DATA *data) {
  if(data->aliases) {
    HASH_ITERATOR *hash_i = newHashIterator(data->aliases);
    const char     *alias = NULL;
    char             *cmd = NULL;

    ITERATE_HASH(alias, cmd, hash_i) 
      free(cmd);
    deleteHashIterator(hash_i);
    deleteHashtable(data->aliases);
  }
  free(data);
}


void
aliasAuxDataCopyTo(ALIAS_AUX_DATA *from, ALIAS_AUX_DATA *to) {
  // aliases are for personal use, and it is doubtful we would
  // ever have a motivation for copying them over. Thus, we shall
  // not do any copying here.
}


ALIAS_AUX_DATA *
aliasAuxDataCopy(ALIAS_AUX_DATA *data) {
  // as with aliasAuxDataCopyTo, there seems to be little reason
  // why we would ever want to copy aliases. They are personal,
  // and have no reason for being copied. Thus, let us just return
  // a new datastructure.
  return newAliasAuxData();
}


STORAGE_SET *aliasAuxDataStore(ALIAS_AUX_DATA *data) {
  // if we have no hashtable, return an empty set
  if(data->aliases == NULL || hashSize(data->aliases) == 0)
    return new_storage_set();

  STORAGE_SET *set       = new_storage_set();
  STORAGE_SET_LIST *list = new_storage_list();
  HASH_ITERATOR  *hash_i = newHashIterator(data->aliases);
  const char       *name = NULL;
  const char        *cmd = NULL;

  store_list(set, "aliases", list);
  ITERATE_HASH(name, cmd, hash_i) {
    STORAGE_SET *alias_set = new_storage_set();
    store_string(alias_set, "key", name);
    store_string(alias_set, "val", hashIteratorCurrentVal(hash_i));
    storage_list_put(list, alias_set);
  }
  deleteHashIterator(hash_i);
  return set;
}


ALIAS_AUX_DATA *aliasAuxDataRead(STORAGE_SET *set) {
  // if the file contains no alias data, then don't load any
  if(!storage_contains(set, "aliases"))
    return newAliasAuxData();

  ALIAS_AUX_DATA *data   = newAliasAuxData();
  data->aliases          = newHashtable();
  STORAGE_SET_LIST *list = read_list(set, "aliases");
  STORAGE_SET *var       = NULL;

  while( (var = storage_list_next(list)) != NULL)
    hashPut(data->aliases, read_string(var, "key"), 
	    strdup(read_string(var, "val")));
  return data;
}


void init_aliases() {
  // install aliases on the character datastructure
  auxiliariesInstall("alias_aux_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_CHAR,
				       newAliasAuxData, deleteAliasAuxData,
				       aliasAuxDataCopyTo, aliasAuxDataCopy,
				       aliasAuxDataStore, aliasAuxDataRead));

  // allow people to view their aliases
  add_cmd("alias", NULL, cmd_alias, 0, POS_UNCONCIOUS, POS_FLYING, 
	  LEVEL_PLAYER, FALSE, TRUE);
}



//*****************************************************************************
//
// functions for interacting with character aliases
//
//*****************************************************************************
const char *charGetAlias(CHAR_DATA *ch, const char *alias) {
  ALIAS_AUX_DATA *data = charGetAuxiliaryData(ch, "alias_aux_data");
  if(data->aliases == NULL)
    return NULL;
  return hashGet(data->aliases, alias);
}


void charSetAlias(CHAR_DATA *ch, const char *alias, const char *cmd){
  ALIAS_AUX_DATA *data = charGetAuxiliaryData(ch, "alias_aux_data");
  // if our alias table is NULL, create a new one
  if(data->aliases == NULL)
    data->aliases = newHashtable();

  // pull out the last one
  char *oldcmd = hashRemove(data->aliases, alias);
  if(oldcmd != NULL)
    free(oldcmd);
  // put in the new one if it exists
  if(cmd && *cmd)
    hashPut(data->aliases, alias, strdup(cmd));
}


char *expand_alias(const char *alias, const char *arg) {
  char *cmd = strdup(alias);

  // first, we replace all occurances of the numeric parameters
  int i;
  for(i = 1; i < 10; i++) {
    char param[3];
    char one_arg[SMALL_BUFFER];
    sprintf(param, "$%d", i);
    arg_num(arg, one_arg, i);
    replace_string(&cmd, param, one_arg, TRUE);
  }

  // then we replace the wildcard
  replace_string(&cmd, "$*", arg, TRUE);
  return cmd;
}


//
// Set or delete an alias. If no argument is supplied, all aliases are listed.
//
COMMAND(cmd_alias) {
  // list off all of the aliases
  if(!arg || !*arg) {
    ALIAS_AUX_DATA *data = charGetAuxiliaryData(ch, "alias_aux_data");
    send_to_char(ch, "Current aliases:\r\n");
    if(data->aliases == NULL || hashSize(data->aliases) == 0)
      send_to_char(ch, "  none\r\n");
    else {
      HASH_ITERATOR *alias_i = newHashIterator(data->aliases);
      const char      *alias = NULL;
      const char        *cmd = NULL;

      ITERATE_HASH(alias, cmd, alias_i)
	send_to_char(ch, "  %-20s %s\r\n", alias, cmd);
      deleteHashIterator(alias_i);
    }
  }
  else {
    char alias[SMALL_BUFFER];
    arg = one_arg(arg, alias);

    // try to delete an alias
    if(!arg || !*arg) {
      const char *curr_cmd = charGetAlias(ch, alias);
      if(!curr_cmd)
	send_to_char(ch, "You do not have such an alias.\r\n");
      else {
	charSetAlias(ch, alias, arg);
	send_to_char(ch, "Alias deleted.\r\n");
      }
    }

    // try to set an alias
    else {
      charSetAlias(ch, alias, arg);
      send_to_char(ch, "Alias set.\r\n");
    }
  }
}
