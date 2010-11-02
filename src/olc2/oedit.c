//*****************************************************************************
//
// oedit.c
//
// When obj prototypes became python scripts, OLC for mobs had to be rethought.
// Ideally, all builders would have a basic grasp on python and thus would be
// able to write scripts. Ideally. Sadly, I don't think this can be expected
// out of most builders, and we still need some sort of non-scripting interface
// for editing objs. So here it is...
//
//*****************************************************************************
#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../races.h"
#include "../world.h"
#include "../object.h"
#include "../character.h"
#include "../extra_descs.h"
#include "../prototype.h"

#include "olc.h"
#include "olc_submenus.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../editor/editor.h"
#include "../scripts/scripts.h"
#include "../scripts/script_editor.h"
#include "../items/items.h"
#include "../items/iedit.h"



//*****************************************************************************
// obj olc structure, and functions
//*****************************************************************************
typedef struct {
  char          *key; // the key for our prototype
  char      *parents; // things we inherit from
  bool      abstract; // can we be laoded into the game?
  OBJ_DATA      *obj; // our object, which holds most of our variables
  BUFFER *extra_code; // any extra code that should go to our prototype
} OBJ_OLC;

OBJ_OLC *newObjOLC(void) {
  OBJ_OLC *data  = malloc(sizeof(OBJ_OLC));
  data->key       = strdup("");
  data->parents   = strdup("");
  data->abstract  = TRUE;
  data->obj       = newObj();
  objSetWeightRaw(data->obj, -1);
  data->extra_code = newBuffer(1);
  return data;
}

void deleteObjOLC(OBJ_OLC *data) {
  if(data->key)        free(data->key);
  if(data->parents)    free(data->parents);
  if(data->extra_code) deleteBuffer(data->extra_code);
  if(data->obj)        deleteObj(data->obj);
  free(data);
}

const char *objOLCGetKey(OBJ_OLC *data) {
  return data->key;
}

const char *objOLCGetParents(OBJ_OLC *data) {
  return data->parents;
}

bool objOLCGetAbstract(OBJ_OLC *data) {
  return data->abstract;
}

OBJ_DATA *objOLCGetObj(OBJ_OLC *data) {
  return data->obj;
}

BUFFER *objOLCGetExtraCode(OBJ_OLC *data) {
  return data->extra_code;
}

void objOLCSetKey(OBJ_OLC *data, const char *key) {
  if(data->key) free(data->key);
  data->key = strdup(key);
}

void objOLCSetParents(OBJ_OLC *data, const char *parents) {
  if(data->parents) free(data->parents);
  data->parents = strdup(parents);
}

void objOLCSetAbstract(OBJ_OLC *data, bool abstract) {
  data->abstract = abstract;
}


//
// takes in an obj prototype, and tries to generate an obj olc out of it. This
// function is messy and ugly and icky and yuck. But, alas, I cannot think of
// a better way to do this. Maybe next version...
OBJ_OLC *objOLCFromProto(PROTO_DATA *proto) {
  OBJ_OLC *data = newObjOLC();
  OBJ_DATA *obj = objOLCGetObj(data);
  objOLCSetKey(data, protoGetKey(proto));
  objOLCSetParents(data, protoGetParents(proto));
  objOLCSetAbstract(data, protoIsAbstract(proto));

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
      objSetName(obj, lptr);
    }
    else if(!strncmp(lptr, "me.mname", 8)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      objSetMultiName(obj, lptr);
    }
    else if(!strncmp(lptr, "me.rdesc", 8)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      objSetRdesc(obj, lptr);
    }
    else if(!strncmp(lptr, "me.mdesc", 8)) {
      while(*lptr != '\"') lptr++;
      lptr++;                      // kill the leading "
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      objSetMultiRdesc(obj, lptr);
    }
    else if(!strncmp(lptr, "me.desc",  7)) {
      // we have three "'s to skip by, because this lptr will take the form:
      // me.desc = me.desc + " " + "..."
      while(*lptr != '\"') lptr++; lptr++;
      while(*lptr != '\"') lptr++; lptr++;
      while(*lptr != '\"') lptr++; lptr++;
      lptr[strlen(lptr)-1] = '\0'; // kill the ending "
      objSetDesc(obj, lptr);
      // replace our \"s with "
      bufferReplace(objGetDescBuffer(obj), "\\\"", "\"", TRUE);
      bufferFormat(objGetDescBuffer(obj), SCREEN_WIDTH, PARA_INDENT);
    }
    else if(!strncmp(lptr, "me.keywords", 11)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[next_letter_in(lptr, '\"')] = '\0'; // kill the ending "
      objSetKeywords(obj, lptr);
    }
    else if(!strncmp(lptr, "me.bits", 7)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                  // kill the leading "
      lptr[next_letter_in(lptr, '\"')] = '\0'; // kill the ending "
      bitSet(objGetBits(obj), lptr);
    }
    else if(!strncmp(lptr, "me.weight", 9)) {
      while(*lptr != '\0' && !isdigit(*lptr)) lptr++;
      objSetWeightRaw(obj, atof(lptr));
    }
    else if(!strncmp(lptr, "me.edesc(", 9)) {
      while(*lptr != '\"') lptr++;
      lptr++;                                              // kill the leading "
      char *desc_start = lptr + next_letter_in(lptr, '\"') + 1;
      lptr[next_letter_in(lptr, '\"')] = '\0';             // kill the ending "
      while(*desc_start != '\"') desc_start++;
      desc_start++;                                        // kill start and end
      desc_start[next_letter_in(desc_start, '\"')] = '\0'; // "s for desc too
      edescSetPut(objGetEdescs(obj), newEdesc(lptr, desc_start));
    }
    // setting an item type
    else if(!strncmp(lptr, "me.settype(", 11)) {
      char type[SMALL_BUFFER];
      sscanf(lptr, "me.settype(\"%s", type);
      // kill our ending ")
      type[strlen(type)-2] = '\0';
      objSetType(obj, type);

      // parse out all of our type info
      void       *data = objGetTypeData(obj, type);
      BUFFER *type_buf = newBuffer(1);
      code = strcpyto(line, code, '\n');
      while(*line && strcmp(line, "### end type") != 0) {
	bprintf(type_buf, "%s\n", line);
	code = strcpyto(line, code, '\n');
      }

      // parse out our type info
      item_from_proto_func(type)(data, type_buf);

      // garbage collection
      deleteBuffer(type_buf);
    }
    else if(!strncmp(lptr, "me.attach(\"", 11)) {
      char trigname[SMALL_BUFFER];
      sscanf(lptr, "me.attach(\"%s", trigname);
      // kill our ending ")
      trigname[strlen(trigname)-2] = '\0';
      triggerListAdd(objGetTriggers(obj), trigname);
    }
    else if(!strcmp(lptr, "### begin extra code")) {
      code = strcpyto(line, code, '\n');
      while(strcmp(line, "### end extra code") != 0) {
	bprintf(objOLCGetExtraCode(data), "%s\n", line);
	if(!*code) break;
	code = strcpyto(line, code, '\n');
      }
    }
  } while(*code != '\0');

  return data;
}


//
// takes in an obj olc and tries to generate a prototype out of it
PROTO_DATA *objOLCToProto(OBJ_OLC *data) {
  PROTO_DATA *proto = newProto();
  OBJ_DATA     *obj = objOLCGetObj(data);
  BUFFER       *buf = protoGetScriptBuffer(proto);
  protoSetKey(proto, objOLCGetKey(data));
  protoSetParents(proto, objOLCGetParents(data));
  protoSetAbstract(proto, objOLCGetAbstract(data));

  bprintf(buf, "### The following oproto was generated by oedit\n");
  bprintf(buf, "### If you edit this script, adhere to the stylistic\n"
	       "### conventions laid out by oedit, or delete the top line\n");

  bprintf(buf, "\n### keywords, short descs, room descs, and look descs\n");
  if(*objGetKeywords(obj))
    bprintf(buf, "me.keywords = \"%s\"  + \", \" + me.keywords\n", 
	    objGetKeywords(obj));
  if(*objGetName(obj))
    bprintf(buf, "me.name     = \"%s\"\n", objGetName(obj));
  if(*objGetMultiName(obj))
    bprintf(buf, "me.mname    = \"%s\"\n", objGetMultiName(obj));
  if(*objGetRdesc(obj))
    bprintf(buf, "me.rdesc    = \"%s\"\n", objGetRdesc(obj));
  if(*objGetMultiRdesc(obj))
    bprintf(buf, "me.mdesc    = \"%s\"\n", objGetMultiRdesc(obj));
  if(*objGetDesc(obj)) {
    BUFFER *desc_copy = bufferCopy(objGetDescBuffer(obj));
    bufferReplace(desc_copy, "\n", " ", TRUE);
    bufferReplace(desc_copy, "\r", "",  TRUE);
    bufferReplace(desc_copy, "\"", "\\\"", TRUE);
    bprintf(buf, "me.desc     = me.desc + \" \" + \"%s\"\n", 
	    bufferString(desc_copy));
    deleteBuffer(desc_copy);
  }

  // extra descriptions
  if(listSize(edescSetGetList(objGetEdescs(obj))) > 0) {
    bprintf(buf, "\n### extra descriptions\n");
    LIST_ITERATOR *edesc_i= newListIterator(edescSetGetList(objGetEdescs(obj)));
    EDESC_DATA      *edesc= NULL;
    ITERATE_LIST(edesc, edesc_i) {
      BUFFER *desc_copy = bufferCopy(edescGetDescBuffer(edesc));
      bufferReplace(desc_copy, "\n", " ", TRUE);
      bufferReplace(desc_copy, "\r", "",  TRUE);
      bprintf(buf, "me.edesc(\"%s\", \"%s\")\n", 
	      edescGetKeywords(edesc), bufferString(desc_copy));
      deleteBuffer(desc_copy);
    } deleteListIterator(edesc_i);
  }

  if(*bitvectorGetBits(objGetBits(obj))) {
    bprintf(buf, "\n### object bits\n");
    bprintf(buf, "me.bits     = \"%s\" + \", \" + me.bits\n", 
	    bitvectorGetBits(objGetBits(obj)));
  }

  if(objGetWeight(obj) > 0) {
    bprintf(buf, "\n### numeric values\n");
    bprintf(buf, "me.weight   = %1.3lf\n", objGetWeightRaw(obj));
  }

  // item types
  LIST      *item_types = itemTypeList();
  LIST_ITERATOR *type_i = newListIterator(item_types);
  char            *type = NULL;
  ITERATE_LIST(type, type_i) {
    if(objIsType(obj, type)) {
      void *data = objGetTypeData(obj, type);
      bprintf(buf, "\n### set type: %s\n", type);
      bprintf(buf, "me.settype(\"%s\")\n", type);
      item_to_proto_func(type)(data, buf);      
      bprintf(buf, "### end type\n");
    }
  } deleteListIterator(type_i);
  deleteListWith(item_types, free);

  if(listSize(objGetTriggers(obj)) > 0) {
    bprintf(buf, "\n### object triggers\n");
    LIST_ITERATOR *trig_i = newListIterator(objGetTriggers(obj));
    char            *trig = NULL;
    ITERATE_LIST(trig, trig_i) {
      bprintf(buf, "me.attach(\"%s\")\n", trig);
    } deleteListIterator(trig_i);
  }

  if(bufferLength(objOLCGetExtraCode(data)) > 0) {
    bprintf(buf, "\n### begin extra code\n");
    bprintf(buf, "%s", bufferString(objOLCGetExtraCode(data)));
    bprintf(buf, "### end extra code\n");
  }
  return proto;
}



//*****************************************************************************
// object editing
//*****************************************************************************
#define OEDIT_PARENTS       1
#define OEDIT_NAME          2
#define OEDIT_MULTI_NAME    3
#define OEDIT_KEYWORDS      4
#define OEDIT_RDESC         5
#define OEDIT_MULTI_RDESC   6
#define OEDIT_WEIGHT        7

void oedit_menu(SOCKET_DATA *sock, OBJ_OLC *data) {
  char weight_buf[SMALL_BUFFER];
  if(objGetWeightRaw(objOLCGetObj(data)) <= 0)
    sprintf(weight_buf, "leave unchanged");
  else
    sprintf(weight_buf, "%1.3lf", objGetWeightRaw(objOLCGetObj(data)));

  send_to_socket(sock,
		 "{g[{c%s{g]\r\n"
		 "{g1) Abstract: {c%s\r\n"
		 "{g2) Inherits from prototypes:\r\n"
		 "{c%s\r\n"
		 "{g3) Name:\r\n"
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
		 "{gW) Weight         : {c%s\r\n"
		 "{gI) Edit item types: {c%s\r\n"
		 "{gB) Edit bitvector : {c%s\r\n"
		 "{gT) Trigger menu\r\n"
		 "{gX) Extra Descriptions menu\r\n",
		 objOLCGetKey(data),
		 (objOLCGetAbstract(data) ? "yes" : "no"),
		 objOLCGetParents(data),
		 objGetName(objOLCGetObj(data)),
		 objGetMultiName(objOLCGetObj(data)),
		 objGetKeywords(objOLCGetObj(data)),
		 objGetRdesc(objOLCGetObj(data)),
		 objGetMultiRdesc(objOLCGetObj(data)),
		 objGetDesc(objOLCGetObj(data)),
		 weight_buf,
		 objGetTypes(objOLCGetObj(data)),
		 bitvectorGetBits(objGetBits(objOLCGetObj(data)))
		 );

  // only allow code editing for people with scripting priviledges
  send_to_socket(sock, "{gC) Extra code%s\r\n", 
		 ((!socketGetChar(sock) ||  
		   !bitIsOneSet(charGetUserGroups(socketGetChar(sock)),
				"scripter")) ? "    {y({cuneditable{y){g":""));
  script_display(sock, bufferString(objOLCGetExtraCode(data)), FALSE);
}

int  oedit_chooser(SOCKET_DATA *sock, OBJ_OLC *data, const char *option) {
  switch(toupper(*option)) {
  case '1':
    objOLCSetAbstract(data, (objOLCGetAbstract(data) + 1) % 2);
    return MENU_NOCHOICE;
  case '2':
    text_to_buffer(sock,"Enter comma-separated list of objs to inherit from: ");
    return OEDIT_PARENTS;
  case '3':
    text_to_buffer(sock, "Enter name: ");
    return OEDIT_NAME;
  case '4':
    text_to_buffer(sock, "Enter name for multiple occurences: ");
    return OEDIT_MULTI_NAME;
  case '5':
    text_to_buffer(sock, "Enter keywords: ");
    return OEDIT_KEYWORDS;
  case '6':
    text_to_buffer(sock, "Enter room description: ");
    return OEDIT_RDESC;
  case '7':
    text_to_buffer(sock, "Enter room description for multiple occurences: ");
    return OEDIT_MULTI_RDESC;
  case '8':
    text_to_buffer(sock, "Enter description\r\n");
    socketStartEditor(sock, text_editor, objGetDescBuffer(objOLCGetObj(data)));
    return MENU_NOCHOICE;
  case 'W':
    text_to_buffer(sock, "Enter new weight: ");
    return OEDIT_WEIGHT;
  case 'X':
    do_olc(sock, edesc_set_menu, edesc_set_chooser, edesc_set_parser, NULL,NULL,
	   NULL, NULL, objGetEdescs(objOLCGetObj(data)));
    return MENU_NOCHOICE;
  case 'I':
    do_olc(sock, iedit_menu, iedit_chooser, iedit_parser, NULL, NULL, NULL,
	   NULL, objOLCGetObj(data));
    return MENU_NOCHOICE;
  case 'B':
    do_olc(sock, bedit_menu, bedit_chooser, bedit_parser, NULL, NULL, NULL,
	   NULL, objGetBits(objOLCGetObj(data)));
    return MENU_NOCHOICE;
  case 'T':
    do_olc(sock, trigger_list_menu, trigger_list_chooser, trigger_list_parser,
	   NULL, NULL, NULL, NULL, objGetTriggers(objOLCGetObj(data)));
    return MENU_NOCHOICE;
  case 'C':
    // only scripters can edit extra code
    if(!socketGetChar(sock) || 
       !bitIsOneSet(charGetUserGroups(socketGetChar(sock)), "scripter"))
      return MENU_CHOICE_INVALID;
    text_to_buffer(sock, "Edit extra code\r\n");
    socketStartEditor(sock, script_editor, objOLCGetExtraCode(data));
    return MENU_NOCHOICE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool oedit_parser(SOCKET_DATA *sock, OBJ_OLC *data, int choice, 
		  const char *arg){
  switch(choice) {
  case OEDIT_PARENTS:
    objOLCSetParents(data, arg);
    return TRUE;    
  case OEDIT_NAME:
    objSetName(objOLCGetObj(data), arg);
    return TRUE;
  case OEDIT_MULTI_NAME:
    objSetMultiName(objOLCGetObj(data), arg);
    return TRUE;
  case OEDIT_KEYWORDS:
    objSetKeywords(objOLCGetObj(data), arg);
    return TRUE;
  case OEDIT_RDESC:
    objSetRdesc(objOLCGetObj(data), arg);
    return TRUE;
  case OEDIT_MULTI_RDESC:
    objSetMultiRdesc(objOLCGetObj(data), arg);
    return TRUE;
  case OEDIT_WEIGHT: {
    double val = atof(arg);
    if(val < 0) return FALSE;
    objSetWeightRaw(objOLCGetObj(data), val);
    return TRUE;
  }
  default: return FALSE;
  }
}

void save_obj_olc(OBJ_OLC *data) {
  PROTO_DATA *old_proto = worldGetType(gameworld, "oproto", objOLCGetKey(data));
  PROTO_DATA *new_proto = objOLCToProto(data);
  if(old_proto == NULL)
    worldPutType(gameworld, "oproto", protoGetKey(new_proto), new_proto);
  else {
    protoCopyTo(new_proto, old_proto);
    deleteProto(new_proto);
  }

  worldSaveType(gameworld, "oproto", objOLCGetKey(data));
}



//*****************************************************************************
// commands
//*****************************************************************************
COMMAND(cmd_oedit) {
  ZONE_DATA    *zone = NULL;
  PROTO_DATA  *proto = NULL;

  // we need a key
  if(!arg || !*arg)
    send_to_char(ch, "What is the name of the obj you want to edit?\r\n");
  else {
    char locale[SMALL_BUFFER];
    char   name[SMALL_BUFFER];
    if(!parse_worldkey_relative(ch, arg, name, locale))
      send_to_char(ch, "Which obj are you trying to edit?\r\n");
    // make sure we can edit the zone
    else if((zone = worldGetZone(gameworld, locale)) == NULL)
      send_to_char(ch, "No such zone exists.\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit that zone.\r\n");
    else {
      // try to make our OLC datastructure
      OBJ_OLC *data = NULL;

      // try to pull up the prototype
      proto = worldGetType(gameworld, "oproto", get_fullkey(name, locale));

      // if we already have proto data, try to parse an obj olc out of it
      if(proto != NULL) {
	// check to make sure the prototype was made by oedit
	char line[SMALL_BUFFER];
	strcpyto(line, protoGetScript(proto), '\n');
	if(strcmp(line, "### The following oproto was generated by oedit")!=0){
	  send_to_char(ch, "This obj was not generated by oedit and potential "
		      "formatting problems prevent oedit from being used. To "
		       "edit, opedit must be used\r\n");
	  return;
	}
	else
	  data = objOLCFromProto(proto);
      }
      // otherwise, make a new obj olc and assign its key
      else {
	data = newObjOLC();
	objOLCSetKey(data, get_fullkey(name, locale));
	objOLCSetAbstract(data, TRUE);

	OBJ_DATA *obj = objOLCGetObj(data);
	objSetName      (obj, "an unfinished object");
	objSetKeywords  (obj, "object, unfinshed");
	objSetRdesc     (obj, "an unfinished object is lying here.");
	objSetDesc      (obj, "it looks unfinished.\r\n");
	objSetMultiName (obj, "a group of %d unfinished objects");
	objSetMultiRdesc(obj, "%d objects lay here, all unfinished.");
      }
      
      do_olc(charGetSocket(ch), oedit_menu, oedit_chooser, oedit_parser, 
	     NULL, NULL, deleteObjOLC, save_obj_olc, data);
    }
  }
}
