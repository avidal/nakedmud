//*****************************************************************************
//
// medit.c
//
// When mob prototypes became python scripts, OLC for mobs had to be rethought.
// Ideally, all builders would have a basic grasp on python and thus would be
// able to write scripts. Ideally. Sadly, I don't think this can be expected
// out of most builders, and we still need some sort of non-scripting interface
// for editing mobs. So here it is...
//
//*****************************************************************************
#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../races.h"
#include "../world.h"
#include "../character.h"
#include "../prototype.h"

#include "olc.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../editor/editor.h"
#include "../scripts/scripts.h"
#include "../scripts/script_editor.h"



//*****************************************************************************
// mob olc structure, and functions
//*****************************************************************************
typedef struct {
  char          *key; // the key for our prototype
  char      *parents; // things we inherit from
  bool      abstract; // can we be laoded into the game?
  CHAR_DATA      *ch; // our character, which holds most of our variables
  BUFFER *extra_code; // any extra code that should go to our prototype
} CHAR_OLC;

CHAR_OLC *newCharOLC(void) {
  CHAR_OLC *data  = malloc(sizeof(CHAR_OLC));
  data->key       = strdup("");
  data->parents   = strdup("");
  data->abstract  = TRUE;
  data->ch        = newMobile();
  charSetRace(data->ch, "");
  data->extra_code = newBuffer(1);
  charSetSex(data->ch, SEX_NONE);
  return data;
}

void deleteCharOLC(CHAR_OLC *data) {
  if(data->key)        free(data->key);
  if(data->parents)    free(data->parents);
  if(data->extra_code) deleteBuffer(data->extra_code);
  if(data->ch)         deleteChar(data->ch);
  free(data);
}

const char *charOLCGetKey(CHAR_OLC *data) {
  return data->key;
}

const char *charOLCGetParents(CHAR_OLC *data) {
  return data->parents;
}

bool charOLCGetAbstract(CHAR_OLC *data) {
  return data->abstract;
}

CHAR_DATA *charOLCGetChar(CHAR_OLC *data) {
  return data->ch;
}

BUFFER *charOLCGetExtraCode(CHAR_OLC *data) {
  return data->extra_code;
}

void charOLCSetKey(CHAR_OLC *data, const char *key) {
  if(data->key) free(data->key);
  data->key = strdup(key);
}

void charOLCSetParents(CHAR_OLC *data, const char *parents) {
  if(data->parents) free(data->parents);
  data->parents = strdup(parents);
}

void charOLCSetAbstract(CHAR_OLC *data, bool abstract) {
  data->abstract = abstract;
}

//
// takes in a char prototype, and tries to generate a char olc out of it. This
// function is messy and ugly and icky and yuck. But, alas, I cannot think of
// a better way to do this. Maybe next version...
CHAR_OLC *charOLCFromProto(PROTO_DATA *proto) {
  CHAR_OLC *data = newCharOLC();
  CHAR_DATA  *ch = charOLCGetChar(data);
  charOLCSetKey(data, protoGetKey(proto));
  charOLCSetParents(data, protoGetParents(proto));
  charOLCSetAbstract(data, protoIsAbstract(proto));

  // this is a really ugly way to do the conversion, but basically let's
  // just look through every line in the buffer and if we recognize some
  // token, parse out whatever is assigned to it
  char line[MAX_BUFFER];
  const char *code = protoGetScript(proto);
  do {
    code = strcpyto(line, code, '\n');
    char *lptr = line;
    if(!strncmp(lptr, "me.name", 7)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      charSetName(ch, lptr);
    }
    else if(!strncmp(lptr, "me.mname", 8)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      charSetMultiName(ch, lptr);
    }
    else if(!strncmp(lptr, "me.rdesc", 8)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      charSetRdesc(ch, lptr);
    }
    else if(!strncmp(lptr, "me.mdesc", 8)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      charSetMultiRdesc(ch, lptr);
    }
    else if(!strncmp(lptr, "me.desc",  7)) {
      // we have three "'s to skip by, because this lptr will take the form:
      // me.desc = me.desc + " " + "..."
      while(*lptr != '\"') lptr++; lptr++;
      while(*lptr != '\"') lptr++; lptr++;
      while(*lptr != '\"') lptr++; lptr++;
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      charSetDesc(ch, lptr);
      // replace our \"s with "
      bufferReplace(charGetDescBuffer(ch), "\\\"", "\"", TRUE);
      bufferFormat(charGetDescBuffer(ch), SCREEN_WIDTH, PARA_INDENT);
    }
    else if(!strncmp(lptr, "me.keywords", 11)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[next_letter_in(lptr, '\"')] = '\0'; // kill the ending "
      charSetKeywords(ch, lptr);
    }
    else if(!strncmp(lptr, "me.gender", 9)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      charSetSex(ch, sexGetNum(lptr));
    }
    else if(!strncmp(lptr, "me.race", 7)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      charSetRace(ch, lptr);
    }
    else if(!strncmp(lptr, "me.attach(\"", 11)) {
      char trigname[SMALL_BUFFER];
      sscanf(lptr, "me.attach(\"%s", trigname);
      // kill our ending ")
      trigname[strlen(trigname)-2] = '\0';
      triggerListAdd(charGetTriggers(ch), trigname);
    }
    else if(!strcmp(lptr, "### begin extra code")) {
      code = strcpyto(line, code, '\n');
      while(strcmp(line, "### end extra code") != 0) {
	bprintf(charOLCGetExtraCode(data), "%s\n", line);
	if(!*code) break;
	code = strcpyto(line, code, '\n');
      }
    }
    // we didn't recognize the line... just ignore it
    else
      continue;
  } while(*code != '\0');

  return data;
}

//
// takes in a char olc and tries to generate a prototype out of it
PROTO_DATA *charOLCToProto(CHAR_OLC *data) {
  PROTO_DATA *proto = newProto();
  CHAR_DATA     *ch = charOLCGetChar(data);
  BUFFER       *buf = protoGetScriptBuffer(proto);
  protoSetKey(proto, charOLCGetKey(data));
  protoSetParents(proto, charOLCGetParents(data));
  protoSetAbstract(proto, charOLCGetAbstract(data));

  bprintf(buf, "### The following mproto was generated by medit\n");
  bprintf(buf, "### If you edit this script, adhere to the stylistic\n"
	       "### conventions laid out by medit, or delete the top line\n");

  bprintf(buf, "\n### keywords, short descs, room descs, and look descs\n");
  if(*charGetKeywords(ch))
    bprintf(buf, "me.keywords = \"%s\"  + \", \" + me.keywords\n", 
	    charGetKeywords(ch));
  if(*charGetName(ch))
    bprintf(buf, "me.name     = \"%s\"\n", charGetName(ch));
  if(*charGetMultiName(ch))
    bprintf(buf, "me.mname    = \"%s\"\n", charGetMultiName(ch));
  if(*charGetRdesc(ch))
    bprintf(buf, "me.rdesc    = \"%s\"\n", charGetRdesc(ch));
  if(*charGetMultiRdesc(ch))
    bprintf(buf, "me.mdesc    = \"%s\"\n", charGetMultiRdesc(ch));
  if(*charGetDesc(ch)) {
    BUFFER *desc_copy = bufferCopy(charGetDescBuffer(ch));
    bufferReplace(desc_copy, "\n", " ", TRUE);
    bufferReplace(desc_copy, "\r", "",  TRUE);
    bufferReplace(desc_copy, "\"", "\\\"", TRUE);
    bprintf(buf, "me.desc     = me.desc + \" \" + \"%s\"\n", 
	    bufferString(desc_copy));
    deleteBuffer(desc_copy);
  }
  
  if(*charGetRace(ch) || charGetSex(ch) != SEX_NONE) {
    bprintf(buf, "\n### race and gender\n");
    if(*charGetRace(ch))
      bprintf(buf, "me.race     = \"%s\"\n", charGetRace(ch));
    if(charGetSex(ch) != SEX_NONE)
      bprintf(buf, "me.gender   = \"%s\"\n", sexGetName(charGetSex(ch)));
  }

  if(listSize(charGetTriggers(ch)) > 0) {
    bprintf(buf, "\n### character triggers\n");
    LIST_ITERATOR *trig_i = newListIterator(charGetTriggers(ch));
    char            *trig = NULL;
    ITERATE_LIST(trig, trig_i) {
      bprintf(buf, "me.attach(\"%s\")\n", trig);
    } deleteListIterator(trig_i);
  }

  if(bufferLength(charOLCGetExtraCode(data)) > 0) {
    bprintf(buf, "\n### begin extra code\n");
    bprintf(buf, "%s", bufferString(charOLCGetExtraCode(data)));
    bprintf(buf, "### end extra code\n");
  }
  return proto;
}



//*****************************************************************************
// mobile editing
//*****************************************************************************
#define MEDIT_PARENTS       1
#define MEDIT_NAME          2
#define MEDIT_MULTI_NAME    3
#define MEDIT_KEYWORDS      4
#define MEDIT_RDESC         5
#define MEDIT_MULTI_RDESC   6
#define MEDIT_RACE          7
#define MEDIT_SEX           8


void medit_menu(SOCKET_DATA *sock, CHAR_OLC *data) {
  send_to_socket(sock,
		 "{g[{c%s{g]\r\n"
		 "{g1) Abstract: {c%s\r\n"
		 "{g2) Inherits from prototypes:\r\n"
		 "{c%s\r\n"
		 "{g3) Name\r\n"
		 "{c%s\r\n"
		 "{g4) Name for multiple occurences:\r\n"
		 "{c%s\r\n"
		 "{g5) Keywords:\r\n"
		 "{c%s\r\n"
		 "{g6) Room description:\r\n"
		 "{c%s\r\n"
		 "{g7) Room description for multiple occurences:\r\n"
		 "{c%s\r\n"
		 "{g8) Description:\r\n"
		 "{c%s"
		 "{gT) Trigger menu\r\n"
		 "{gR) Change race   {y[{c%8s{y]\r\n"
		 "{gG) Change Gender {y[{c%8s{y]\r\n",
		 charOLCGetKey(data),
		 (charOLCGetAbstract(data) ? "yes" : "no"),
		 charOLCGetParents(data),
		 charGetName(charOLCGetChar(data)),
		 charGetMultiName(charOLCGetChar(data)),
		 charGetKeywords(charOLCGetChar(data)),
		 charGetRdesc(charOLCGetChar(data)),
		 charGetMultiRdesc(charOLCGetChar(data)),
		 charGetDesc(charOLCGetChar(data)),
		 (!*charGetRace(charOLCGetChar(data)) ?
		  "leave unchanged" :
		  charGetRace(charOLCGetChar(data))),
		 (charGetSex(charOLCGetChar(data)) == SEX_NONE ? 
		  "leave unchanged" :
		  sexGetName(charGetSex(charOLCGetChar(data))))
		 );

  // only allow code editing for people with scripting priviledges
  send_to_socket(sock, "{gC) Extra code%s\r\n", 
		 ((!socketGetChar(sock) ||  
		   !bitIsOneSet(charGetUserGroups(socketGetChar(sock)),
				"scripter")) ? "    {y({cuneditable{y){g":""));
  script_display(sock, bufferString(charOLCGetExtraCode(data)), FALSE);
}

int  medit_chooser(SOCKET_DATA *sock, CHAR_OLC *data, const char *option) {
  switch(toupper(*option)) {
  case '1':
    charOLCSetAbstract(data, (charOLCGetAbstract(data) + 1) % 2);
    return MENU_NOCHOICE;
  case '2':
    text_to_buffer(sock,"Enter comma-separated list of mobs to inherit from: ");
    return MEDIT_PARENTS;
  case '3':
    text_to_buffer(sock, "Enter name: ");
    return MEDIT_NAME;
  case '4':
    text_to_buffer(sock, "Enter name for multiple occurences: ");
    return MEDIT_MULTI_NAME;
  case '5':
    text_to_buffer(sock, "Enter keywords: ");
    return MEDIT_KEYWORDS;
  case '6':
    text_to_buffer(sock, "Enter room description: ");
    return MEDIT_RDESC;
  case '7':
    text_to_buffer(sock, "Enter room description for multiple occurences: ");
    return MEDIT_MULTI_RDESC;
  case '8':
    text_to_buffer(sock, "Enter description\r\n");
    socketStartEditor(sock,text_editor,charGetDescBuffer(charOLCGetChar(data)));
    return MENU_NOCHOICE;
  case 'R':
    send_to_socket(sock, "%s\r\n\r\n", raceGetList(FALSE));
    text_to_buffer(sock, "Please select a race: ");
    return MEDIT_RACE;
  case 'G':
    olc_display_table(sock, sexGetName, NUM_SEXES, 1);
    text_to_buffer(sock, "Pick a gender: ");
    return MEDIT_SEX;
  case 'T':
    do_olc(sock, trigger_list_menu, trigger_list_chooser, trigger_list_parser,
	   NULL, NULL, NULL, NULL, charGetTriggers(charOLCGetChar(data)));
    return MENU_NOCHOICE;
  case 'C':
    // only scripters can edit extra code
    if(!socketGetChar(sock) || 
       !bitIsOneSet(charGetUserGroups(socketGetChar(sock)), "scripter"))
      return MENU_CHOICE_INVALID;
    text_to_buffer(sock, "Edit extra code\r\n");
    socketStartEditor(sock,script_editor,charOLCGetExtraCode(data));
    return MENU_NOCHOICE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool medit_parser(SOCKET_DATA *sock, CHAR_OLC *data, int choice, 
		      const char *arg) {
  switch(choice) {
  case MEDIT_PARENTS:
    charOLCSetParents(data, arg);
    return TRUE;
  case MEDIT_NAME:
    charSetName(charOLCGetChar(data), arg);
    return TRUE;
  case MEDIT_MULTI_NAME:
    charSetMultiName(charOLCGetChar(data), arg);
    return TRUE;
  case MEDIT_KEYWORDS:
    charSetKeywords(charOLCGetChar(data), arg);
    return TRUE;
  case MEDIT_RDESC:
    charSetRdesc(charOLCGetChar(data), arg);
    return TRUE;
  case MEDIT_MULTI_RDESC:
    charSetMultiRdesc(charOLCGetChar(data), arg);
    return TRUE;
  case MEDIT_RACE:
    if(!isRace(arg))
      return FALSE;
    charSetRace(charOLCGetChar(data), arg);
    return TRUE;
  case MEDIT_SEX: {
    int val = atoi(arg);
    if(!isdigit(*arg) || val < 0 || val >= NUM_SEXES)
      return FALSE;
    charSetSex(charOLCGetChar(data), val);
    return TRUE;
  }
  default: return FALSE;
  }
}

void save_mob_olc(CHAR_OLC *data) {
  PROTO_DATA *old_proto = worldGetType(gameworld, "mproto",charOLCGetKey(data));
  PROTO_DATA *new_proto = charOLCToProto(data);
  if(old_proto == NULL)
    worldPutType(gameworld, "mproto", protoGetKey(new_proto), new_proto);
  else {
    protoCopyTo(new_proto, old_proto);
    deleteProto(new_proto);
  }

  worldSaveType(gameworld, "mproto", charOLCGetKey(data));
}



//*****************************************************************************
// commands
//*****************************************************************************
COMMAND(cmd_medit) {
  ZONE_DATA    *zone = NULL;
  PROTO_DATA  *proto = NULL;

  // we need a key
  if(!arg || !*arg)
    send_to_char(ch, "What is the name of the mob you want to edit?\r\n");
  else {
    char locale[SMALL_BUFFER];
    char   name[SMALL_BUFFER];
    if(!parse_worldkey_relative(ch, arg, name, locale))
      send_to_char(ch, "Which mob are you trying to edit?\r\n");
    // make sure we can edit the zone
    else if((zone = worldGetZone(gameworld, locale)) == NULL)
      send_to_char(ch, "No such zone exists.\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit that zone.\r\n");
    else {
      // try to make our OLC datastructure
      CHAR_OLC *data = NULL;

      // try to pull up the prototype
      proto = worldGetType(gameworld, "mproto", get_fullkey(name, locale));

      // if we already have proto data, try to parse a char olc out of it
      if(proto != NULL) {
	// check to make sure the prototype was made by medit
	char line[SMALL_BUFFER];
	strcpyto(line, protoGetScript(proto), '\n');
	if(strcmp(line, "### The following mproto was generated by medit")!=0){
	  send_to_char(ch, "This mob was not generated by medit and potential "
		      "formatting problems prevent medit from being used. To "
		       "edit, mpedit must be used\r\n");
	  return;
	}
	else
	  data = charOLCFromProto(proto);
      }
      // otherwise, make a new char olc and assign its key
      else {
	data = newCharOLC();
	charOLCSetKey(data, get_fullkey(name, locale));
	charOLCSetAbstract(data, TRUE);

	CHAR_DATA *mob = charOLCGetChar(data);
	charSetName(mob,       "an unfinished mobile");
	charSetMultiName(mob,  "%d unfinished mobiles");
	charSetKeywords(mob,   "mobile, unfinished");
	charSetRdesc(mob,      "an unfinished mobile is standing here.");
	charSetMultiRdesc(mob, "A group of %d mobiles are here, "
			       "looking unfinished.");
	charSetDesc(mob,       "It looks unfinished.\r\n");
      }
      
      do_olc(charGetSocket(ch), medit_menu, medit_chooser, medit_parser, 
	     NULL, NULL, deleteCharOLC, save_mob_olc, data);
    }
  }
}
