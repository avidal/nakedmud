#ifndef __DIALOG_H
#define __DIALOG_H
//*****************************************************************************
//
// dialog.h
//
// contains all of the functions neccessary for interacting with mobile speech
// sets. This includes responses to greets, as well as responses to keywords
// in say, and answers to ask.
//
//*****************************************************************************


RESPONSE_DATA *newResponse       (void);
void           deleteResponse    (RESPONSE_DATA *response);

void           responseCopyTo    (RESPONSE_DATA *from, RESPONSE_DATA *to);
RESPONSE_DATA *responseCopy      (RESPONSE_DATA *response);

bool          responseIsKeyword (RESPONSE_DATA *response, const char *keyword);
DIALOG_DATA  *responseGetDialog (RESPONSE_DATA *response);

void         responseSetDialog  (RESPONSE_DATA *response, DIALOG_DATA *dialog);
void         responseSetKeywords(RESPONSE_DATA *response, const char *keywords);
void         responseSetMessage (RESPONSE_DATA *response, const char *message);

const char   *responseGetKeywords(RESPONSE_DATA *response);
const char   *responseGetMessage (RESPONSE_DATA *response);
BUFFER       *responseGetMessageBuffer(RESPONSE_DATA *response);

DIALOG_DATA   *newDialog         ();
void           deleteDialog      (DIALOG_DATA *dialog);

DIALOG_DATA   *dialogRead        (STORAGE_SET *set);
STORAGE_SET   *dialogStore       (DIALOG_DATA *dialog);

void           dialogCopyTo      (DIALOG_DATA *from, DIALOG_DATA *to);
DIALOG_DATA   *dialogCopy        (DIALOG_DATA *dialog);

void           dialogPut         (DIALOG_DATA *dialog, RESPONSE_DATA *response);
void           dialogRemove      (DIALOG_DATA *dialog, RESPONSE_DATA *response);
RESPONSE_DATA *dialogRemoveKeyword(DIALOG_DATA *dialog, const char *keyword);
RESPONSE_DATA *dialogRemoveNum   (DIALOG_DATA *dialog, int entry_num);

LIST          *dialogGetList     (DIALOG_DATA *dialog);
RESPONSE_DATA *dialogGetResponse (DIALOG_DATA *dialog, const char *keyword);
RESPONSE_DATA *dialogGetNum      (DIALOG_DATA *dialog, int entry_num);
int            dialogGetSize     (DIALOG_DATA *dialog);

BUFFER        *dialogGetGreetBuffer(DIALOG_DATA *dialog);
const char    *dialogGetGreet    (DIALOG_DATA *dialog);
void           dialogSetGreet    (DIALOG_DATA *dialog, const char *greet);

void           dialogSetVnum     (DIALOG_DATA *dialog, dialog_vnum vnum);
dialog_vnum    dialogGetVnum     (DIALOG_DATA *dialog);

void           dialogSetName     (DIALOG_DATA *dialog, const char *name);
const char    *dialogGetName     (DIALOG_DATA *dialog);

char          *tagResponses      (DIALOG_DATA *dialog, const char *string,
				  const char *start_tag, const char *end_tag);

#endif //__DIALOG_H
