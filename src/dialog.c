//*****************************************************************************
//
// dialog.c
//
// contains all of the functions neccessary for interacting with mobile speech
// sets. This includes responses to greets, as well as responses to keywords
// in say, and answers to ask.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "dialog.h"
#include "storage.h"


struct dialog_data {
  int   vnum;      // what vnum are we in the world?
  char *name;      // the name of this dialog (e.g. generic citizen dialog)
  char *greet;     // what do we say when someone approaches/greets us?
  LIST *responses; // what kind of topics do we talk about?
};


struct response_data {
  char *keywords;      // what triggers the talking?
  char *message;       // what is the response?
  DIALOG_DATA *dialog; // which dialog do we belong to?
};



//*****************************************************************************
//
// response stuff
//
//*****************************************************************************
RESPONSE_DATA *newResponse(const char *keywords, const char *message) {
  RESPONSE_DATA *response = malloc(sizeof(RESPONSE_DATA));
  response->keywords = strdup(keywords ? keywords : "");
  response->message  = strdup(message  ? message  : "");
  response->dialog = NULL;
  return response;
}

void deleteResponse(RESPONSE_DATA *response) {
  if(response->keywords) free(response->keywords);
  if(response->message)  free(response->message);
  free(response);
}

RESPONSE_DATA *responseRead(STORAGE_SET *set) {
  return newResponse(read_string(set, "keywords"), read_string(set, "message"));
}

STORAGE_SET *responseStore(RESPONSE_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "keywords", data->keywords, NULL);
  store_string(set, "message",  data->message,  NULL);
  return set;
}

void responseCopyTo(RESPONSE_DATA *from, RESPONSE_DATA *to) {
  responseSetKeywords(to, responseGetKeywords(from));
  responseSetMessage (to, responseGetMessage(from));
}

RESPONSE_DATA *responseCopy(RESPONSE_DATA *response) {
  RESPONSE_DATA *newresp = newResponse(response->keywords, response->message);
  return newresp;
}

void responseSetDialog(RESPONSE_DATA *response, DIALOG_DATA *dialog) {
  response->dialog = dialog;
}

bool responseIsKeyword(RESPONSE_DATA *response, const char *keyword) {
  return is_keyword(response->keywords, keyword, FALSE);
}

DIALOG_DATA *responseGetDialog(RESPONSE_DATA *response) {
  return response->dialog;
}

void responseSetKeywords(RESPONSE_DATA *response, const char *keywords) {
  if(response->keywords) free(response->keywords);
  response->keywords = strdup(keywords ? keywords : "");
}

void responseSetMessage(RESPONSE_DATA *response, const char *message) {
  if(response->message) free(response->message);
  response->message = strdup(message ? message : "");
}

const char *responseGetKeywords(RESPONSE_DATA *response) {
  return response->keywords;
}

const char *responseGetMessage(RESPONSE_DATA *response) {
  return response->message;
}




//*****************************************************************************
//
// dialog stuff
//
//*****************************************************************************
DIALOG_DATA *newDialog() {
  DIALOG_DATA *dialog = malloc(sizeof(DIALOG_DATA));
  dialog->responses = newList();
  dialog->name  = strdup("");
  dialog->greet = strdup("");
  dialog->vnum  = NOTHING;
  return dialog;
}

void deleteDialog(DIALOG_DATA *dialog) {
  RESPONSE_DATA *response = NULL;
  while( (response = listPop(dialog->responses)) != NULL)
    deleteResponse(response);
  if(dialog->name)  free(dialog->name);
  if(dialog->greet) free(dialog->greet);
  free(dialog);
}

STORAGE_SET *dialogStore(DIALOG_DATA *dialog) {
  STORAGE_SET *set = new_storage_set();
  store_int   (set, "vnum",     dialog->vnum, NULL);
  store_string(set, "name",     dialog->name, NULL);
  store_string(set, "greet",    dialog->greet, NULL);
  store_list(set, "responses", gen_store_list(dialog->responses, responseStore),
	     NULL);
  return set;
}

DIALOG_DATA *dialogRead(STORAGE_SET *set) {
  DIALOG_DATA *dialog = newDialog();
  dialogSetVnum(dialog,  read_int   (set, "vnum"));
  dialogSetName(dialog,  read_string(set, "name"));
  dialogSetGreet(dialog, read_string(set, "greet"));

  deleteListWith(dialog->responses, deleteResponse);
  dialog->responses = gen_read_list(read_list(set, "responses"), responseRead);

  // we have to make sure they are all attached to us, now
  LIST_ITERATOR  *list_i = newListIterator(dialog->responses);
  RESPONSE_DATA *response = NULL;
  ITERATE_LIST(response, list_i)
    response->dialog = dialog;
  deleteListIterator(list_i);
  return dialog;
}


void dialogCopyTo(DIALOG_DATA *from, DIALOG_DATA *to) {
  // first remove all of the old responses
  deleteListWith(to->responses, deleteResponse);
  to->responses = newList();

  // now copy all the new ones over and set their set to us
  RESPONSE_DATA *response = NULL;
  LIST_ITERATOR *resp_i = newListIterator(from->responses);
  ITERATE_LIST(response, resp_i)
    dialogPut(to, responseCopy(response));
  deleteListIterator(resp_i);

  // now the name and greet
  if(to->name) free(to->name);
  if(to->greet) free(to->greet);

  to->name  = strdup(from->name ? from->name : "");
  to->greet = strdup(from->greet ? from->greet : "");
  to->vnum  = from->vnum;
}

DIALOG_DATA *dialogCopy(DIALOG_DATA *dialog) {
  DIALOG_DATA *dianew = newDialog();
  dialogCopyTo(dialog, dianew);
  return dianew;
}

void dialogPut(DIALOG_DATA *dialog, RESPONSE_DATA *response) {
  listQueue(dialog->responses, response);
  response->dialog = dialog;
}

void dialogRemove(DIALOG_DATA *dialog, RESPONSE_DATA *response) {
  if(listRemove(dialog->responses, response))
    response->dialog = NULL;
}

RESPONSE_DATA *dialogRemoveKeyword(DIALOG_DATA *dialog, const char *keyword) {
  RESPONSE_DATA *response = dialogGetResponse(dialog, keyword);
  if(response != NULL) {
    listRemove(dialog->responses, response);
    response->dialog = NULL;
  }
  return response;
}

RESPONSE_DATA *dialogRemoveNum(DIALOG_DATA *dialog, int entry_num) {
  RESPONSE_DATA *resp = listRemoveNum(dialog->responses, entry_num);
  if(resp) resp->dialog = NULL;
  return resp;
}

LIST *dialogGetList(DIALOG_DATA *dialog) {
  return dialog->responses;
}

RESPONSE_DATA *dialogGetResponse(DIALOG_DATA *dialog, const char *keyword) {
  LIST_ITERATOR *resp_i = newListIterator(dialog->responses);
  RESPONSE_DATA *response = NULL;

  ITERATE_LIST(response, resp_i)
    if(responseIsKeyword(response, keyword))
      break;
  
  deleteListIterator(resp_i);
  return response;
}

const char *dialogGetName(DIALOG_DATA *dialog) {
  return dialog->name;
}

dialog_vnum dialogGetVnum(DIALOG_DATA *dialog) {
  return dialog->vnum;
}

RESPONSE_DATA *dialogGetNum(DIALOG_DATA *dialog, int entry_num) {
  return listGet(dialog->responses, entry_num);
}

int dialogGetSize(DIALOG_DATA *dialog) {
  return listSize(dialog->responses);
}

const char *dialogGetGreet(DIALOG_DATA *dialog) {
  return dialog->greet;
}

void dialogSetGreet(DIALOG_DATA *dialog, const char *greet) {
  if(dialog->greet) free(dialog->greet);
  dialog->greet = strdup(greet ? greet : "");
}

void dialogSetVnum(DIALOG_DATA *dialog, dialog_vnum vnum) {
  dialog->vnum = vnum;
}

void dialogSetName(DIALOG_DATA *dialog, const char *name) {
  if(dialog->name) free(dialog->name);
  dialog->name = strdup(name ? name : "");
}

char *tagResponses(DIALOG_DATA *dialog, const char *string,
		   const char *start_tag, const char *end_tag) {
  LIST_ITERATOR *list_i = newListIterator(dialog->responses);
  RESPONSE_DATA *response  = NULL;
  char          *newstring = strdup(string);

  // go through, and apply colors for each response we have
  ITERATE_LIST(response, list_i) {
    char *oldstring = newstring;
    newstring = tag_keywords(response->keywords, oldstring, start_tag, end_tag);
    free(oldstring);
  }
  deleteListIterator(list_i);

  return newstring;
}
