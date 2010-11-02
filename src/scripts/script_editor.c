//*****************************************************************************
//
// script_editor.c
//
// this is an extention of the basic script editor (/src/editor/editor.h) to
// be tailored for script editing. Provides syntax coloring and line number
// display, and auto indenting.
//
//*****************************************************************************

#include "../mud.h"
#include "../socket.h"
#include "../auxiliary.h"

#include "../editor/editor.h"
#include "script_editor.h"
#include "script.h"



//*****************************************************************************
// auxiliary data and functions
//*****************************************************************************
typedef struct script_editor_aux_data {
  int indent; // how far are we indented for our next line of code?
} SCRIPT_EDITOR_AUX_DATA;

SCRIPT_EDITOR_AUX_DATA *
newScriptEditorAuxData() {
  SCRIPT_EDITOR_AUX_DATA *data = malloc(sizeof(SCRIPT_EDITOR_AUX_DATA));
  data->indent = 0;
  return data;
}



//*****************************************************************************
// local functions and data structures
//*****************************************************************************
EDITOR *script_editor = NULL;


//
// increase the indentation in our script by 2
void scriptEditorIndent(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  SCRIPT_EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "script_editor_aux_data");
  data->indent += 2;
}

//
// decrease the indentation in our script by 2
void scriptEditorUndent(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  SCRIPT_EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "script_editor_aux_data");
  data->indent -= 2;
  if(data->indent < 0)
    data->indent = 0;
}

//
// return what our current level of indenting is
int socketGetScriptEditorIndent(SOCKET_DATA *sock) {
  SCRIPT_EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "script_editor_aux_data");
  return data->indent;
}

//
// append new text to the script editor. Make sure we add the appropriate amount
// of spaces before the addition, and increment/decrement our indent as needed
void scriptEditorAppend(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  // if we're playing with else/elif/case, take down the input a notch
  if(!strncmp(arg, "else:", 5) ||
     !strncmp(arg, "elif ", 5) ||
     !strncmp(arg, "case ", 5))
    scriptEditorUndent(sock, NULL, NULL);

  // add in our indents if neccessary
  int indent = socketGetScriptEditorIndent(sock);
  if(indent > 0) {
    char fmt[20];
    sprintf(fmt, "%%%ds", indent);
    bprintf(buf, fmt, " ");
  }

  // cat the new line of code
  bufferCat(buf, arg);
  bufferCat(buf, "\n");

  // see if we need to change our indent
  int len = strlen(arg);
  if(len > 0 && arg[len-1] == ':')
    scriptEditorIndent(sock, NULL, NULL);
}


//
// list the script buffer to the socket, but do all of our syntax highlighting
void scriptEditorList(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  script_display(sock, bufferString(buf), TRUE);
}


//
// \r really screws up python; this function makes sure they're all stripped out
void scriptEditorFormat(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  bufferReplace(buf, "\r", "", TRUE);
}

//
// prepare our socket to use the script editor
void socketInitScriptEditor(SOCKET_DATA *sock) {
  SCRIPT_EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "script_editor_aux_data");
  data->indent = 0;
}



//*****************************************************************************
// implementation of script_editor.h
//*****************************************************************************
void init_script_editor() {
  script_editor = newEditor();
  editorSetAppend(script_editor, scriptEditorAppend);
  editorSetInit(script_editor, socketInitScriptEditor);
  editorAddCommand(script_editor, "v", "        Indent down the script editor",
		   scriptEditorUndent);
  editorAddCommand(script_editor, "^", "        Indent up the script editor",
		   scriptEditorIndent);
  editorAddCommand(script_editor, "l", "        List the current buffer contents",
		   scriptEditorList);
  editorAddCommand(script_editor, "f", "        Strips all bad characters out of the script",
		   scriptEditorFormat);

  auxiliariesInstall("script_editor_aux_data", 
		     newAuxiliaryFuncs(AUXILIARY_TYPE_SOCKET,
				       newScriptEditorAuxData, free,
				       NULL, NULL, NULL, NULL));
}
