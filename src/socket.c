//*****************************************************************************
//
// socket.c
//
// This file contains the socket code, used for accepting new connections as 
// well as reading and writing to sockets, and closing down unused sockets.
//
//*****************************************************************************
#include "wrapsock.h"
#include <netdb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h> 

#include "mud.h"
#include "character.h"
#include "account.h"
#include "save.h"
#include "utils.h"
#include "socket.h"
#include "auxiliary.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_ALIAS
#include "alias/alias.h"
#endif



//
// Here it is... the big ol' datastructure for sockets. Yum.
struct socket_data {
  CHAR_DATA     * player;
  ACCOUNT_DATA  * account;
  char          * hostname;
  char            inbuf[MAX_INPUT_LEN];
  char            outbuf[MAX_OUTPUT];
  char            next_command[MAX_BUFFER];
  bool            cmd_read;
  bool            bust_prompt;
  bool            closed;
  sh_int          lookup_status;
  sh_int          control;
  sh_int          top_output;

  char          * page_string;   // the string that has been paged to us
  int             curr_page;     // the current page we're on
  int             tot_pages;     // the total number of pages the string has
  
  BUFFER        * text_editor;   // where we do our actual work

  LIST          * input_handlers;// a stack of our input handlers and prompts
  LIST          * input;         // lines of input we have received

  unsigned char   compressing;                 /* MCCP support */
  z_stream      * out_compress;                /* MCCP support */
  unsigned char * out_compress_buf;            /* MCCP support */

  HASHTABLE     * auxiliary;     // auxiliary data installed by other modules
};


//
// contains an input handler and the socket prompt in one structure, so they
// can be stored together in the socket_data
typedef struct input_handler_data {
  void (* handler)(SOCKET_DATA *, char *);
  void (*  prompt)(SOCKET_DATA *);
} IH_PAIR;


//
// required for looking up a socket's IP in a new thread
typedef struct lookup_data {
  SOCKET_DATA    * dsock;   // the socket we wish to do a hostlookup on
  char           * buf;     // the buffer it should be stored in
} LOOKUP_DATA;



/* global variables */
fd_set        fSet;             /* the socket list for polling       */
fd_set        rFd;

/* mccp support */
const unsigned char compress_will   [] = { IAC, WILL, TELOPT_COMPRESS,  '\0' };
const unsigned char compress_will2  [] = { IAC, WILL, TELOPT_COMPRESS2, '\0' };


/*
 * Init_socket()
 *
 * Used at bootup to get a free
 * socket to run the server from.
 */
int init_socket()
{
  struct sockaddr_in my_addr;
  int sockfd, reuse = 1;

  /* let's grab a socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  /* setting the correct values */
  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = INADDR_ANY;
  my_addr.sin_port = htons(mudport);

  /* this actually fixes any problems with threads */
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) == -1)
  {
    perror("Error in setsockopt()");
    exit(1);
  } 

  /* bind the port */
  bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr));

  /* start listening already :) */
  listen(sockfd, 3);

  /* return the socket */
  return sockfd;
}


/* 
 * New_socket()
 *
 * Initializes a new socket, get's the hostname
 * and puts it in the active socket_list.
 */
bool new_socket(int sock)
{
  struct sockaddr_in   sock_addr;
  pthread_attr_t       attr;
  pthread_t            thread_lookup;
  LOOKUP_DATA        * lData;
  SOCKET_DATA           * sock_new;
  int                  argp = 1;
  socklen_t            size;

  /* initialize threads */
  pthread_attr_init(&attr);   
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  /* create and clear the socket */
  sock_new = calloc(1, sizeof(SOCKET_DATA));

  /* attach the new connection to the socket list */
  FD_SET(sock, &fSet);

  /* clear out the socket */
  clear_socket(sock_new, sock);
  sock_new->closed = FALSE;

  /* set the socket as non-blocking */
  ioctl(sock, FIONBIO, &argp);

  /* update the socket list */
  listPut(socket_list, sock_new);

  /* do a host lookup */
  size = sizeof(sock_addr);
  if (getpeername(sock, (struct sockaddr *) &sock_addr, &size) < 0)
  {
    perror("New_socket: getpeername");
    sock_new->hostname = strdup("unknown");
  }
  else
  {
    /* set the IP number as the temporary hostname */
    sock_new->hostname = strdup(inet_ntoa(sock_addr.sin_addr));

    if (!compares(sock_new->hostname, "127.0.0.1"))
    {
      /* allocate some memory for the lookup data */
      if ((lData = malloc(sizeof(*lData))) == NULL)
      {
        bug("New_socket: Cannot allocate memory for lookup data.");
        abort();
      }

      /* Set the lookup_data for use in lookup_address() */
      lData->buf    =  strdup((char *) &sock_addr.sin_addr);
      lData->dsock  =  sock_new;

      /* dispatch the lookup thread */
      pthread_create(&thread_lookup, &attr, &lookup_address, (void*) lData);
    }
    else sock_new->lookup_status++;
  }

  /* negotiate compression */
  text_to_buffer(sock_new, (char *) compress_will2);
  text_to_buffer(sock_new, (char *) compress_will);

  /* send the greeting */
  text_to_buffer(sock_new, greeting);
  text_to_buffer(sock_new, "What is your account (not character) name? ");

  /* everything went as it was supposed to */
  return TRUE;
}


/*
 * Close_socket()
 *
 * Will close one socket directly, freeing all
 * resources and making the socket availably on
 * the socket free_list.
 */
void close_socket(SOCKET_DATA *dsock, bool reconnect)
{
  if (dsock->lookup_status > TSTATE_DONE) return;
  dsock->lookup_status += 2;

  /* remove the socket from the polling list */
  FD_CLR(dsock->control, &fSet);

  /* remove ourself from the list */
  //
  // NO! We don't want to remove ourself from the list just yet.
  // We will do that the next time we show up in the game_loop.
  // removing now will not close the socket completely. Why, I'm not
  // entirely sure... but it doesn't ;)
  //   - Geoff, Dec26/04
  //
  //  listRemove(socket_list, dsock);
  //

  // we have a character, and it's one that's
  // not in the process of being created
  if (dsock->player && charGetUID(dsock->player) != NOBODY) {
    if (reconnect)
      text_to_socket(dsock, "This connection has been taken over.\r\n");
    else {
      charSetSocket(dsock->player, NULL);
      log_string("Closing link to %s", charGetName(dsock->player));
    }
  }
  else if(dsock->player) {
    charSetSocket(dsock->player, NULL);
    extract_mobile(dsock->player);
  }
  
  if(dsock->account)
    deleteAccount(dsock->account);

  /* set the closed state */
  dsock->closed = TRUE;
  //  dsock->state = STATE_CLOSED;
}


/* 
 * Read_from_socket()
 *
 * Reads one line from the socket, storing it
 * in a buffer for later use. Will also close
 * the socket if it tries a buffer overflow.
 */
bool read_from_socket(SOCKET_DATA *dsock)
{
  int size;
  extern int errno;

  /* check for buffer overflows, and drop connection in that case */
  size = strlen(dsock->inbuf);
  if (size >= sizeof(dsock->inbuf) - 2)
  {
    text_to_socket(dsock, "\n\r!!!! Input Overflow !!!!\n\r");
    return FALSE;
  }

  /* start reading from the socket */
  for (;;)
  {
    int sInput;
    int wanted = sizeof(dsock->inbuf) - 2 - size;

    sInput = read(dsock->control, dsock->inbuf + size, wanted);

    if (sInput > 0)
    {
      size += sInput;

      if (dsock->inbuf[size-1] == '\n' || dsock->inbuf[size-1] == '\r')
        break;
    }
    else if (sInput == 0)
    {
      log_string("Read_from_socket: EOF");
      return FALSE;
    }
    else if (errno == EAGAIN || sInput == wanted)
      break;
    else
    {
      perror("Read_from_socket");
      return FALSE;
    }
  }
  dsock->inbuf[size] = '\0';
  return TRUE;
}


/*
 * Text_to_socket()
 *
 * Sends text directly to the socket,
 * will compress the data if needed.
 */
bool text_to_socket(SOCKET_DATA *dsock, const char *txt)
{
  int iBlck, iPtr, iWrt = 0, length, control = dsock->control;

  length = strlen(txt);

  /* write compressed */
  if (dsock && dsock->out_compress)
  {
    dsock->out_compress->next_in  = (unsigned char *) txt;
    dsock->out_compress->avail_in = length;

    while (dsock->out_compress->avail_in)
    {
      dsock->out_compress->avail_out = COMPRESS_BUF_SIZE - (dsock->out_compress->next_out - dsock->out_compress_buf);

      if (dsock->out_compress->avail_out)
      {
        int status = deflate(dsock->out_compress, Z_SYNC_FLUSH);

        if (status != Z_OK)
        return FALSE;
      }

      length = dsock->out_compress->next_out - dsock->out_compress_buf;
      if (length > 0)
      {
        for (iPtr = 0; iPtr < length; iPtr += iWrt)
        {
          iBlck = UMIN(length - iPtr, 4096);
          if ((iWrt = write(control, dsock->out_compress_buf + iPtr, iBlck)) < 0)
          {
            perror("Text_to_socket (compressed):");
            return FALSE;
          }
        }
        if (iWrt <= 0) break;
        if (iPtr > 0)
        {
          if (iPtr < length)
            memmove(dsock->out_compress_buf, dsock->out_compress_buf + iPtr, length - iPtr);

          dsock->out_compress->next_out = dsock->out_compress_buf + length - iPtr;
        }
      }
    }
    return TRUE;
  }

  /* write uncompressed */
  for (iPtr = 0; iPtr < length; iPtr += iWrt)
  {
    iBlck = UMIN(length - iPtr, 4096);
    if ((iWrt = write(control, txt + iPtr, iBlck)) < 0)
    {
      perror("Text_to_socket:");
      return FALSE;
    }
  }

  return TRUE;
}


void  send_to_socket( SOCKET_DATA *dsock, const char *format, ...) {
  if(format && *format) {
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);
    text_to_buffer(dsock, buf);
  }
}


/*
 * Text_to_buffer()
 *
 * Stores outbound text in a buffer, where it will
 * stay untill it is flushed in the gameloop.
 *
 * Will also parse ANSI colors and other tags.
 */
void text_to_buffer(SOCKET_DATA *dsock, const char *txt)
{
  static char output[8 * MAX_BUFFER];
  bool underline = FALSE, bold = FALSE;
  int iPtr = 0, last = -1, i = 0, j, k;
  int length = strlen(txt);

  /* the color struct */
  struct sAnsiColor
  {
    const char    cTag;
    const char  * cString;
    int           aFlag;
  };

  /* the color table... */
  const struct sAnsiColor ansiTable[] =
  {
    { 'd',  "30",  eTHIN },
    { 'D',  "30",  eBOLD },
    { 'r',  "31",  eTHIN },
    { 'R',  "31",  eBOLD },
    { 'g',  "32",  eTHIN },
    { 'G',  "32",  eBOLD },
    { 'y',  "33",  eTHIN },
    { 'Y',  "33",  eBOLD },
    { 'b',  "34",  eTHIN },
    { 'B',  "34",  eBOLD },
    { 'p',  "35",  eTHIN },
    { 'P',  "35",  eBOLD },
    { 'c',  "36",  eTHIN },
    { 'C',  "36",  eBOLD },
    { 'w',  "37",  eTHIN },
    { 'W',  "37",  eBOLD },

    /* the end tag */
    { '\0',  "",   eTHIN }
  };

  if (length >= MAX_BUFFER)
  {
    log_string("text_to_buffer: buffer overflow.");
    return;
  }


  while (*txt != '\0' && i++ < length)
  {
    /* simple bound checking */
    if (iPtr > (8 * MAX_BUFFER - 15))
      break;

    switch(*txt)
    {
      default:
        output[iPtr++] = *txt++;
        break;
      case '{':
        i++; txt++;

        /* toggle underline on/off with #u */
        if (*txt == 'u')
        {
          txt++;
          if (underline)
          {
            underline = FALSE;
            output[iPtr++] =  27; output[iPtr++] = '['; output[iPtr++] = '0';
            if (bold)
            {
              output[iPtr++] = ';'; output[iPtr++] = '1';
            }
            if (last != -1)
            {
              output[iPtr++] = ';';
              for (j = 0; ansiTable[last].cString[j] != '\0'; j++)
              {
                output[iPtr++] = ansiTable[last].cString[j];
              }
            }
            output[iPtr++] = 'm';
          }
          else
          {
            underline = TRUE;
            output[iPtr++] =  27; output[iPtr++] = '[';
            output[iPtr++] = '4'; output[iPtr++] = 'm';
          }
        }

        /* parse {{ to { */
        else if (*txt == '{')
        {
          txt++;
          output[iPtr++] = '{';
        }

        /* {n should clear all tags */
        else if (*txt == 'n')
        {
          txt++;
          if (last != -1 || underline || bold)
          {  
            underline = FALSE;
            bold = FALSE;
            output[iPtr++] =  27; output[iPtr++] = '[';
            output[iPtr++] = '0'; output[iPtr++] = 'm';
          }

          last = -1;
        }

        /* check for valid color tag and parse */
        else
        {
          bool validTag = FALSE;

          for (j = 0; ansiTable[j].cString[0] != '\0'; j++)
          {
            if (*txt == ansiTable[j].cTag)
            {
              validTag = TRUE;

              /* we only add the color sequence if it's needed */
              if (last != j)
              {
                bool cSequence = FALSE;

                /* escape sequence */
                output[iPtr++] = 27; output[iPtr++] = '[';

                /* remember if a color change is needed */
                if (last == -1 || last / 2 != j / 2)
                  cSequence = TRUE;

                /* handle font boldness */
                if (bold && ansiTable[j].aFlag == eTHIN)
                {
                  output[iPtr++] = '0';
                  bold = FALSE;

                  if (underline)
                  {
                    output[iPtr++] = ';'; output[iPtr++] = '4';
                  }

                  /* changing to eTHIN wipes the old color */
                  output[iPtr++] = ';';
                  cSequence = TRUE;
                }
                else if (!bold && ansiTable[j].aFlag == eBOLD)
                {
                  output[iPtr++] = '1';
                  bold = TRUE;

                  if (cSequence)
                    output[iPtr++] = ';';
                }

                /* add color sequence if needed */
                if (cSequence)
                {
                  for (k = 0; ansiTable[j].cString[k] != '\0'; k++)
                  {
                    output[iPtr++] = ansiTable[j].cString[k];
                  }
                }

                output[iPtr++] = 'm';
              }

              /* remember the last color */
              last = j;
            }
          }

          /* it wasn't a valid color tag */
          if (!validTag)
            output[iPtr++] = '{';
          else
            txt++;
        }
        break;   
    }
  }

  /* and terminate it with the standard color */
  /*
  if (last != -1 || underline || bold) 
  {
    output[iPtr++] =  27; output[iPtr++] = '[';
    output[iPtr++] = '0'; output[iPtr++] = 'm';
  } 
  */
  output[iPtr] = '\0';

  /* check to see if the socket can accept that much data */
  if (dsock->top_output + iPtr >= MAX_OUTPUT)
  {
    //
    // what happens if the buffer overflow is on an immortal who can see the
    // logs? We get tossed into an infinite loop... this really isn't too big
    // of a deal to have to report. Let's just leave it out.
    //   - Geoff, Mar 22/05
    //
    //    bug("Text_to_buffer: ouput overflow on %s.", dsock->hostname);
    return;
  }

  // if we're at the head of the outbuf and haven't entered a command, 
  // also copy a newline so we're not printing in front of the prompt
  if(dsock->top_output == 0 && !dsock->bust_prompt) {
    strcpy(dsock->outbuf, "\r\n");
    dsock->top_output += 2;
  }

  /* add data to buffer */
  strcpy(dsock->outbuf + dsock->top_output, output);
  dsock->top_output += iPtr;
}


void next_cmd_from_buffer(SOCKET_DATA *dsock) {
  // do we have stuff in our input list? If so, use that instead of inbuf
  if(listSize(dsock->input) > 0) {
    char *cmd = listPop(dsock->input);
    strncpy(dsock->next_command, cmd, MAX_BUFFER);
    dsock->cmd_read    = TRUE;
    dsock->bust_prompt = TRUE;
    free(cmd);
  }
  else {
    int size = 0, i = 0, j = 0, telopt = 0;

    // if theres already a command ready, we return
    if(dsock->next_command[0] != '\0')
      return;

    // if there is nothing pending, then return
    if(dsock->inbuf[0] == '\0')
      return;

    // check how long the next command is
    while(dsock->inbuf[size] != '\0' && 
	  dsock->inbuf[size] != '\n' && dsock->inbuf[size] != '\r')
      size++;

    /* we only deal with real commands */
    if(dsock->inbuf[size] == '\0')
      return;

    // copy the next command into next_command
    for(; i < size; i++) {
      if(dsock->inbuf[i] == (signed char) IAC)
	telopt = 1;
      else if(telopt == 1 && (dsock->inbuf[i] == (signed char) DO || 
			      dsock->inbuf[i] == (signed char) DONT))
	telopt = 2;

      // check for compression format
      else if(telopt == 2) {
	unsigned char compress_opt = dsock->inbuf[i];
	telopt = 0;
	
	// check if we're using a valid compression
	if(compress_opt == TELOPT_COMPRESS || compress_opt == TELOPT_COMPRESS2){
	  // start compressing
	  if(dsock->inbuf[i-1] == (signed char) DO)                  
	    compressStart(dsock, compress_opt);
	  // stop compressing
	  else if(dsock->inbuf[i-1] == (signed char) DONT)
	    compressEnd(dsock, compress_opt, FALSE);
	}
      }
      else if(isprint(dsock->inbuf[i]) && isascii(dsock->inbuf[i])) {
	dsock->next_command[j++] = dsock->inbuf[i];
      }
    }
    dsock->next_command[j] = '\0';

    // skip forward to the next line
    while(dsock->inbuf[size] == '\n' || dsock->inbuf[size] == '\r') {
      dsock->cmd_read = TRUE;
      dsock->bust_prompt = TRUE;   // seems like a good place to check
      size++;
    }

    // use i as a static pointer
    i = size;
    
    // move the context of inbuf down
    while(dsock->inbuf[size] != '\0') {
      dsock->inbuf[size - i] = dsock->inbuf[size];
      size++;
    }
    dsock->inbuf[size - i] = '\0';
  }
}



bool flush_output(SOCKET_DATA *dsock)
{
  IH_PAIR *pair = listGet(dsock->input_handlers, 0);
  void (* prompt_func)(SOCKET_DATA *) = (pair ? pair->prompt : NULL);

  // quit if we have no output and don't need/can't have a prompt
  if(dsock->top_output <= 0 && (!dsock->bust_prompt || !prompt_func))
    return TRUE;

  if(dsock->bust_prompt && prompt_func) {
    prompt_func(dsock);
    dsock->bust_prompt = FALSE;
  }

  // reset the top pointer
  dsock->top_output = 0;

  /*
   * Send the buffer, and return FALSE
   * if the write fails.
   */
  if (!text_to_socket(dsock, dsock->outbuf))
    return FALSE;

  // Success
  return TRUE;
}




//*****************************************************************************
//
// SOCKET MAINTENANCE
//
// Functions below this point are mainly concerned with the upkeep and
// maintenance of sockets (making sure they are initialized, garbage collected, 
// getting their addresses, etc...)
//
//*****************************************************************************
void deleteSocket(SOCKET_DATA *sock) {
  if(sock->hostname)       free(sock->hostname);
  if(sock->page_string)    free(sock->page_string);
  if(sock->text_editor)    deleteBuffer(sock->text_editor);
  if(sock->input_handlers) deleteListWith(sock->input_handlers, free);
  if(sock->input)          deleteListWith(sock->input, free);
  if(sock->auxiliary)      deleteAuxiliaryData(sock->auxiliary);
  free(sock);
}

void clear_socket(SOCKET_DATA *sock_new, int sock)
{
  if(sock_new->page_string)    free(sock_new->page_string);
  if(sock_new->text_editor)    deleteBuffer(sock_new->text_editor);
  if(sock_new->input_handlers) deleteListWith(sock_new->input_handlers, free);
  if(sock_new->auxiliary)      deleteAuxiliaryData(sock_new->auxiliary);
  if(sock_new->input)          deleteListWith(sock_new->input, free);

  bzero(sock_new, sizeof(*sock_new));
  sock_new->auxiliary = newAuxiliaryData(AUXILIARY_TYPE_SOCKET);
  sock_new->input_handlers = newList();
  sock_new->input          = newList();
  socketPushInputHandler(sock_new, handle_new_connections, NULL);
  sock_new->control        = sock;
  sock_new->lookup_status  = TSTATE_LOOKUP;
  sock_new->top_output     = 0;

  sock_new->text_editor    = newBuffer(MAX_BUFFER);
}


/* does the lookup, changes the hostname, and dies */
void *lookup_address(void *arg)
{
  LOOKUP_DATA *lData = (LOOKUP_DATA *) arg;
  struct hostent *from = 0;
  //  struct hostent ent;
  //  char buf[16384];
  //  int err;

  /* do the lookup and store the result at &from */
  from = gethostbyaddr(lData->buf, sizeof(lData->buf), AF_INET);

  /* did we get anything ? */
  if (from && from->h_name)
  {
    free(lData->dsock->hostname);
    lData->dsock->hostname = strdup(from->h_name);
  }

  /* set it ready to be closed or used */
  lData->dsock->lookup_status++;

  /* free the lookup data */
  free(lData->buf);
  free(lData);

  /* and kill the thread */
  pthread_exit(0);
  return NULL;
}


void recycle_sockets()
{
  SOCKET_DATA *dsock;
  LIST_ITERATOR *sock_i = newListIterator(socket_list);

  ITERATE_LIST(dsock, sock_i) {
    if (dsock->lookup_status != TSTATE_CLOSED) 
      continue;

    /* remove the socket from the main list */
    listRemove(socket_list, dsock);

    /* close the socket */
    close(dsock->control);

    /* stop compression */
    compressEnd(dsock, dsock->compressing, TRUE);

    /* delete the socket from memory */
    deleteSocket(dsock);
  }
  deleteListIterator(sock_i);
}


/* reset all of the sockets' control values */
void reconnect_copyover_sockets() {
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL; 
  ITERATE_LIST(sock, sock_i)
    FD_SET(sock->control, &fSet);
  deleteListIterator(sock_i);
}


/* Recover from a copyover - load players */
void copyover_recover() {     
  CHAR_DATA    *dMob;
  ACCOUNT_DATA *account;
  SOCKET_DATA  *dsock;
  FILE *fp;
  char acct[100];
  char name[100];
  char host[MAX_BUFFER];
  int desc;
      
  log_string("Copyover recovery initiated");

  if ((fp = fopen(COPYOVER_FILE, "r")) == NULL) {  
    log_string("Copyover file not found. Exitting.");
    exit (1);
  }
      
  /* In case something crashes - doesn't prevent reading */
  unlink(COPYOVER_FILE);

  for (;;) {  
    fscanf(fp, "%d %s %s %s\n", &desc, acct, name, host);
    if (desc == -1)
      break;

    // Many thanks to Rhaelar for the help in finding this bug; clear_socket
    // does not like receiving freshly malloc'd data. We have to make sure
    // everything is zeroed before we pass it to clear_socket
    //    dsock = malloc(sizeof(*dsock));
    dsock = calloc(1, sizeof(*dsock));
    clear_socket(dsock, desc);

    dsock->hostname = strdup(host);
    listPut(socket_list, dsock);

    // load account data
    if((account = load_account(acct)) != NULL)
      socketSetAccount(dsock, account);
    // no luck!
    else {
      printf("Account load failed.\r\n");
      close_socket(dsock, FALSE);
      continue;
    }

    // load player data
    if ((dMob = load_player(name)) != NULL) {
      // attach to socket
      charSetSocket(dMob, dsock);
      socketSetChar(dsock, dMob);

      // try putting the character into the game
      // close the socket if we fail.
      if(!try_enter_game(dMob)) {
	printf("Enter game failed.\r\n");
	// do not bother extracting, since we haven't entered the game yet
	deleteChar(socketGetChar(dsock));
	socketSetChar(dsock, NULL);
	close_socket(dsock, FALSE);
	continue;
      }
    }
    // no luck
    else {
      printf("Player load failed.\r\n");
      close_socket(dsock, FALSE);
      continue;
    }
   
    // Write something, and check if it goes error-free
    if (!text_to_socket(dsock, "\n\r <*>  And before you know it, everything has changed  <*>\n\r")) { 
      close_socket(dsock, FALSE);
      continue;
    }
  
    // make sure the socket can be used
    dsock->bust_prompt    =  TRUE;
    dsock->lookup_status  =  TSTATE_DONE;
    socketReplaceInputHandler(dsock, account_handle_menu, account_menu);
    socketPushInputHandler(dsock, handle_cmd_input, show_prompt);

    // negotiate compression
    text_to_buffer(dsock, (char *) compress_will2);
    text_to_buffer(dsock, (char *) compress_will);
  }
  fclose(fp);

  // now, set all of the sockets' control to the new fSet
  reconnect_copyover_sockets();
}     


void socket_handler() {
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL; 

  ITERATE_LIST(sock, sock_i) {
    // Close sockects we are unable to read from, or if we have no handler
    // to take in input
    if ((FD_ISSET(sock->control, &rFd) && !read_from_socket(sock)) ||
	listSize(sock->input_handlers) == 0) {
      close_socket(sock, FALSE);
      continue;
    }

    /* Ok, check for a new command */
    next_cmd_from_buffer(sock);
    
    /* Is there a new command pending ? */
    if (sock->cmd_read) {
      socketGetInputHandler(sock)(sock, sock->next_command);
      sock->next_command[0] = '\0';
      sock->cmd_read = FALSE;
    }

#ifdef MODULE_ALIAS
    // ACK!! this is so yucky, but I can't think of a better way to do it...
    // if this command was put in place by an alias, decrement the alias_queue
    // counter by one. This counter is in place mainly so aliases do not end
    // up calling eachother and making us get stuck in an infinite loop.
    if(sock->player) {
      int alias_queue = charGetAliasesQueued(sock->player);
      if(alias_queue > 0)
	charSetAliasesQueued(sock->player, --alias_queue);
    }
#endif
    
    /* if the player quits or get's disconnected */
    if(sock->closed)
      continue;
    
    /* Send all new data to the socket and close it if any errors occour */
    if (!flush_output(sock))
      close_socket(sock, FALSE);
  } deleteListIterator(sock_i);
}


int count_pages(const char *string) {
  int num_newlines = count_letters(string, '\n', strlen(string));
  return ((num_newlines / NUM_LINES_PER_PAGE) + 
	  (num_newlines % NUM_LINES_PER_PAGE != 0));
}

void show_page(SOCKET_DATA *dsock, int page_num) {
  static char buf[MAX_BUFFER];
  char *page = dsock->page_string;
  *buf = '\0';
  int buf_i = 0, newline_count = 0, curr_page = 1;

  // skip to our page
  while(*page && curr_page < page_num) {
    if(*page == '\n')
      newline_count++;
    if(newline_count >= NUM_LINES_PER_PAGE) {
      curr_page++;
      newline_count = 0;
    }
    page++;
  }

  // copy our page to the buf
  while(*page && newline_count < NUM_LINES_PER_PAGE) {
    if(*page == '\n')
      newline_count++;
    buf[buf_i] = *page;
    page++;
    buf_i++;
  }

  buf[buf_i] = '\0';
  text_to_buffer(dsock, buf);
  if(dsock->tot_pages != 1)
    send_to_socket(dsock, "{c[{ypage %d of %d. Use MORE to view next page or BACK to view previous{c]{n\r\n",
		   page_num, dsock->tot_pages);
}


void delete_page(SOCKET_DATA *dsock) {
  if(dsock->page_string) free(dsock->page_string);
  dsock->page_string = NULL;
  dsock->tot_pages = 0;
  dsock->curr_page = 0;
}

void  page_string(SOCKET_DATA *dsock, const char *string) {
  if(dsock->page_string) 
    free(dsock->page_string);
  dsock->page_string = strdup(string);
  dsock->tot_pages   = count_pages(string);
  dsock->curr_page   = 1;
  show_page(dsock, 1);
  if(dsock->tot_pages == 1)
    delete_page(dsock);
}

void page_back(SOCKET_DATA *dsock) {
  dsock->curr_page--;
  if(dsock->tot_pages > 0 && dsock->curr_page <= dsock->tot_pages &&
     dsock->curr_page > 0)
    show_page(dsock, dsock->curr_page);
  else {
    dsock->curr_page = MIN(dsock->tot_pages, 1);
    text_to_buffer(dsock, "There is no more text in your page buffer.\r\n");
  }
  /*
  if(dsock->curr_page >= dsock->tot_pages)
    delete_page(dsock);
  */
}

void  page_continue(SOCKET_DATA *dsock) {
  dsock->curr_page++;
  if(dsock->tot_pages > 0 && dsock->curr_page > 0 && 
     dsock->curr_page <= dsock->tot_pages)
    show_page(dsock, dsock->curr_page);
  else {
    dsock->curr_page = dsock->tot_pages;
    text_to_buffer(dsock, "There is no more text in your page buffer.\r\n");
  }
}


//
// the command handler for the reader
void read_handler(SOCKET_DATA *sock, char *input) {
  if(!strncasecmp(input, "more", strlen(input)))
    page_continue(sock);
  else if(!strncasecmp(input, "back", strlen(input)))
    page_back(sock);
  else if(!strncasecmp(input, "quit", strlen(input)))
    socketPopInputHandler(sock);
  else
    text_to_buffer(sock, "Invalid choice!\r\n");
}


//
// the prompt for reading text
void read_prompt(SOCKET_DATA *sock) {
  text_to_buffer(sock, "\r\nQ to stop reading> ");
}

//
// a new handler that allows people to read long bits of text
void  start_reader(SOCKET_DATA *dsock, const char *text) {
  // add a new input handler to control the reading
  socketPushInputHandler(dsock, read_handler, read_prompt);
  
  // page the string
  page_string(dsock, text);
}


void do_copyover(CHAR_DATA *ch) {
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL;
  FILE *fp;
  char buf[100];
  char control_buf[20];
  char port_buf[20];

  if ((fp = fopen(COPYOVER_FILE, "w+")) == NULL) {
    text_to_char(ch, "Copyover file not writeable, aborted.\n\r");
    return;
  }

  sprintf(buf, "\n\r <*>            The world starts spinning             <*>\n\r");

  // For each playing descriptor, save its character and account
  ITERATE_LIST(sock, sock_i) {
    compressEnd(sock, sock->compressing, FALSE);
    // kick off anyone who hasn't yet logged in a character
    if (!socketGetChar(sock) || !charGetRoom(socketGetChar(sock))) {
      text_to_socket(sock, "\r\nSorry, we are rebooting. Come back in a few minutes.\r\n");
      close_socket(sock, FALSE);
    }
    // save account and player info to file
    else {
      fprintf(fp, "%d %s %s %s\n",
	      sock->control, accountGetName(sock->account), 
	      charGetName(sock->player), sock->hostname);
      // save the player
      save_player(sock->player);
      save_account(sock->account);
      text_to_socket(sock, buf);
    }
  } deleteListIterator(sock_i);
  
  fprintf (fp, "-1\n");
  fclose (fp);

  // close any pending sockets
  recycle_sockets();
  
  // exec - descriptors are inherited
  sprintf(control_buf, "%d", control);
  sprintf(port_buf, "%d", mudport);
  execl(EXE_FILE, "NakedMud", "-copyover", control_buf, port_buf, NULL);

  // Failed - sucessful exec will not return
  text_to_char(ch, "Copyover FAILED!\n\r");
}



//*****************************************************************************
// get and set functions
//*****************************************************************************
CHAR_DATA *socketGetChar     ( SOCKET_DATA *dsock) {
  return dsock->player;
}

void       socketSetChar     ( SOCKET_DATA *dsock, CHAR_DATA *ch) {
  dsock->player = ch;
}

ACCOUNT_DATA *socketGetAccount ( SOCKET_DATA *dsock) {
  return dsock->account;
}

void socketSetAccount (SOCKET_DATA *dsock, ACCOUNT_DATA *account) {
  dsock->account = account;
}

BUFFER *socketGetTextEditor   ( SOCKET_DATA *sock) {
  return sock->text_editor;
}

void socketPushInputHandler  ( SOCKET_DATA *socket, 
			       void handler(SOCKET_DATA *socket, char *input),
			       void prompt (SOCKET_DATA *socket)) {
  IH_PAIR *pair = malloc(sizeof(IH_PAIR));
  pair->handler = handler;
  pair->prompt  = prompt;
  listPush(socket->input_handlers, pair);
}

void socketPopInputHandler   ( SOCKET_DATA *socket) {
  IH_PAIR *pair = listPop(socket->input_handlers);
  free(pair);
}

void socketReplaceInputHandler( SOCKET_DATA *socket,
				void handler(SOCKET_DATA *socket, char *input),
				void prompt (SOCKET_DATA *socket)) {
  socketPopInputHandler(socket);
  socketPushInputHandler(socket, handler, prompt);
}

void (*socketGetInputHandler ( SOCKET_DATA *socket))(SOCKET_DATA *, char *) {
  if(listSize(socket->input_handlers) == 0)
    return NULL;
  IH_PAIR *pair = listGet(socket->input_handlers, 0);
  return (pair ? pair->handler : NULL);
}

void socketQueueCommand( SOCKET_DATA *sock, const char *cmd) {
  listQueue(sock->input, strdup(cmd));
}

void socketShowPrompt( SOCKET_DATA *sock) {
  ((IH_PAIR *)listGet(sock->input_handlers, 0))->prompt(sock);
}

void *socketGetAuxiliaryData  ( SOCKET_DATA *sock, const char *name) {
  return hashGet(sock->auxiliary, name);
}

const char *socketGetHostname(SOCKET_DATA *sock) {
  return sock->hostname;
}

sh_int socketGetDNSLookupStatus(SOCKET_DATA *sock) {
  return sock->lookup_status;
}

void socketBustPrompt(SOCKET_DATA *sock) {
  sock->bust_prompt = TRUE;
}




//*****************************************************************************
//
// MCCP SUPPORT IS BELOW THIS LINE. NOTHING BUT MCCP SUPPORT SHOULD GO BELOW
// THIS LINE.
//
//*****************************************************************************
/*
 * mccp.c - support functions for the Mud Client Compression Protocol
 *
 * see http://www.randomly.org/projects/MCCP/
 *
 * Copyright (c) 1999, Oliver Jowett <oliver@randomly.org>
 *
 * This code may be freely distributed and used if this copyright
 * notice is retained intact.
 */
/*
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "mud.h"
#include "socket.h"
#include "utils.h"
*/

/* local functions */
bool  processCompressed       ( SOCKET_DATA *dsock );

const unsigned char enable_compress  [] = { IAC, SB, TELOPT_COMPRESS, WILL, SE, 0 };
const unsigned char enable_compress2 [] = { IAC, SB, TELOPT_COMPRESS2, IAC, SE, 0 };

/*
 * Memory management - zlib uses these hooks to allocate and free memory
 * it needs
 */
void *zlib_alloc(void *opaque, unsigned int items, unsigned int size)
{
  return calloc(items, size);
}

void zlib_free(void *opaque, void *address)
{
  free(address);
}

/*
 * Begin compressing data on `desc'
 */
bool compressStart(SOCKET_DATA *dsock, unsigned char teleopt)
{
  z_stream *s;

  /* already compressing */
  if (dsock->out_compress)
    return TRUE;

  /* allocate and init stream, buffer */
  s = (z_stream *) malloc(sizeof(*s));
  dsock->out_compress_buf = (unsigned char *) malloc(COMPRESS_BUF_SIZE);

  s->next_in    =  NULL;
  s->avail_in   =  0;
  s->next_out   =  dsock->out_compress_buf;
  s->avail_out  =  COMPRESS_BUF_SIZE;
  s->zalloc     =  zlib_alloc;
  s->zfree      =  zlib_free;
  s->opaque     =  NULL;

  if (deflateInit(s, 9) != Z_OK)
  {
    free(dsock->out_compress_buf);
    free(s);
    return FALSE;
  }

  /* version 1 or 2 support */
  if (teleopt == TELOPT_COMPRESS)
    text_to_socket(dsock, (char *) enable_compress);
  else if (teleopt == TELOPT_COMPRESS2)
    text_to_socket(dsock, (char *) enable_compress2);
  else
  {
    bug("Bad teleoption %d passed", teleopt);
    free(dsock->out_compress_buf);
    free(s);
    return FALSE;
  }

  /* now we're compressing */
  dsock->compressing = teleopt;
  dsock->out_compress = s;

  /* success */
  return TRUE;
}

/* Cleanly shut down compression on `desc' */
bool compressEnd(SOCKET_DATA *dsock, unsigned char teleopt, bool forced)
{
  unsigned char dummy[1];

  if (!dsock->out_compress)
    return TRUE;

  if (dsock->compressing != teleopt)
    return FALSE;

  dsock->out_compress->avail_in = 0;
  dsock->out_compress->next_in = dummy;
  dsock->top_output = 0;

  /* No terminating signature is needed - receiver will get Z_STREAM_END */
  if (deflate(dsock->out_compress, Z_FINISH) != Z_STREAM_END && !forced)
    return FALSE;

  /* try to send any residual data */
  if (!processCompressed(dsock) && !forced)
    return FALSE;

  /* reset compression values */
  deflateEnd(dsock->out_compress);
  free(dsock->out_compress_buf);
  free(dsock->out_compress);
  dsock->compressing      = 0;
  dsock->out_compress     = NULL;
  dsock->out_compress_buf = NULL;

  /* success */
  return TRUE;
}

/* Try to send any pending compressed-but-not-sent data in `desc' */
bool processCompressed(SOCKET_DATA *dsock)
{
  int iStart, nBlock, nWrite, len;

  if (!dsock->out_compress)
    return TRUE;
    
  len = dsock->out_compress->next_out - dsock->out_compress_buf;
  if (len > 0)
  {
    for (iStart = 0; iStart < len; iStart += nWrite)
    {
      nBlock = UMIN (len - iStart, 4096);
      if ((nWrite = write(dsock->control, dsock->out_compress_buf + iStart, nBlock)) < 0)
      {
        if (errno == EAGAIN/* || errno == ENOSR*/)
          break;

        /* write error */
        return FALSE;
      }
      if (nWrite <= 0)
        break;
    }

    if (iStart)
    {
      if (iStart < len)
        memmove(dsock->out_compress_buf, dsock->out_compress_buf+iStart, len - iStart);

      dsock->out_compress->next_out = dsock->out_compress_buf + len - iStart;
    }
  }

  /* success */
  return TRUE;
}


//
// compress output
//
COMMAND(cmd_compress)
{
  /* no socket, no compression */
  if (!charGetSocket(ch))
    return;

  /* enable compression */
  if (!charGetSocket(ch)->out_compress) {
    text_to_char(ch, "Trying compression.\n\r");
    text_to_buffer(charGetSocket(ch), (char *) compress_will2);
    text_to_buffer(charGetSocket(ch), (char *) compress_will);
  }
  else /* disable compression */ {
    if (!compressEnd(charGetSocket(ch), charGetSocket(ch)->compressing, FALSE)){
      text_to_char(ch, "Failed.\n\r");
      return;
    }
    text_to_char(ch, "Compression disabled.\n\r");
  }
}
//*****************************************************************************
//
// IF YOU ARE PUTTING ANYTHING BELOW THIS LINE, YOU ARE PUTTING IT IN THE WRONG
// PLACE!! ALL SOCKET-RELATED STUFF SHOULD GO UP ABOVE THE MCCP SUPPORT STUFF!
//
//*****************************************************************************
