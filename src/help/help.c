//*****************************************************************************
//
// help.c
//
// This helpfile system has been set up to be amenable to allowing players to
// edit helpfiles without having to worry about malicious users. 
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../storage.h"
#include "../character.h"
#include "../socket.h"

#include "../editor/editor.h"

#include "help.h"



//*****************************************************************************
//
// local variables, data structures, and functions
//
//*****************************************************************************

// the file we keep all of our help in
#define HELP_FILE          "../lib/misc/help"

// how many buckets are in our help table
#define HELP_TABLE_BUCKETS             27 // 26 for letters, 1 for non-alphas

// the table we store all of our helpfiles in
LIST *help_table[HELP_TABLE_BUCKETS];

typedef struct help_data {
  char *keywords;   // words that bring up this helpfile
  char *info;       // the information in the helpfile
  char *editor;     // who edited the helpfile?
  char *timestamp;  // when was it last edited?
  LIST *backups;    // a chronologically sorted list of backup helps
} HELP_DATA;


HELP_DATA *newHelp(const char *editor, const char *timestamp,
		   const char *keywords, const char *info) {
  HELP_DATA *data = malloc(sizeof(HELP_DATA));
  data->keywords  = strdupsafe(keywords);
  data->editor    = strdupsafe(editor);
  data->info      = strdupsafe(info);
  data->timestamp = strdupsafe(timestamp);
  data->backups   = newList();
  return data;
}

void deleteHelp(HELP_DATA *data) {
  deleteListWith(data->backups, deleteHelp);
  if(data->keywords)  free(data->keywords);
  if(data->editor)    free(data->editor);
  if(data->timestamp) free(data->timestamp);
  if(data->info)      free(data->info);
  free(data);
}

HELP_DATA *helpRead(STORAGE_SET *set) {
  HELP_DATA *data = newHelp(read_string(set, "editor"),
			    read_string(set, "timestamp"),
			    read_string(set, "keywords"),
			    read_string(set, "info"));
  deleteList(data->backups);
  data->backups = gen_read_list(read_list(set, "backups"), helpRead);
  return data;
}

STORAGE_SET *helpStore(HELP_DATA *help) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "keywords",  help->keywords);
  store_string(set, "editor",    help->editor);
  store_string(set, "timestamp", help->timestamp);
  store_string(set, "info",      help->info);
  store_list  (set, "backups",   gen_store_list(help->backups, helpStore));
  return set;
}


//
// HELP_DATA holds all of the information about a helpfile, but HELP_ENTRY
// is a key/value pair used for storing helpfiles in the help table.
//
typedef struct help_entry {
  char   *keyword;
  HELP_DATA *help;
} HELP_ENTRY;

HELP_ENTRY *newHelpEntry(const char *keyword, HELP_DATA *data) {
  HELP_ENTRY *entry = malloc(sizeof(HELP_ENTRY));
  entry->keyword = strdupsafe(keyword);
  entry->help    = data;
  return entry;
}

void deleteHelpEntry(HELP_ENTRY *entry) {
  if(entry->keyword) free(entry->keyword);
  free(entry);
}

int hentrycmp(HELP_ENTRY *entry1, HELP_ENTRY *entry2) {
  return strcasecmp(entry1->keyword, entry2->keyword);
}

int is_help_entry(const char *help, HELP_ENTRY *entry) {
  return strcasecmp(help, entry->keyword);
}

int is_help_abbrev(const char *help, HELP_ENTRY *entry) {
  return strncasecmp(help, entry->keyword, strlen(help));
}


//
// return the bucket that the specified helpfile belongs to
//
int helpbucket(const char *help) {
  if(isalpha(*help))
    return (tolower(*help) - 'a');
  else
    return HELP_TABLE_BUCKETS-1;
}


//
// save all of the helpfiles
//
void save_help() {
  STORAGE_SET      *set = new_storage_set();
  LIST       *help_list = newList();

  // iterate across the table and save all the unique helpfiles
  int i;
  for(i = 0; i < HELP_TABLE_BUCKETS; i++) {
    LIST_ITERATOR *row_i = newListIterator(help_table[i]);
    HELP_ENTRY    *entry = NULL;
    ITERATE_LIST(entry, row_i)
      listPut(help_list, entry->help);
    deleteListIterator(row_i);
  }

  store_list(set, "helpfiles", gen_store_list(help_list, helpStore));
  deleteList(help_list);

  // write the set
  storage_write(set, HELP_FILE);
  
  // close the set
  storage_close(set);
}


//
// add a helpfile to the help table. Assocciate all of its keywords with it
//
void add_help(HELP_DATA *help) {
  int num_keywords = 0;
  char  **keywords = parse_keywords(help->keywords, &num_keywords);
  int i;
  for(i = 0; i < num_keywords; i++) {
    LIST *row = help_table[helpbucket(keywords[i])];
    HELP_ENTRY *new_entry = newHelpEntry(keywords[i], help);
    HELP_ENTRY *old_entry = NULL;
    // make sure we're not already in the help table
    if((old_entry = listRemoveWith(row, new_entry, hentrycmp)) != NULL)
      deleteHelpEntry(old_entry);
    listPutWith(row, new_entry, hentrycmp);
    free(keywords[i]);
  }
  free(keywords);
}


//
// Returns the datastructure for the helpfile
//
HELP_DATA *get_help_data(const char *keyword, bool abbrev_ok) {
  HELP_ENTRY *entry = listGetWith(help_table[helpbucket(keyword)], keyword, 
				  (abbrev_ok ? is_help_abbrev : is_help_entry));
  return (entry ? entry->help : NULL);
}


//
// remove a helpfile from the help table
//
HELP_DATA *remove_help(const char *keyword) {
  HELP_ENTRY *entry = listRemoveWith(help_table[helpbucket(keyword)], keyword, 
				     is_help_entry);
  HELP_DATA *help = (entry ? entry->help : NULL);
  if(entry) deleteHelpEntry(entry);
  return help;
}


//
// return a list of matches for the specified help keyword. If we find an
// exact match, only that match will be contained within the list.
// List must be deleted after use.
//
LIST *help_matches(const char *keyword) {
  int bucket  = helpbucket(keyword);
  int key_len = strlen(keyword);
  LIST_ITERATOR *buck_i = newListIterator(help_table[bucket]);
  HELP_ENTRY     *entry = NULL;
  LIST         *matches = newList();

  ITERATE_LIST(entry, buck_i) {
    // exact match
    if(!strcasecmp(keyword, entry->keyword)) {
      while(listSize(matches) > 0) listPop(matches);
      listPut(matches, entry);
      break;
    }
    else if(!strncasecmp(keyword, entry->keyword, key_len))
      listQueue(matches, entry);
  }
  deleteListIterator(buck_i);
  return matches;
}


//
// Show the contents of a helpfile to the character. If no argument is
// supplied, list all of the helpfiles. If we try to get info on a help
// that does not exist, list near-matches.
//
// usage:
//   help <topic>
//
COMMAND(cmd_help) {
  BUFFER *buf = build_help(arg);
  if(buf == NULL)
    send_to_char(ch, "No help exists on that topic.\r\n");
  else {
    // send this to the character
    if(charGetSocket(ch))
      page_string(charGetSocket(ch), bufferString(buf));
    deleteBuffer(buf);
  }
}


//
// same as cmd_write, but loads the contents of the helpfile into the buffer
//
COMMAND(cmd_hedit) {
  if(!arg || !*arg) {
    send_to_char(ch, "Which helpfile are you trying to edit?\r\n");
    return;
  }
  if(!charGetSocket(ch)) {
    send_to_char(ch, "You must have a socket attached to use hedit.\r\n");
    return;
  }

  // because we replace underscores with spaces in hlink,
  // we should do the same thing here for consistancy's sake
  char *ptr;
  for(ptr = arg; *ptr; ptr++)
    if(*ptr == '_') *ptr = ' ';

  HELP_DATA *data = get_help_data(arg, FALSE);

  socketSetNotepad(charGetSocket(ch), (data ? data->info : ""));
  socketStartNotepad(charGetSocket(ch), text_editor);
}


//
// Edit the contents of a helpfile
//
COMMAND(cmd_hupdate) {
  if(!arg || !*arg) {
    send_to_char(ch, "Which helpfile were you trying to update?\r\n");
    return;
  }
  if(!charGetSocket(ch) || !bufferLength(socketGetNotepad(charGetSocket(ch)))) {
    send_to_char(ch, "You have nothing in your notepad! Try writing something.\r\n");
    return;
  }

  // because we replace underscores with spaces in hlink,
  // we should do the same thing here for consistancy's sake
  char *ptr;
  for(ptr = arg; *ptr; ptr++)
    if(*ptr == '_') *ptr = ' ';

  update_help(charGetName(ch), arg, 
	      bufferString(socketGetNotepad(charGetSocket(ch))));
}


//
// Link a keyword to an already-existing helpfile. underscores must be
// used instead of spaces.
//   usage:
//     hlink [new_topic] [old_topic]
//
COMMAND(cmd_hlink) {
  char new_help[SMALL_BUFFER];

  if(!arg || !*arg) {
    send_to_char(ch, "Link which helpfile to which?\r\n");
    return;
  }

  arg = one_arg(arg, new_help);

  if(!*new_help || !*arg) {
    send_to_char(ch, "You must provide a new keyword and an old keyword "
		 "to link it to.\r\n");
    return;
  }

  // kill all of the underscores, and put spaces in instead
  char *ptr;
  for(ptr = new_help; *ptr; ptr++)
    if(*ptr == '_') *ptr = ' ';
  for(ptr = arg; *ptr; ptr++)
    if(*ptr == '_') *ptr = ' ';

  // find the help we're trying to link to
  HELP_DATA *help = get_help_data(arg, FALSE);

  if(help == NULL) {
    send_to_char(ch, "No help exists for %s.\r\n", arg);
    return;
  }


  // perform the link
  link_help(new_help, arg);
  send_to_char(ch, "%s linked to %s.\r\n", new_help, arg);
}


//
// Unlink a keyword from its assocciated helpfile. 
//   usage:
//     hunlink [topic]
//
COMMAND(cmd_hunlink) {
  if(!arg || !*arg) {
    send_to_char(ch, "Unlink which helpfile?\r\n");
    return;
  }

  // because we replace underscores with spaces in hlink,
  // we should do the same thing here for consistancy's sake
  char *ptr;
  for(ptr = arg; *ptr; ptr++)
    if(*ptr == '_') *ptr = ' ';

  // find the social we're trying to link to
  HELP_DATA *help = get_help_data(arg, FALSE);

  if(help == NULL) {
    send_to_char(ch, "No help exists for %s.\r\n", arg);
    return;
  }

  // perform the unlinking
  unlink_help(arg);
  send_to_char(ch, "The %s helpfile was unlinked.\r\n", arg);
}



//*****************************************************************************
//
// implementation of help.h
//
//*****************************************************************************
void init_help() {
  // init all of our buckets
  int i;
  for(i = 0; i < HELP_TABLE_BUCKETS; i++)
    help_table[i] = newList();

  // add all of our commands
  add_cmd("help", NULL, cmd_help, POS_UNCONCIOUS, POS_FLYING,
	  "player", FALSE, FALSE);
  add_cmd("hlink", NULL, cmd_hlink, POS_UNCONCIOUS, POS_FLYING,
	  "admin", FALSE, FALSE);
  add_cmd("hunlink", NULL, cmd_hunlink, POS_UNCONCIOUS, POS_FLYING,
	  "admin", FALSE, FALSE);
  add_cmd("hupdate", NULL, cmd_hupdate, POS_SITTING, POS_FLYING,
	  "builder", FALSE, TRUE);
  add_cmd("hedit", NULL, cmd_hedit, POS_SITTING, POS_FLYING,
	  "builder", FALSE, TRUE);

  // read in all of our helps
  STORAGE_SET       *set = storage_read(HELP_FILE);
  STORAGE_SET_LIST *list = read_list(set, "helpfiles");
  STORAGE_SET     *entry = NULL;

  // parse all of the helpfiles
  while( (entry = storage_list_next(list)) != NULL)
    add_help(helpRead(entry));

  storage_close(set);
}

BUFFER *build_help(const char *keyword) {
  // we have to edit the keyword... dup it
  char   *arg = strdup(keyword);
  BUFFER *buf = newBuffer(MAX_BUFFER);
  // no argument. Show a list of all our help topics
  if(!*arg) {
    int count, i;
    bprintf(buf, "Help is available on the following topics:\r\n");
    for(i = 0, count = 0; i < HELP_TABLE_BUCKETS; i++) {
      LIST_ITERATOR *buck_i = newListIterator(help_table[i]);
      HELP_ENTRY     *entry = NULL;
      ITERATE_LIST(entry, buck_i) {
	bprintf(buf, "%-16s", entry->keyword);
	count++;
	// only 4 entries per row
	if((count % 5) == 0) 
	  bufferCat(buf, "\r\n");
      } deleteListIterator(buck_i);
    }

    // make sure we add a newline to the end
    if((count % 5) != 0)
      bufferCat(buf, "\r\n");
  }

  // pull out the helpfile
  else {
    // because we replace underscores with spaces in hlink,
    // we should do the same thing here for consistancy's sake
    char *ptr;
    for(ptr = arg; *ptr; ptr++)
      if(*ptr == '_') *ptr = ' ';

    LIST *matches = help_matches(arg);
    HELP_ENTRY *entry = NULL;
    // no matches found
    if(listSize(matches) == 0)
      bprintf(buf, "No help exists for %s.\r\n", arg);
    // one match found
    else if(listSize(matches) == 1) {
      entry = listPop(matches);
      char header[128]; // +2 for \r\n, +1 for \0
      center_string(header, entry->keyword, 80, 128, TRUE);
      bprintf(buf, "%s{wBy: %-36s%40s\r\n\r\n{n%s",
	      header, entry->help->editor, entry->help->timestamp,
	      entry->help->info);
    }

    // more than one match found. Tell person to narrow search
    else {
      bprintf(buf, "More than one entry matched your query: \r\n");
      LIST_ITERATOR *match_i = newListIterator(matches);
      ITERATE_LIST(entry, match_i) {
	bprintf(buf, "{c%s ", entry->keyword);
      } deleteListIterator(match_i);
      bufferCat(buf, "{n\r\n");
    }
    
    // delete the list of matches we found
    deleteList(matches);
  }

  // clean up our mess
  free(arg);

  // return whatever we created
  return buf;
}


void update_help(const char *editor, const char *keyword, const char *info) {
  HELP_DATA *data = get_help_data(keyword, FALSE);

  if(data != NULL) {
    HELP_DATA *help_old = newHelp(data->editor, data->timestamp, data->keywords,
				  data->info);
    if(data->editor)    free(data->editor);
    if(data->timestamp) free(data->timestamp);
    if(data->info)      free(data->info);
    
    data->editor      = strdupsafe(editor);
    data->info        = strdupsafe(info);
    data->timestamp   = strdup(get_time());

    listPut(data->backups, help_old);
  }
  else
    add_help(newHelp(editor, get_time(), keyword, info));

  save_help();
}


void unlink_help(const char *keyword) {
  // remove the helpfile
  HELP_DATA *data = remove_help(keyword);

  // unlink the helpfile
  if(data != NULL) {
    remove_keyword(data->keywords, keyword);

    // if no links are left, delete the helpfile
    if(!*data->keywords)
      deleteHelp(data);
    
    // save our changes
    save_help();
  }
}


void link_help(const char *keyword, const char *old_word) {
  // check for the old keyword
  HELP_DATA *data = get_help_data(old_word, FALSE);

  // link the new keyword to the old one
  if(data != NULL) {
    // first, remove the current new_keyword help, if it exists
    unlink_help(keyword);

    // add the new keyword to the help
    add_keyword(&(data->keywords), keyword);

    // add the new command to the help table
    LIST *row = help_table[helpbucket(keyword)];
    HELP_ENTRY *new_entry = newHelpEntry(keyword, data);
    listPutWith(row, new_entry, hentrycmp);

    // save our changes
    save_help();
  }
}
