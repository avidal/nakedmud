#ifndef __CHARACTER_H
#define __CHARACTER_H
//*****************************************************************************
//
// character.h
//
// Basic implementation of the character datastructure (PCs and NPCs). Includes
// data for storing room, inventory, body, equipment, and other "essential"
// information. If you plan on adding any other information to characters, it
// is strongly suggested you do so through auxiliary data (see auxiliary.h).
//
// For a recap, IF YOU PLAN ON ADDING ANY OTHER INFORMATION TO CHARACTERS, IT
// IS STRONGLY SUGGESTED YOU DO SO THROUGH AUXILIARY DATA (see auxiliary.h).
//
//*****************************************************************************


//
// the difference between newChar and newMobile is that newMobile
// assigned a new UID whereas newChar does not.
//
CHAR_DATA   *newChar          ();
CHAR_DATA    *newMobile       ();
void         deleteChar       (CHAR_DATA *mob);

CHAR_DATA    *charRead        (STORAGE_SET *set);
STORAGE_SET *charStore        (CHAR_DATA *mob);

CHAR_DATA    *charCopy        (CHAR_DATA *mob);
void         charCopyTo       (CHAR_DATA *from, CHAR_DATA *to);


bool         charIsNPC        (CHAR_DATA *ch);
bool         charIsName       (CHAR_DATA *ch, const char *name);
void         putCharInventory (CHAR_DATA *ch, OBJ_DATA *obj);
void         charResetBody    (CHAR_DATA *ch);



//*****************************************************************************
//
// utility functions
//
//*****************************************************************************

/* buffers the output        */
void  text_to_char          (CHAR_DATA *dMob, const char *txt );
void  send_to_char          (CHAR_DATA *ch, const char *format, ...) 
__attribute__ ((format (printf, 2, 3)));
void  send_to_list          (LIST *list, const char *format, ...) 
__attribute__ ((format (printf, 2, 3)));


//*****************************************************************************
//
// set and get functions
//
//*****************************************************************************
SOCKET_DATA *charGetSocket    ( CHAR_DATA *ch);
ROOM_DATA   *charGetRoom      ( CHAR_DATA *ch);
const char  *charGetName      ( CHAR_DATA *ch);
const char  *charGetDesc      ( CHAR_DATA *ch);
const char  *charGetPassword  ( CHAR_DATA *ch);
const char  *charGetRdesc     ( CHAR_DATA *ch);
const char  *charGetMultiRdesc( CHAR_DATA *ch);
const char  *charGetMultiName ( CHAR_DATA *ch);
int          charGetLevel     ( CHAR_DATA *ch);
int          charGetSex       ( CHAR_DATA *ch);
bitvector_t  charGetBits      ( CHAR_DATA *ch, int field);
// for editing with the text editor
char       **charGetDescPtr   ( CHAR_DATA *ch);
LIST        *charGetInventory ( CHAR_DATA *ch);
BODY_DATA   *charGetBody      ( CHAR_DATA *ch);
const char  *charGetRace      ( CHAR_DATA *ch);
int          charGetUID       ( CHAR_DATA *ch);
room_vnum    charGetLoadroom  ( CHAR_DATA *ch);
OBJ_DATA    *charGetFurniture ( CHAR_DATA *ch);
int          charGetPos       ( CHAR_DATA *ch);
void        *charGetAuxiliaryData(const CHAR_DATA *ch, const char *name);
int          charGetImmInvis  ( CHAR_DATA *ch);

void         charSetSocket    ( CHAR_DATA *ch, SOCKET_DATA *socket);
void         charSetRoom      ( CHAR_DATA *ch, ROOM_DATA *room);
void         charSetName      ( CHAR_DATA *ch, const char *name);
void         charSetPassword  ( CHAR_DATA *ch, const char *password);
void         charSetLevel     ( CHAR_DATA *ch, int level);
void         charSetSex       ( CHAR_DATA *ch, int sex);
void         charSetDesc      ( CHAR_DATA *ch, const char *desc);
void         charSetRdesc     ( CHAR_DATA *ch, const char *rdesc);
void         charSetMultiRdesc( CHAR_DATA *ch, const char *multi_rdesc);
void         charSetMultiName ( CHAR_DATA *ch, const char *multi_name);
void         charSetBody      ( CHAR_DATA *ch, BODY_DATA *body);
void         charSetRace      ( CHAR_DATA *ch, const char *race);
void         charSetUID       ( CHAR_DATA *ch, int uid);
void         charSetLoadroom  ( CHAR_DATA *ch, room_vnum loadroom);
void         charSetFurniture ( CHAR_DATA *ch, OBJ_DATA *furniture);
void         charSetPos       ( CHAR_DATA *ch, int pos);
void         charSetImmInvis  ( CHAR_DATA *ch, int level);



//*****************************************************************************
//
// mob-specific functions
//
//*****************************************************************************
mob_vnum     charGetVnum       (CHAR_DATA *ch);
dialog_vnum  charGetDialog     (CHAR_DATA *ch);
const char  *charGetKeywords   (CHAR_DATA *ch);

void         charSetVnum       (CHAR_DATA *ch, mob_vnum vnum);
void         charSetDialog     (CHAR_DATA *ch, dialog_vnum vnum);
void         charSetKeywords   (CHAR_DATA *ch, const char *keywords);



//*****************************************************************************
//
// Bitfields and their bits
//
//*****************************************************************************
void         charToggleBit    ( CHAR_DATA *ch, int field, int bit);
void         charSetBit       ( CHAR_DATA *ch, int field, int bit);
void         charRemoveBit    ( CHAR_DATA *ch, int field, int bit);
bool         charIsBitSet     ( CHAR_DATA *ch, int field, int bit);

// hmmm... we should really replace bitvectors with something a bit
// more amenable to extention by modules. Perhaps use char_vars?
#define BITFIELD_PRFS             0

#define PRF_BUILDWALK             0    // (1 << 0)
#define PRF_MAPWALK               1    // (1 << 1)



//*****************************************************************************
//
// Sexes
//
//*****************************************************************************
#define SEX_NONE                (-1)
#define SEX_MALE                  0
#define SEX_FEMALE                1
#define SEX_NEUTRAL               2
#define NUM_SEXES                 3

const char *sexGetName(int sex);
int  sexGetNum(const char *sex);



//*****************************************************************************
//
// Positions
//
//*****************************************************************************
#define POS_NONE                (-1)
#define POS_UNCONCIOUS            0
#define POS_SLEEPING              1
#define POS_SITTING               2
//#define POS_UNBALANCED            3 <-- position or status affect?
#define POS_STANDING              3
#define POS_FLYING                4
#define NUM_POSITIONS             5

const char *posGetName(int pos);
const char *posGetActionSelf(int pos);
const char *posGetActionOther(int pos);
int posGetNum(const char *pos);
int poscmp(int pos1, int pos2);

#endif // __CHARACTER_H
