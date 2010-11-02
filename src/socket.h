#ifndef __SOCKET_H
#define __SOCKET_H

//*****************************************************************************
//
// socket.h
//
// all of the functions needed for working with character sockets
//
//*****************************************************************************



/* I'd be much happier of we hid this datastructure in a
   separate file at some point */
struct socket_data
{
  CHAR_DATA     * player;
  char          * hostname;
  char            inbuf[MAX_INPUT_LEN];
  char            outbuf[MAX_OUTPUT];
  char            next_command[MAX_BUFFER];
  bool            cmd_read;
  bool            bust_prompt;
  sh_int          lookup_status;
  sh_int          state;
  sh_int          control;
  sh_int          top_output;

  char          * page_string;   // the string that has been paged to us
  int             curr_page;     // the current page we're on
  int             tot_pages;     // the total number of pages the string has
  
  bool            in_text_edit;  // are we doing text editing?
  BUFFER        * text_editor;   // where we do our actual work
  char         ** text_pointer;  // where the work will go to
  int             max_text_len;  // the max length we are allowed
  int             editor_mode;   // what mode are we running the editor under?
  int             indent;        // how far do we indent (script editing)
  sh_int          old_state;     // the state we were in before text editing

  unsigned char   compressing;                 /* MCCP support */
  z_stream      * out_compress;                /* MCCP support */
  unsigned char * out_compress_buf;            /* MCCP support */

#ifdef MODULE_OLC
  OLC_DATA      * olc;
#endif
};


int   init_socket           ( void );
bool  new_socket            ( int sock );
void  close_socket          ( SOCKET_DATA *dsock, bool reconnect );
bool  read_from_socket      ( SOCKET_DATA *dsock );

/* sends the output directly */
bool  text_to_socket        ( SOCKET_DATA *dsock, const char *txt );
void  send_to_socket        ( SOCKET_DATA *dsock, const char *format, ...) __attribute__ ((format (printf, 2, 3)));

/* buffers the output        */
void  text_to_buffer        ( SOCKET_DATA *dsock, const char *txt );

void  next_cmd_from_buffer  ( SOCKET_DATA *dsock );
bool  flush_output          ( SOCKET_DATA *dsock );
void  handle_new_connections( SOCKET_DATA *dsock, char *arg );
void  clear_socket          ( SOCKET_DATA *sock_new, int sock );
void  recycle_sockets       ( void );
void *lookup_address        ( void *arg );


//*****************************************************************************
//
// set and get functions
//
//*****************************************************************************
#ifdef MODULE_OLC
OLC_DATA  *socketGetOLC      ( SOCKET_DATA *dsock);
#endif
sh_int     socketGetState    ( SOCKET_DATA *dsock);
CHAR_DATA *socketGetChar     ( SOCKET_DATA *dsock);

#ifdef MODULE_OLC
void       socketSetOLC      ( SOCKET_DATA *dsock, OLC_DATA *olc);
#endif
void       socketSetState    ( SOCKET_DATA *dsock, sh_int state);
void       socketSetChar     ( SOCKET_DATA *dsock, CHAR_DATA *ch);


#endif // __SOCKET_H
