#ifndef __TEXT_EDITOR_H
#define __TEXT_EDITOR_H
//*****************************************************************************
//
// text_editor.h
//
// a small online text editor for people to use. Good for editing room
// descriptions, board messages, notes, and other long strings.
//
//*****************************************************************************



#define EDITOR_MODE_NORMAL        0
#define EDITOR_MODE_SCRIPT        1

void start_text_editor(SOCKET_DATA *sock, char **text, int max_len, int mode);
void text_editor_loop (SOCKET_DATA *sock, char *arg);
void start_notepad    (SOCKET_DATA *sock, const char *txt, int max_len, 
		       int mode);

#endif //__TEXT_EDITOR_H
