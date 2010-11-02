/*
 * This file contains the socket code, used for accepting
 * new connections as well as reading and writing to
 * sockets, and closing down unused sockets.
 */

#include "wrapsock.h"
#include <netdb.h>
#include <sys/ioctl.h>

/* including main header file */
#include "mud.h"
#include "character.h"
#include "utils.h"
#include "socket.h"

#ifdef MODULE_OLC
#include "olc/olc.h"
#endif



/* global variables */
fd_set        fSet;             /* the socket list for polling       */


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

  /*
   * allocate some memory for a new socket if
   * there is no free socket in the free_list
   */
  if( (sock_new = listPop(socket_free)) == NULL) {
    if ((sock_new = malloc(sizeof(*sock_new))) == NULL)
    {
      bug("New_socket: Cannot allocate memory for socket.");
      abort();
    }
    bzero(sock_new, sizeof(*sock_new));    
  }


  /* attach the new connection to the socket list */
  FD_SET(sock, &fSet);

  /* clear out the socket */
  clear_socket(sock_new, sock);

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
  text_to_buffer(sock_new, "Who wants to get naked? ");

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

  /* set the closed state */
  dsock->state = STATE_CLOSED;
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

  // always start with a leading space if we don't need to bust a prompt
  if(dsock->state == STATE_PLAYING && dsock->bust_prompt == FALSE) {
    dsock->outbuf[0] = '\r';
    dsock->outbuf[1] = '\n';
    dsock->top_output = 2;
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
  if (last != -1 || underline || bold) 
  {
    output[iPtr++] =  27; output[iPtr++] = '[';
    output[iPtr++] = '0'; output[iPtr++] = 'm';
  }
  output[iPtr] = '\0';

  /* check to see if the socket can accept that much data */
  if (dsock->top_output + iPtr >= MAX_OUTPUT)
  {
    bug("Text_to_buffer: ouput overflow on %s.", dsock->hostname);
    return;
  }

  /* add data to buffer */
  strcpy(dsock->outbuf + dsock->top_output, output);
  dsock->top_output += iPtr;
}


void next_cmd_from_buffer(SOCKET_DATA *dsock)
{
  int size = 0, i = 0, j = 0, telopt = 0;

  /* if theres already a command ready, we return */
  if (dsock->next_command[0] != '\0')
    return;

  /* if there is nothing pending, then return */
  if (dsock->inbuf[0] == '\0')
    return;

  /* check how long the next command is */
  while (dsock->inbuf[size] != '\0' && dsock->inbuf[size] != '\n' && dsock->inbuf[size] != '\r')
    size++;

  /* we only deal with real commands */
  if (dsock->inbuf[size] == '\0')
    return;

  /* copy the next command into next_command */
  for ( ; i < size; i++)
  {
    if (dsock->inbuf[i] == (signed char) IAC)
    {
      telopt = 1;
    }
    else if (telopt == 1 && (dsock->inbuf[i] == (signed char) DO || dsock->inbuf[i] == (signed char) DONT))
    {
      telopt = 2;
    }
    else if (telopt == 2)
    {
      telopt = 0;

      if (dsock->inbuf[i] == (signed char) TELOPT_COMPRESS)         /* check for version 1 */
      {
        if (dsock->inbuf[i-1] == (signed char) DO)                  /* start compressing   */
          compressStart(dsock, TELOPT_COMPRESS);
        else if (dsock->inbuf[i-1] == (signed char) DONT)           /* stop compressing    */
          compressEnd(dsock, TELOPT_COMPRESS, FALSE);
      }
      else if (dsock->inbuf[i] == (signed char) TELOPT_COMPRESS2)   /* check for version 2 */
      {
        if (dsock->inbuf[i-1] == (signed char) DO)                  /* start compressing   */
          compressStart(dsock, TELOPT_COMPRESS2);
        else if (dsock->inbuf[i-1] == (signed char) DONT)           /* stop compressing    */
          compressEnd(dsock, TELOPT_COMPRESS2, FALSE);
      }
    }
    else if (isprint(dsock->inbuf[i]) && isascii(dsock->inbuf[i]))
    {
      dsock->next_command[j++] = dsock->inbuf[i];
    }
  }
  dsock->next_command[j] = '\0';

  /* skip forward to the next line */
  while (dsock->inbuf[size] == '\n' || dsock->inbuf[size] == '\r')
  {
    dsock->cmd_read = TRUE;
    dsock->bust_prompt = TRUE;   /* seems like a good place to check */
    size++;
  }

  /* use i as a static pointer */
  i = size;

  /* move the context of inbuf down */
  while (dsock->inbuf[size] != '\0')
  {
    dsock->inbuf[size - i] = dsock->inbuf[size];
    size++;
  }
  dsock->inbuf[size - i] = '\0';
}



bool flush_output(SOCKET_DATA *dsock)
{
  /* nothing to send */
  if (dsock->top_output <= 0 && 
      !((dsock->bust_prompt && 
	 (dsock->state == STATE_PLAYING || dsock->state == STATE_TEXT_EDITOR))))
    return TRUE;

  /* bust a prompt */
  if (dsock->state == STATE_PLAYING && dsock->bust_prompt) {
    text_to_buffer(dsock, custom_prompt(dsock->player));
    dsock->bust_prompt = FALSE;
  }
  else if(dsock->state == STATE_TEXT_EDITOR && dsock->bust_prompt) {
    text_to_buffer(dsock, "] ");
    dsock->bust_prompt = FALSE;
  }

  /* reset the top pointer */
  dsock->top_output = 0;

  /*
   * Send the buffer, and return FALSE
   * if the write fails.
   */
  if (!text_to_socket(dsock, dsock->outbuf))
    return FALSE;

  /* Success */
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

void clear_socket(SOCKET_DATA *sock_new, int sock)
{
  if(sock_new->page_string) free(sock_new->page_string);
  if(sock_new->text_editor) buffer_free(sock_new->text_editor);
  if(sock_new->notepad)     free(sock_new->notepad);

  bzero(sock_new, sizeof(*sock_new));
  sock_new->control        =  sock;
  sock_new->state          =  STATE_NEW_NAME;
  sock_new->lookup_status  =  TSTATE_LOOKUP;
  sock_new->player         =  NULL;
  sock_new->top_output     =  0;

  sock_new->text_editor    = NULL;
  sock_new->text_pointer   = NULL;
  sock_new->notepad        = NULL;
  sock_new->in_text_edit   = FALSE;
  sock_new->max_text_len   = 0;

#ifdef MODULE_OLC
  socketSetOLC(sock_new, NULL);
#endif
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

    /* free the memory */
    free(dsock->hostname);

    /* stop compression */
    compressEnd(dsock, dsock->compressing, TRUE);

    /* put the socket in the free_list */
    listPut(socket_free, dsock);
  }
  deleteListIterator(sock_i);
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
  /*
  if(dsock->curr_page >= dsock->tot_pages)
    delete_page(dsock);
  */
}


//*****************************************************************************
//
// get and set functions
//
//*****************************************************************************
sh_int socketGetState       ( SOCKET_DATA *dsock) {
  return dsock->state;
}

CHAR_DATA *socketGetChar     ( SOCKET_DATA *dsock) {
  return dsock->player;
}

#ifdef MODULE_OLC
OLC_DATA *socketGetOLC      ( SOCKET_DATA *dsock) {
  return dsock->olc;
}

void socketSetOLC           ( SOCKET_DATA *dsock, OLC_DATA *olc) {
  if(dsock->olc) deleteOLC(dsock->olc);
  dsock->olc = olc;
}
#endif

void socketSetState         ( SOCKET_DATA *dsock, sh_int state) {
  dsock->state = state;
}

void       socketSetChar     ( SOCKET_DATA *dsock, CHAR_DATA *ch) {
  dsock->player = ch;
}
