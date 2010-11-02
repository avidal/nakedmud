//*****************************************************************************
//
// text_editor.c
//
// a small online text editor for people to use. Good for editing room
// descriptions, board messages, notes, and other long strings.
//
//*****************************************************************************

#include <strings.h>

#include "mud.h"
#include "socket.h"
#include "text_editor.h"
#include "utils.h"

// optional modules
#ifdef MODULE_SCRIPTS
#include "scripts/script.h"
#endif
#ifdef MODULE_OLC
#include "olc/olc.h"
#endif


// different formatting types
#define FORMAT_INDENT     (1 << 0)
#define FORMAT_SCRIPT     (1 << 1)

//
// sends a list of the control options to the socket
//
void show_text_editor_help( SOCKET_DATA *sock) {
  send_to_socket(sock,
	"Text Editor Options:\r\n"
	"  /s   /q       save and quit\r\n"
	"  /h   /?       bring up the editor options\r\n"
	"  /a            abort without saving changes\r\n"  
	"  /l            list the current buffer\r\n"
	"  /c            clear the buffer\r\n"
	"  /n            insert a new line\r\n"
	"  /d#           delete line with the specified number\r\n"
	"  /i# [text]    insert text at the specified line number\r\n"
	"  /e# [text]    changes the text at line # with the new text\r\n"
	"  /r  'a' 'b'   replace the first occurance of 'a' with 'b'\r\n"
	"  /ra 'a' 'b'   replace all occurances of 'a' with 'b'\r\n"
	"  /f            formats the text\r\n"
	"  /fi           formats the text with an indent\r\n"
	"  /v            drop indentation down (used for script editing)\r\n"
	"  /^            push indentation up (used for script editing\r\n\r\n"
		 );
}



//
// replace occurances of a with b
//
void text_editor_replace(SOCKET_DATA *sock, char *a, char *b, bool all) {
  char *text = strdup(buffer_string(sock->text_editor));
  int found = replace_string(&text, a, b, all);

  buffer_clear(sock->text_editor);
  buffer_strcat(sock->text_editor, text);
  free(text);
  send_to_socket(sock, "Replacing '%s' with '%s'. %d occurances replaced.\r\n",
		 a, b, found);
}


//
// Doubles as delete when newline == NULL
//
void text_editor_insert(SOCKET_DATA *sock,
			char *newline, int line) {
  char buf[MAX_BUFFER];
  const char *ptr = buffer_string(sock->text_editor);
  int curr_line = 1;
  int i = 0;

  for(i = 0; curr_line < line; i++) {
    // we've hit the end before we found our line
    if(ptr[i] == '\0') {
      send_to_socket(sock, "Line %d not found.\r\n", line);
      return;
    }

    if(ptr[i] == '\n')
      curr_line++;
    buf[i] = ptr[i];
  }

  // copy it all over
  sprintf(buf+i, "%s%s", newline, (sock->editor_mode == EDITOR_MODE_SCRIPT ?
				   "\n" : "\r\n"));
  strcat(buf, ptr+i);

  // set the new text editor's values
  buffer_clear(sock->text_editor);
  buffer_strcat(sock->text_editor, buf);
  send_to_socket(sock, "Line inserted.\r\n");
}


//
// replace the line number with the new value
//
void text_editor_edit(SOCKET_DATA *sock, char *newline, char line) {
  char buf[MAX_BUFFER];
  const char *ptr = buffer_string(sock->text_editor);
  int curr_line = 1;
  int i = 0;

  for(i = 0; curr_line < line; i++) {
    // we've hit the end before we found our line
    if(ptr[i] == '\0') {
      send_to_socket(sock, "Line %d not found.\r\n", line);
      return;
    }

    buf[i] = ptr[i];
    if(ptr[i] == '\n')
      curr_line++;
  }

  // copy it all over
  snprintf(&buf[i], MAX_BUFFER - i - 1, "%s%s", 
	   (newline == NULL ? "" : newline), 
	   // don't add returns to the very end of the buffer, or
	   // when we are deleting a line (newline == NULL)
	   ((ptr[i] == '\0' || newline == NULL) ? "" : 
	    (sock->editor_mode == EDITOR_MODE_SCRIPT ? "\n" :"\r\n")));


  // skip the next line
  for(; ptr[i] != '\0'; i++) {
    if(ptr[i] == '\n') {
      i++;
      break;
    }
  }

  // copy the remaining data over
  strncat(buf, &ptr[i], MAX_BUFFER - i - 1);

  // set the new text editor's values
  buffer_clear(sock->text_editor);
  buffer_strcat(sock->text_editor, buf);
  send_to_socket(sock, "%d Line replaced.\r\n", line);
}


//
// format the string of text
//
void text_editor_format(SOCKET_DATA *sock, bitvector_t options) {
  char *buf = strdup(buffer_string(sock->text_editor));

#ifdef MODULE_SCRIPTS
  if(IS_SET(options, FORMAT_SCRIPT))
    format_script(&(buf), MAX_BUFFER);
  else
#endif
    format_string(&(buf), 80, MAX_BUFFER, IS_SET(options, FORMAT_INDENT));

  buffer_clear(sock->text_editor);
  buffer_strcat(sock->text_editor, buf);
  free(buf);

  send_to_socket(sock, "Buffer formatted%s.\r\n", 
		 (IS_SET(options, FORMAT_INDENT) ? " with indent" : 
		  (IS_SET(options, FORMAT_SCRIPT) ? " as script" : "")));
}


//
// Display the text editor to the socket
//
void text_editor_display(SOCKET_DATA *sock, int mode, bool show_line_nums) {
  // just print the buffer if we don't need line nums
  if(!*buffer_string(sock->text_editor))
    send_to_socket(sock, "The buffer is empty.\r\n");
#ifdef MODULE_SCRIPTS
  // check if it's a script
  else if(mode == EDITOR_MODE_SCRIPT)
    script_display(sock, buffer_string(sock->text_editor), show_line_nums);
  // otherwise just cat the buffer
#endif
  else
    send_to_socket(sock, "%s\r\n", buffer_string(sock->text_editor));

}


void text_editor_loop (SOCKET_DATA *sock, char *arg) {
  // control command
  if(*arg == '/') {
    switch(*(arg+1)) {
    case 's':
    case 'S':
    case 'q':
    case 'Q':
      if(*(sock->text_pointer))
	free(*(sock->text_pointer));
      *(sock->text_pointer) = strdup(buffer_string(sock->text_editor));
      // fall through
    case 'a':
    case 'A':
      buffer_free(sock->text_editor);
      //      free(sock->text_pointer);
      sock->text_pointer = NULL;
      sock->state = sock->old_state;
      sock->in_text_edit = FALSE;

#ifdef MODULE_OLC
      // OLC is annoying, in that is uses the text editor for certain things
      // (e.g. room descriptions) but doesn't really have a way of
      // re-displaying the menu once the editing is done, in the game. So
      // here we have to put a little hack to make it display whenever we
      // exit OLC.
      if(sock->state == STATE_OLC)
	olc_menu(sock);
#endif
      break;

    case 'i':
    case 'I': {
      int line = 0;
      sscanf((arg+2), "%d", &line);

      // skip the number and the first space
      while(isdigit(*(arg+2)))
	arg++;
      if(isspace(*(arg+2))) arg++;

      text_editor_insert(sock, (arg+2), line);
      break;
    }

    case 'e':
    case 'E': {
      int line = 0;
      sscanf((arg+2), "%d", &line);

      // skip the number and the first space
      while(isdigit(*(arg+2)))
	arg++;
      if(isspace(*(arg+2))) arg++;

      text_editor_edit(sock, (arg+2), line);
      break;
    }

    case 'd':
    case 'D': {
      int line = 0;
      sscanf((arg+2), "%d", &line);

      // skip the number
      while(isdigit(*(arg+2)))
	arg++;

      text_editor_edit(sock, NULL, line);
      break;
    }

    case 'n':
    case 'N':
      send_to_socket(sock, "New line inserted.\r\n");
      if(sock->editor_mode == EDITOR_MODE_NORMAL)
	buffer_strcat(sock->text_editor, "\r\n");
      else if(sock->editor_mode == EDITOR_MODE_SCRIPT)
	buffer_strcat(sock->text_editor, "\n");
      break;

    case 'l':
    case 'L':
      text_editor_display(sock, sock->editor_mode, TRUE);
      break;

    case 'f':
    case 'F':
      text_editor_format(sock, toupper(*(arg+2)) == 'I' ? FORMAT_INDENT : 0);
      break;

    case 'r':
    case 'R': {
      bool all = FALSE;
      char *a;
      char *b;

      if(count_letters(arg, '\'', strlen(arg)) != 4) {
	send_to_socket(sock, "format is: /r[a] 'to replace' 'replacement'\r\n");
	break;	
      }

      if(toupper(*(arg+2)) == 'A')
	all = TRUE;

      // kill everything before the first '
      strtok(arg, "\'");

      a = strtok(NULL, "\'");
      if(a == NULL) {
	send_to_socket(sock, "format is: /r[a] 'to replace' 'replacement'\r\n");
	break;
      }

      // kill the leading the leading ' of b
      strtok(NULL, "\'");

      b = strtok(NULL, "\'");
      if(b == NULL)
	b = "\0"; // "\0" will get deleted at the end of this block

      text_editor_replace(sock, a, b, all);
      break;
    }

    case 'v':
    case 'V':
      sock->indent -= 2;
      if(sock->indent < 0) sock->indent = 0;
      send_to_socket(sock, "Dropped indent to %d spaces.\r\n", sock->indent);
      break;

    case '^':
      sock->indent += 2;
      if(sock->indent < 0) sock->indent = 0;
      send_to_socket(sock, "Upped indent to %d spaces.\r\n", sock->indent);
      break;

    case 'c':
    case 'C':
      sock->indent = 0;
      buffer_clear(sock->text_editor);
      send_to_socket(sock, "Buffer cleared.\r\n");
      break;

    case 'h':
    case 'H':
    case '?':
    default:
      show_text_editor_help(sock);
      break;
    }
  }
  else {
    // no newline on the first line
    //
    // bprintf was removed, because it causes problems if the
    // text we're editing has any %d's or %s's or whatnot in it
    // (e.g. when we're editing a script).
    // 
    if(sock->editor_mode == EDITOR_MODE_NORMAL) {
      // print our text
      buffer_strcat(sock->text_editor, arg);

      // print the newline
      buffer_strcat(sock->text_editor, "\r\n");      
    }

    else if(sock->editor_mode == EDITOR_MODE_SCRIPT) {
      // get rid of all trailing and leading white spaces
      trim(arg);

      // print spaces for our indent
      if(sock->indent > 0) {
	char fmt[8] = "%%-%ds\0";
	sprintf(fmt, fmt, sock->indent);
	bprintf(sock->text_editor, fmt, " ");
      }

      // print our text
      buffer_strcat(sock->text_editor, arg);

      // print the newline
      // python chokes on carraige returns, so we only have a newline here
      buffer_strcat(sock->text_editor, "\n");

      // if the last character is a colon, we need to up our indent
      if(arg[strlen(arg)-1] == ':')
	sock->indent += 2;
    }


    else {
      log_string("ERROR: Socket in invalid editor mode, %d", sock->editor_mode);
      send_to_socket(sock, 
		     "You are in an invalid editor mode, and your last line "
		     "was not appended.\r\n");
    }
  }
}


void start_text_editor( SOCKET_DATA *sock, char **text, int max_len, int mode) {
  // we're already in edit mode ... don't mess around
  if(sock->state == STATE_TEXT_EDITOR)
    return;

  sock->old_state = sock->state;
  sock->state     = STATE_TEXT_EDITOR;

  sock->text_editor  = buffer_new(max_len);

  
  buffer_strcat(sock->text_editor, (text ? *text : ""));
  // bprintf has been canned! Screws up if we have %d or %s (e.g. for scripts)
  // in there.
  //  bprintf(sock->text_editor, (text ? *text : "\0"));
  sock->text_pointer = text;
  sock->max_text_len = max_len;
  sock->in_text_edit = TRUE;
  sock->editor_mode  = mode;
  sock->indent       = 0;

  send_to_socket(sock,
"========================================================================\r\n"
"Begin text editing. /s on a new line to save, /a to abort. /h for help  \r\n"
"========================================================================\r\n"
	 );
  text_editor_display(sock, sock->editor_mode, TRUE);
}
