//*****************************************************************************
//
// olc.c
//
// Contains all of the functions neccessary for online editing.
//
//*****************************************************************************

#include <mud.h>
#include <socket.h>
#include <object.h>
#include <character.h>
#include <world.h>
#include <zone.h>
#include <room.h>
#include <room_reset.h>
#include <exit.h>
#include <extra_descs.h>
#include <dialog.h>

#include "olc.h"

// optional modules
#ifdef MODULE_SCRIPTS
#include "../scripts/script.h"
#include "../scripts/script_set.h"
#endif


/* sucks to have to do this, but I'm declaring some
   external functions to avoid having to make some
   more header files. */
void dedit_loop   (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void dedit_menu   (SOCKET_DATA *sock, OLC_DATA *olc);
void respedit_loop(SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void respedit_menu(SOCKET_DATA *sock, OLC_DATA *olc);
void edsedit_loop (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void edsedit_menu (SOCKET_DATA *sock, OLC_DATA *olc);
void ededit_loop  (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void ededit_menu  (SOCKET_DATA *sock, OLC_DATA *olc);
void redit_loop   (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void redit_menu   (SOCKET_DATA *sock, OLC_DATA *olc);
void medit_loop   (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void medit_menu   (SOCKET_DATA *sock, OLC_DATA *olc);
void exedit_loop  (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void exedit_menu  (SOCKET_DATA *sock, OLC_DATA *olc);
void zedit_loop   (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void zedit_menu   (SOCKET_DATA *sock, OLC_DATA *olc);
void wedit_loop   (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void wedit_menu   (SOCKET_DATA *sock, OLC_DATA *olc);
void oedit_loop   (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void oedit_menu   (SOCKET_DATA *sock, OLC_DATA *olc);
void resedit_loop (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void resedit_menu (SOCKET_DATA *sock, OLC_DATA *olc);
#ifdef MODULE_SCRIPTS
void scedit_loop  (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void scedit_menu  (SOCKET_DATA *sock, OLC_DATA *olc);
void ssedit_loop  (SOCKET_DATA *sock, OLC_DATA *olc, char *arg);
void ssedit_menu  (SOCKET_DATA *sock, OLC_DATA *olc);
#endif

struct olc_data {
  bool complete;
  bool save;

  int state;
  int substate;

  char *arg;
  void *data;

  OLC_DATA *next;
};

void init_olc() {
  // add all of our commands
  add_cmd("dedit",      NULL, cmd_dedit,    0, POS_SITTING,  POS_FLYING,
	  LEVEL_BUILDER, TRUE );
  add_cmd("medit",      NULL, cmd_medit,    0, POS_SITTING,  POS_FLYING,
	  LEVEL_BUILDER, TRUE );
  add_cmd("oedit",      NULL, cmd_oedit,    0, POS_SITTING,  POS_FLYING,
	  LEVEL_BUILDER, TRUE );
  add_cmd("redit",      NULL, cmd_redit,    0, POS_SITTING,  POS_FLYING,
	  LEVEL_BUILDER, TRUE );
  add_cmd("wedit",      NULL, cmd_wedit,    0, POS_SITTING,  POS_FLYING,
	  LEVEL_BUILDER, TRUE );
  add_cmd("zedit",      NULL, cmd_zedit,    0, POS_SITTING,  POS_FLYING,
	  LEVEL_BUILDER, TRUE );
#ifdef MODULE_SCRIPTS
  add_cmd("scedit",     NULL, cmd_scedit,   0, POS_SITTING, POS_FLYING,
	  LEVEL_SCRIPTER, TRUE );
#endif
}


OLC_DATA *newOLC(int state, int substate, void *data, const char *arg) {
  OLC_DATA *olc = malloc(sizeof(OLC_DATA));
  olc->data     = data;
  olc->arg      = strdup(arg ? arg : "");
  olc->complete = FALSE;
  olc->save     = FALSE;
  olc->state    = state;
  olc->substate = substate;
  olc->next     = NULL;
  return olc;
}

void deleteOLC(OLC_DATA *olc) {
  if(olc->next) 
    deleteOLC(olc->next);

  if(olc->data) {
    switch(olc->state) {
    case OLC_DEDIT:   deleteDialog(olc->data);    break;
    case OLC_RESPEDIT:deleteResponse(olc->data);  break;
    case OLC_EDSEDIT: deleteEdescSet(olc->data);  break;
    case OLC_EDEDIT:  deleteEdesc(olc->data);     break;
    case OLC_REDIT:   deleteRoom(olc->data);      break;
    case OLC_OEDIT:   deleteObj(olc->data);       break;
    case OLC_MEDIT:   deleteChar(olc->data);    break;
    case OLC_ZEDIT:   deleteZone(olc->data);      break;
    case OLC_EXEDIT:  deleteExit(olc->data);      break;
    case OLC_WEDIT:   deleteWorld(olc->data);     break;
    case OLC_RESEDIT: deleteReset(olc->data);     break;
#ifdef MODULE_SCRIPTS
    case OLC_SCEDIT:  deleteScript(olc->data);    break;
    case OLC_SSEDIT:  deleteScriptSet(olc->data); break;
#endif
    }
  }

  if(olc->arg) 
    free(olc->arg);
  free(olc);
}

int       olcGetState   (OLC_DATA *olc) {
  return olc->state;
}

int       olcGetSubstate(OLC_DATA *olc) {
  return olc->substate;
}

void      olcSetState   (OLC_DATA *olc, int state) {
  olc->state = state;
}

void      olcSetSubstate(OLC_DATA *olc, int substate) {
  olc->substate = substate;
}

void  *olcGetData    (OLC_DATA *olc) {
  return olc->data;
}

const char *olcGetArgument(OLC_DATA *olc) {
  return olc->arg;
}

void olcSetArgument(OLC_DATA *olc, char *arg) {
  if(olc->arg) free(olc->arg);
  olc->arg = (arg ? strdup(arg) : NULL);
}

void olcSetData(OLC_DATA *olc, void  *data) {
  if(olc->data) {
    switch(olc->state) {
    case OLC_DEDIT:    deleteDialog(olc->data);      break;
    case OLC_RESPEDIT: deleteResponse(olc->data);    break;
    case OLC_EDSEDIT:  deleteEdescSet(olc->data);    break;
    case OLC_EDEDIT:   deleteEdesc(olc->data);       break;
    case OLC_MEDIT:    deleteChar(olc->data);      break;
    case OLC_REDIT:    deleteRoom(olc->data);        break;
    case OLC_OEDIT:    deleteObj(olc->data);         break;
    case OLC_ZEDIT:    deleteZone(olc->data);        break;
    case OLC_EXEDIT:   deleteExit(olc->data);        break;
    case OLC_WEDIT:    deleteWorld(olc->data);       break;
    case OLC_RESEDIT:  deleteReset(olc->data);       break;
#ifdef MODULE_SCRIPTS
    case OLC_SCEDIT:   deleteScript(olc->data);      break;
    case OLC_SSEDIT:   deleteScriptSet(olc->data);   break;
#endif
    }
  }
  olc->data = data;
}

bool        isOLCComplete (OLC_DATA *olc) {
  return olc->complete;
}

bool        olcGetSave    (OLC_DATA *olc) {
  return olc->save;
}

OLC_DATA   *olcGetNext    (OLC_DATA *olc) {
  return olc->next;
}

void        olcSetNext    (OLC_DATA *olc, OLC_DATA *next) {
  if(olc->next) deleteOLC(olc->next);
  olc->next = next;
}

void        olcSetComplete(OLC_DATA *olc, bool complete) {
  olc->complete = complete;
}

void        olcSetSave    (OLC_DATA *olc, bool save) {
  olc->save = save;
}


//
// Save all of the changes made in the OLC to the game world
//
void save_olc(OLC_DATA *olc) {
  int state = olcGetState(olc);
  switch(state) {
  case OLC_MEDIT: {
    CHAR_DATA *mob = (CHAR_DATA *)olcGetData(olc);
    CHAR_DATA *old  = worldGetMob(gameworld, charGetVnum(mob));
    // if the mob already exists, just copy over the changes
    if(old) charCopyTo(mob, old);
    // otherwise, put a new copy of the mob into the world. We can't
    // put this copy in, because it will be deleted when the OLC is
    // deleted.
    else    worldPutMob(gameworld, charCopy(mob));
    break;
  }

  case OLC_OEDIT: {
    OBJ_DATA *obj = (OBJ_DATA *)olcGetData(olc);
    OBJ_DATA *old  = worldGetObj(gameworld, objGetVnum(obj));
    // if the obj already exists, just copy over the changes
    if(old) objCopyTo(obj, old);
    // otherwise, put a new copy of the obj into the world. We can't
    // put this copy in, because it will be deleted when the OLC is
    // deleted.
    else    worldPutObj(gameworld, objCopy(obj));
    break;
  }

  case OLC_ZEDIT: {
    ZONE_DATA *zone = (ZONE_DATA *)olcGetData(olc);
    ZONE_DATA *old  = worldGetZone(gameworld, zoneGetVnum(zone));
    // if the obj already exists, just copy over the changes
    if(old) zoneCopyTo(zone, old);
    // otherwise, put a new copy of the obj into the world. We can't
    // put this copy in, because it will be deleted when the OLC is
    // deleted.
    else    worldPutZone(gameworld, zoneCopy(zone));
    break;
  }

  case OLC_REDIT: {
    ROOM_DATA *room = (ROOM_DATA *)olcGetData(olc);
    ROOM_DATA *old  = worldGetRoom(gameworld, roomGetVnum(room));
    // if the room already exists, just copy over the changes
    if(old) roomCopyTo(room, old);
    // otherwise, put a new copy of the room into the world. We can't
    // put this copy in, because it will be deleted when the OLC is
    // deleted.
    else    worldPutRoom(gameworld, roomCopy(room));
    break;
  }

  case OLC_DEDIT: {
    DIALOG_DATA *dialog = (DIALOG_DATA *)olcGetData(olc);
    DIALOG_DATA *old  = worldGetDialog(gameworld, dialogGetVnum(dialog));
    // if the dialog already exists, just copy over the changes
    if(old) dialogCopyTo(dialog, old);
    // otherwise, put a new copy of the dialog into the world. We can't
    // put this copy in, because it will be deleted when the OLC is
    // deleted.
    else    worldPutDialog(gameworld, dialogCopy(dialog));
    break;
  }

#ifdef MODULE_SCRIPTS
  case OLC_SCEDIT: {
    SCRIPT_DATA *script = (SCRIPT_DATA *)olcGetData(olc);
    SCRIPT_DATA *old  = worldGetScript(gameworld, scriptGetVnum(script));
    // if the script already exists, just copy over the changes
    if(old) scriptCopyTo(script, old);
    // otherwise, put a new copy of the script into the world. We can't
    // put this copy in, because it will be deleted when the OLC is
    // deleted.
    else    worldPutScript(gameworld, scriptCopy(script));
    break;
  }
#endif


  case OLC_RESEDIT:
  case OLC_RESPEDIT:
  case OLC_SSEDIT:
  case OLC_EDEDIT:
  case OLC_EDSEDIT:
  case OLC_EXEDIT: {
    char buf[SMALL_BUFFER];
    // print the type of error that is being made
    sprintf(buf, "%s",
	    (state == OLC_SSEDIT ? "script set" :
	     (state == OLC_EDEDIT ? "extra description" :
	      (state == OLC_EDSEDIT ? "extra desc set" :
	       (state == OLC_EXEDIT ? "exit" : 
		(state == OLC_RESPEDIT ? "dialog response":
		 (state == OLC_RESEDIT ? "reset data" : "unknown type")))))));

    log_string("ERROR: %s edit was bottom-level OLC edit. Cannot save "
	       "%s without a corresponding datastructure!", buf, buf);
    break;
  }

  default:
    log_string("ERROR: tried to save OLC, but type was unknown.");
    break;
  }
}


//
// Find the deepest OLC project that hasn't been finished
//
OLC_DATA *olcGetCurrent(OLC_DATA *olc) {
  if(isOLCComplete(olc))
    return NULL;
  if(olcGetNext(olc) && !isOLCComplete(olcGetNext(olc)))
    return olcGetCurrent(olcGetNext(olc));
  else
    return olc;
}


//*****************************************************************************
//
// The main loops and entrypoints for OLC
//
//*****************************************************************************
void olc_loop(SOCKET_DATA *sock, char *arg) {
  OLC_DATA *olc = socketGetOLC(sock);

  if(olc == NULL) {
    socketSetState(sock, STATE_PLAYING);
    log_string("ERROR: socket in OLC with no OLC data structure.");
    return;
  }

  olc = olcGetCurrent(socketGetOLC(sock));

  // everything has been completed... this should never happen
  if(olc == NULL) {
    socketSetState(sock, STATE_PLAYING);
    log_string("ERROR: entered OLC, but had no olc work left.");
    return;
  }

  switch(olcGetState(olc)) {
  case OLC_MEDIT:
    medit_loop(sock, olc, arg);
    break;
  case OLC_OEDIT:
    oedit_loop(sock, olc, arg);
    break;
  case OLC_REDIT:
    redit_loop(sock, olc, arg);
    break;
  case OLC_EXEDIT:
    exedit_loop(sock, olc, arg);
    break;
  case OLC_EDSEDIT:
    edsedit_loop(sock, olc, arg);
    break;
  case OLC_EDEDIT:
    ededit_loop(sock, olc, arg);
    break;
  case OLC_DEDIT:
    dedit_loop(sock, olc, arg);
    break;
  case OLC_RESPEDIT:
    respedit_loop(sock, olc, arg);
    break;
  case OLC_RESEDIT:
    resedit_loop(sock, olc, arg);
    break;
  case OLC_WEDIT:
    //    wedit_loop(sock, olc, arg);
    break;
  case OLC_ZEDIT:
    zedit_loop(sock, olc, arg);
    break;
#ifdef MODULE_SCRIPTS
  case OLC_SCEDIT:
    scedit_loop(sock, olc, arg);
    break;
  case OLC_SSEDIT:
    ssedit_loop(sock, olc, arg);
    break;
#endif
  default:
    log_string("ERROR: socket in OLC without an appropriate substate.");
    socketSetOLC(sock, NULL); // this deletes olc as well
    socketSetState(sock, STATE_PLAYING);
    return;
  }

  // clear up all of our completed OLC projects
  if(isOLCComplete(olc)) {
    // it's the first OLC project. We have to save to world
    if(olc == socketGetOLC(sock)) {
      if(olcGetSave(socketGetOLC(sock)))
	save_olc(socketGetOLC(sock));
      socketSetOLC(sock, NULL); // deletes olc automatically
      socketSetState(sock, STATE_PLAYING);
      
      // do we save changes to disk?
      if(OLC_AUTOSAVE) {
	switch(olcGetState(olc)) {
	case OLC_OEDIT:
	case OLC_MEDIT:
	case OLC_REDIT:
	case OLC_ZEDIT:
	case OLC_WEDIT:
	case OLC_DEDIT:
#ifdef MODULE_SCRIPTS
	case OLC_SCEDIT:
#endif
	  worldSave(gameworld, WORLD_PATH);
	  break;
	default:
	  log_string("ERROR: Tried to auto-save OLC changes to disk, but state %d was not recognized.", olcGetState(olc));
	  break;
	}
      }
    }

    // otherwise, save to the previous OLC project. It is expecting
    // to be completed, so we can just send an empty buffer for input
    else {
      char buf[1] = "\0";
      olc_loop(sock, buf);
    }
  }

  // we added another subproject ... show the menu for it
  else if(olcGetCurrent(olc) != olc)
    olc_menu(sock);
}


void olc_menu(SOCKET_DATA *sock) {
  OLC_DATA *olc = socketGetOLC(sock);

  // we have no OLC, or we've finished editing
  if(olc == NULL || isOLCComplete(olc))
    return;

  // go to the deepest olc project we have not completed working on
  olc = olcGetCurrent(olc);

  switch(olcGetState(olc)) {
  case OLC_MEDIT:
    medit_menu(sock, olc);
    break;
  case OLC_OEDIT:
    oedit_menu(sock, olc);
    break;
  case OLC_REDIT:
    redit_menu(sock, olc);
    break;
  case OLC_EXEDIT:
    exedit_menu(sock, olc);
    break;
  case OLC_EDSEDIT:
    edsedit_menu(sock, olc);
    break;
  case OLC_EDEDIT:
    ededit_menu(sock, olc);
    break;
  case OLC_DEDIT:
    dedit_menu(sock, olc);
    break;
  case OLC_RESPEDIT:
    respedit_menu(sock, olc);
    break;
  case OLC_RESEDIT:
    resedit_menu(sock, olc);
    break;
  case OLC_WEDIT:
    //    wedit_menu(sock, olc);
    break;
  case OLC_ZEDIT:
    zedit_menu(sock, olc);
    break;
#ifdef MODULE_SCRIPTS
  case OLC_SCEDIT:
    scedit_menu(sock, olc);
    break;
  case OLC_SSEDIT:
    ssedit_menu(sock, olc);
    break;
#endif
  default:
      break;
  }
}


COMMAND(cmd_redit) {
  ZONE_DATA *zone;
  ROOM_DATA *room;
  ROOM_DATA *tgt;
  room_vnum vnum;

  // if no argument is supplied, default to the current room
  if(!arg || !*arg)
    vnum = roomGetVnum(charGetRoom(ch));
  else
    vnum = atoi(arg);


  // make sure there is a corresponding zone ...
  if((zone = worldZoneBounding(gameworld, vnum)) == NULL) {
    send_to_char(ch, "No zone exists that contains the given vnum.\r\n");
    return;
  }
  if(!canEditZone(zone, ch)) {
    send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    return;
  }

  // find the room
  tgt = worldGetRoom(gameworld, vnum);

  // make our room
  if(tgt == NULL) {
    room = newRoom();
    roomSetVnum(room, vnum);
    roomSetName(room, "An Unfinished Room");
    roomSetDesc(room, "   You are in an unfinished room.\r\n");
  }
  else
    room = roomCopy(tgt);
  OLC_DATA *olc = newOLC(OLC_REDIT, REDIT_MAIN, (void *)room, NULL);
  socketSetOLC(charGetSocket(ch), olc);
  socketSetState(charGetSocket(ch), STATE_OLC);
  olc_menu(charGetSocket(ch));
}


COMMAND(cmd_medit) {
  ZONE_DATA *zone;
  CHAR_DATA *mob;
  CHAR_DATA *tgt;
  mob_vnum vnum;

  // if no argument is supplied, default to the current mob
  if(!arg || !*arg) {
    send_to_char(ch, "Please supply the vnum of a mob you wish to edit.\r\n");
    return;
  }
  else
    vnum = atoi(arg);


  // make sure there is a corresponding zone ...
  if((zone = worldZoneBounding(gameworld, vnum)) == NULL) {
    send_to_char(ch, "No zone exists that contains the given vnum.\r\n");
    return;
  }
  else if(!canEditZone(zone, ch)) {
    send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    return;
  }

  // find the mob
  tgt = worldGetMob(gameworld, vnum);

  // make our mob
  if(tgt == NULL) {
    mob = newMobile(vnum);
    charSetVnum(mob, vnum);
    charSetName   (mob, "an unfinished mobile");
    charSetKeywords(mob, "mobile, unfinshed");
    charSetRdesc  (mob, "an unfinished mobile is standing here.");
    charSetDesc   (mob, "it looks unfinished.\r\n");
    charSetMultiName(mob, "%d unfinished mobiles");
    charSetMultiRdesc(mob, "A group of %d mobiles are here, looking unfinished.");
  }
  else
    mob = charCopy(tgt);
  OLC_DATA *olc = newOLC(OLC_MEDIT, MEDIT_MAIN, (void *)mob, NULL);
  socketSetOLC(charGetSocket(ch), olc);
  socketSetState(charGetSocket(ch), STATE_OLC);
  olc_menu(charGetSocket(ch));
}


COMMAND(cmd_oedit) {
  ZONE_DATA *zone;
  OBJ_DATA *obj;
  OBJ_DATA *tgt;
  obj_vnum vnum;

  // if no argument is supplied, default to the current obj
  if(!arg || !*arg) {
    send_to_char(ch, "Please supply the vnum of a obj you wish to edit.\r\n");
    return;
  }
  else
    vnum = atoi(arg);


  // make sure there is a corresponding zone ...
  if((zone = worldZoneBounding(gameworld, vnum)) == NULL) {
    send_to_char(ch, "No zone exists that contains the given vnum.\r\n");
    return;
  }
  else if(!canEditZone(zone, ch)) {
    send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    return;
  }

  // find the obj
  tgt = worldGetObj(gameworld, vnum);

  // make our obj
  if(tgt == NULL) {
    obj = newObj(vnum);
    objSetVnum(obj, vnum);
    objSetName      (obj, "an unfinished object");
    objSetKeywords  (obj, "object, unfinshed");
    objSetRdesc     (obj, "an unfinished object is lying here.");
    objSetDesc      (obj, "it looks unfinished.\r\n");
    objSetMultiName (obj, "a group of %d unfinished objects");
    objSetMultiRdesc(obj, "%d objects lay here, all unfinished.");
  }
  else
    obj = objCopy(tgt);
  OLC_DATA *olc = newOLC(OLC_OEDIT, OEDIT_MAIN, (void *)obj, NULL);
  socketSetOLC(charGetSocket(ch), olc);
  socketSetState(charGetSocket(ch), STATE_OLC);
  olc_menu(charGetSocket(ch));
}


COMMAND(cmd_zedit) {
  /*
  if(!arg || !*arg) {
    send_to_char(ch, "Edit we must, but edit what?\r\n");
    return;
  }
  */
  // we want to create a new zone?
  if(!strncasecmp(arg, "new ", 4)) {
    char new[20];
    zone_vnum vnum = 0;
    room_vnum min = 0, max = 0;

    // scan for the parameters
    sscanf(arg, "%s %d %d %d", new, &vnum, &min, &max);

    if(worldGetZone(gameworld, vnum))
      send_to_char(ch, "A zone already exists with that vnum.\r\n");
    else if(worldZoneBounding(gameworld, min) || worldZoneBounding(gameworld, max))
      send_to_char(ch, "There is already a zone bounding that vnum range.\r\n");
    else {
      ZONE_DATA *zone = newZone(vnum, min, max);
      char buf[MAX_BUFFER];
      sprintf(buf, "%s's zone", charGetName(ch));
      zoneSetName(zone, buf);
      sprintf(buf, "A new zone created by %s\r\n", charGetName(ch));
      zoneSetDescription(zone, buf);
      zoneSetEditors(zone, charGetName(ch));

      worldPutZone(gameworld, zone);
      send_to_char(ch, "You create a new zone (vnum %d).\r\n", vnum);

      // save the changes... this will get costly as our world gets bigger.
      // But that should be alright once we make zone saving a bit smarter
      worldSave(gameworld, WORLD_PATH);
    }
  }

  // we want to edit a preexisting zone
  else {
    ZONE_DATA *zone = NULL;
    zone_vnum vnum   = (!*arg ? 
			zoneGetVnum(worldZoneBounding(gameworld, roomGetVnum(charGetRoom(ch)))) : atoi(arg));
 
    // make sure there is a corresponding zone ...
    if((zone = worldGetZone(gameworld, vnum)) == NULL) {
      send_to_char(ch, 
		   "No such zone exists. To create a new one, use "
		   "zedit new <vnum> <min> <max>\r\n");
      return;
    }
    else if(!canEditZone(zone, ch)) {
      send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
      return;
    }

    OLC_DATA *olc = newOLC(OLC_ZEDIT, ZEDIT_MAIN, (void *)zoneCopy(zone), NULL);
    socketSetOLC(charGetSocket(ch), olc);
    socketSetState(charGetSocket(ch), STATE_OLC);
    olc_menu(charGetSocket(ch));
  }
}


COMMAND(cmd_dedit) {
  ZONE_DATA *zone;
  DIALOG_DATA *dialog;
  DIALOG_DATA *tgt;
  dialog_vnum vnum;

  // if no argument is supplied, default to the current dialog
  if(!arg || !*arg) {
    send_to_char(ch, "Please supply the vnum of a dialog you wish to edit.\r\n");
    return;
  }
  else
    vnum = atoi(arg);


  // make sure there is a corresponding zone ...
  if((zone = worldZoneBounding(gameworld, vnum)) == NULL) {
    send_to_char(ch, "No zone exists that contains the given vnum.\r\n");
    return;
  }
  else if(!canEditZone(zone, ch)) {
    send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    return;
  }

  // find the dialog
  tgt = worldGetDialog(gameworld, vnum);

  // make our dialog
  if(tgt == NULL) {
    dialog = newDialog();
    dialogSetVnum(dialog, vnum);
  }
  else
    dialog = dialogCopy(tgt);
  OLC_DATA *olc = newOLC(OLC_DEDIT, DEDIT_MAIN, (void *)dialog, NULL);
  socketSetOLC(charGetSocket(ch), olc);
  socketSetState(charGetSocket(ch), STATE_OLC);
  olc_menu(charGetSocket(ch));
}


COMMAND(cmd_wedit) {

}


#ifdef MODULE_SCRIPTS
COMMAND(cmd_scedit) {
  ZONE_DATA *zone;
  SCRIPT_DATA *script;
  SCRIPT_DATA *tgt;
  script_vnum vnum;

  // if no argument is supplied, default to the current script
  if(!arg || !*arg) {
    send_to_char(ch, "Please supply the vnum of a script you wish to edit.\r\n");
    return;
  }
  else
    vnum = atoi(arg);


  // make sure there is a corresponding zone ...
  if((zone = worldZoneBounding(gameworld, vnum)) == NULL) {
    send_to_char(ch, "No zone exists that contains the given vnum.\r\n");
    return;
  }
  else if(!canEditZone(zone, ch)) {
    send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    return;
  }

  // find the script
  tgt = worldGetScript(gameworld, vnum);

  // make our script
  if(tgt == NULL) {
    script = newScript();
    scriptSetVnum(script, vnum);
    scriptSetName(script, "An Unfinished Script");
    scriptSetCode(script, "# script code goes here\n"
		          "# make sure to comment it with pounds (#)\n");

  }
  else
    script = scriptCopy(tgt);
  OLC_DATA *olc = newOLC(OLC_SCEDIT, SCEDIT_MAIN, (void *)script, NULL);
  socketSetOLC(charGetSocket(ch), olc);
  socketSetState(charGetSocket(ch), STATE_OLC);
  olc_menu(charGetSocket(ch));
}
#endif
