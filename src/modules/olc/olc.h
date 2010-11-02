#ifndef __OLC_H
#define __OLC_H
//*****************************************************************************
//
// olc.h
//
// Contains all of the functions neccessary for online editing.
//
//*****************************************************************************

#define OLC_AUTOSAVE                (TRUE)

COMMAND(cmd_oedit);  // object editing
COMMAND(cmd_medit);  // mobile editing
COMMAND(cmd_redit);  // room editing
COMMAND(cmd_zedit);  // zone editing
COMMAND(cmd_wedit);  // world editing
COMMAND(cmd_dedit);  // dialog editing
#ifdef MODULE_SCRIPTS
COMMAND(cmd_scedit); // script editing
#endif

// initialize OLC. Must be called when the MUD starts up
void init_olc();

// enter the olc loop with the specified argument and olc data
void olc_loop(SOCKET_DATA *sock, char *arg);
void olc_menu(SOCKET_DATA *sock);


//
// writes the reset info to a buffer and returns it. 
// This is used by redit and resedit. If indent_first is false, the first
// line is not indented. This can be useful for displaying menus.
//
const char *write_reset(RESET_DATA *reset, int indent, bool indent_first);


//*****************************************************************************
//
// olc datastructure
//
// contains all of the information about what a socket is currently editing
//
//*****************************************************************************
#define OLC_MAIN                 0 // general OLC menu
#define OLC_REDIT                1 // room edit
#define OLC_ZEDIT                2 // zone edit
#define OLC_WEDIT                3 // world edit
#define OLC_EXEDIT               4 // exit edit
#define OLC_EDSEDIT              5 // edesc set edit
#define OLC_EDEDIT               6 // edesc edit
#define OLC_MEDIT                7 // mobile editing
#define OLC_OEDIT                8 // object editing
#define OLC_SCEDIT               9 // script editing
#define OLC_SSEDIT              10 // script set editing
#define OLC_DEDIT               11 // dialog editing
#define OLC_RESPEDIT            12 // dialog response editing
#define OLC_RESEDIT             13 // room reset editing

#define ZEDIT_MAIN               0
#define ZEDIT_CONFIRM_SAVE       1
#define ZEDIT_NAME               2
#define ZEDIT_EDITORS            3
#define ZEDIT_MIN                4
#define ZEDIT_MAX                5
#define ZEDIT_RESET              6

#define SSEDIT_MAIN              0
#define SSEDIT_CONFIRM_SAVE      1
#define SSEDIT_ADD               2
#define SSEDIT_REMOVE            4

#define SCEDIT_MAIN              0
#define SCEDIT_CONFIRM_SAVE      1
#define SCEDIT_NAME              2
#define SCEDIT_TYPE              3
#define SCEDIT_ARGS              4
#define SCEDIT_NARG              5

#define OEDIT_MAIN               0
#define OEDIT_CONFIRM_SAVE       1
#define OEDIT_NAME               2
#define OEDIT_KEYWORDS           3
#define OEDIT_RDESC              4
#define OEDIT_EDESCS             5
#define OEDIT_TYPE               6
#define OEDIT_SUBTYPE            7
#define OEDIT_SCRIPTS            8
#define OEDIT_BITS               9
#define OEDIT_MULTI_NAME        10
#define OEDIT_MULTI_RDESC       11
#define OEDIT_WEIGHT            12
#define OEDIT_CAPACITY          13

#define OEDIT_VAL_0             20
#define OEDIT_VAL_1             21
#define OEDIT_VAL_2             22
#define OEDIT_VAL_3             23

#define MEDIT_MAIN               0
#define MEDIT_CONFIRM_SAVE       1
#define MEDIT_NAME               2
#define MEDIT_KEYWORDS           3
#define MEDIT_RDESC              4
#define MEDIT_SEX                5
#define MEDIT_DIALOG             6
#define MEDIT_RACE               7
#define MEDIT_SCRIPTS            8
#define MEDIT_MULTI_NAME         9
#define MEDIT_MULTI_RDESC       10

#define REDIT_MAIN               0
#define REDIT_CONFIRM_SAVE       1
#define REDIT_NAME               2
#define REDIT_CHOOSE_EXIT        3
#define REDIT_EXIT               4
#define REDIT_TERRAIN            5
#define REDIT_EDESCS             6
#define REDIT_SCRIPTS            7
#define REDIT_RESET              8
#define REDIT_DELETE_RESET       9
#define REDIT_EDIT_RESET        10

#define RESEDIT_MAIN             0
#define RESEDIT_CONFIRM_SAVE     1
#define RESEDIT_TYPE             2
#define RESEDIT_TIMES            3
#define RESEDIT_CHANCE           4
#define RESEDIT_MAX              5
#define RESEDIT_ROOM_MAX         6
#define RESEDIT_ARG              7
#define RESEDIT_IN               8
#define RESEDIT_ON               9
#define RESEDIT_THEN            10
#define RESEDIT_EDIT_IN         11
#define RESEDIT_EDIT_ON         12
#define RESEDIT_EDIT_THEN       13
#define RESEDIT_DELETE_IN       14
#define RESEDIT_DELETE_ON       15
#define RESEDIT_DELETE_THEN     16

#define EDSEDIT_MAIN             0
#define EDSEDIT_CONFIRM_SAVE     1
#define EDSEDIT_ENTRY            2
#define EDSEDIT_DELETE           3

#define EDEDIT_MAIN              0
#define EDEDIT_CONFIRM_SAVE      1
#define EDEDIT_KEYWORDS          2

#define DEDIT_MAIN               0
#define DEDIT_CONFIRM_SAVE       1
#define DEDIT_NAME               2
#define DEDIT_GREET              3
#define DEDIT_ENTRY              4
#define DEDIT_DELETE             5

#define RESPEDIT_MAIN            0
#define RESPEDIT_CONFIRM_SAVE    1
#define RESPEDIT_KEYWORDS        2
#define RESPEDIT_MESSAGE         3

#define EXEDIT_MAIN              0
#define EXEDIT_CONFIRM_SAVE      1
#define EXEDIT_TO                2
#define EXEDIT_SPOT_DIFF         3
#define EXEDIT_PICK_DIFF         4
#define EXEDIT_KEY               5
#define EXEDIT_CLOSABLE          6
#define EXEDIT_LEAVE             7
#define EXEDIT_ENTER             8
#define EXEDIT_CONFIRM_DELETE    9
#define EXEDIT_NAME             10
#define EXEDIT_KEYWORDS         11


OLC_DATA   *newOLC        (int state, int substate, void *data,const char *arg);
void        deleteOLC     (OLC_DATA *olc);

bool        isOLCComplete (OLC_DATA *olc);
bool        olcGetSave    (OLC_DATA *olc);
int         olcGetState   (OLC_DATA *olc);
int         olcGetSubstate(OLC_DATA *olc);
void       *olcGetData    (OLC_DATA *olc);
const char *olcGetArgument(OLC_DATA *olc);
OLC_DATA   *olcGetNext    (OLC_DATA *olc);

void        olcSetSave    (OLC_DATA *olc, bool save);
void        olcSetData    (OLC_DATA *olc, void  *data);
void        olcSetArgument(OLC_DATA *olc, char *arg);
void        olcSetState   (OLC_DATA *olc, int state);
void        olcSetSubstate(OLC_DATA *olc, int substate);
void        olcSetNext    (OLC_DATA *olc, OLC_DATA *next);
void        olcSetComplete(OLC_DATA *olc, bool complete);

#endif // __OLC_H
