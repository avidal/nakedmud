#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h> 

#include "mud.h"
#include "character.h"
#include "hashtable.h"

HASHTABLE   *   helptable = NULL;
char        *   greeting  = NULL;   /* the welcome greeting              */
char        *   motd      = NULL;   /* the MOTD help file                */

void load_helps() {
  helptable = newHashtable(2500);
}

void reload_helps() {

}

HELP_DATA *get_help(const char *keyword) {
  return hashGet(helptable, keyword);
}

void add_help(HELP_DATA *help) {

}

HELP_DATA *new_help(const char *keywords, 
		    const char *desc) {
  return NULL;
}

void show_help(CHAR_DATA *ch, HELP_DATA *help) {

}
