/*
 * This file handles string copy/search/comparison/etc.
 */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

/* include main header file */
#include "mud.h"
#include "utils.h"

/*
 * Compares two strings, and returns TRUE
 * if they match 100% (not case sensetive).
 */
bool compares(const char *aStr, const char *bStr)
{
  int i = 0;

  /* NULL strings never compares */
  if (aStr == NULL || bStr == NULL) return FALSE;

  while (aStr[i] != '\0' && bStr[i] != '\0' && toupper(aStr[i]) == toupper(bStr[i]))
    i++;

  /* if we terminated for any reason except the end of both strings return FALSE */
  if (aStr[i] != '\0' || bStr[i] != '\0')
    return FALSE;

  /* success */
  return TRUE;
}

/*
 * Checks if aStr is a prefix of bStr.
 */
bool is_prefix(const char *aStr, const char *bStr)
{
  /* NULL strings never compares */
  if (aStr == NULL || bStr == NULL) return FALSE;

  /* empty strings never compares */
  if (aStr[0] == '\0' || bStr[0] == '\0') return FALSE;

  /* check if aStr is a prefix of bStr */
  while (*aStr)
  {
    if (tolower(*aStr++) != tolower(*bStr++))
      return FALSE;
  }

  /* success */
  return TRUE;
}

char *one_arg(char *fStr, char *bStr)
{
  /* skip leading spaces */
  while (isspace(*fStr))
    fStr++; 

  /* copy the beginning of the string */
  while (*fStr != '\0')
  {
    /* have we reached the end of the first word ? */
    if (isspace(*fStr))
    {
      fStr++;
      break;
    }

    /* copy one char */
    *bStr++ = *fStr++;
  }

  /* terminate string */
  *bStr = '\0';

  /* skip past any leftover spaces */
  while (isspace(*fStr))
    fStr++;

  /* return the leftovers */
  return fStr;
}


//
// pull out the argument of the specified number
//
void arg_num(const char *from, char *to, int num) {
  int count = 1;

  while(count < num && *from != '\0') {
    if(isspace(*from))
      count++;
    from++;
  }

  int i;
  // copy up to the first space
  for(i = 0; !isspace(from[i]) && from[i] != '\0'; i++)
    to[i] = from[i];

  // now cap our string
  to[i] = '\0';
}


char *capitalize(char *txt)
{
  static char buf[MAX_BUFFER];
  int size, i;

  buf[0] = '\0';

  if (txt == NULL || txt[0] == '\0')
    return buf;

  size = strlen(txt);

  for (i = 0; i < size; i++)
    buf[i] = toupper(txt[i]);
  buf[size] = '\0';

  return buf;
}

char *strfind(char *txt, char *sub)
{
  int i, j;

  int len = strlen(txt) - strlen(sub);

  for(i = 0; i <= len; i++) {
    if(txt[i] == sub[0]) {
      bool found = TRUE;

      for(j = 1; sub[j] != '\0'; j++) {
	if(sub[j] != txt[i+j]) {
	  found = FALSE;
	  break;
	}
      }

      if(found)
	return &txt[i];

      i += j;
    }
  }

  return NULL;
}

/*  
 * Create a new buffer.
 */
BUFFER *__buffer_new(int size)
{
  BUFFER *buffer;
    
  buffer = malloc(sizeof(BUFFER));
  buffer->size = size;
  buffer->data = malloc(size); *(buffer->data) = '\0';
  buffer->len = 0;
  return buffer;
}

/*
 * Add a string to a buffer. Expand if necessary
 */
void __buffer_strcat(BUFFER *buffer, const char *text)  
{
  int new_size;
  int text_len;
  char *new_data;
 
  /* Adding NULL string ? */
  if (!text)
    return;

  text_len = strlen(text);
    
  /* Adding empty string ? */ 
  if (text_len == 0)
    return;

  /* Will the combined len of the added text and the current text exceed our buffer? */
  if ((text_len + buffer->len + 1) > buffer->size)
  { 
    new_size = buffer->size + text_len + 1;
   
    /* Allocate the new buffer */
    new_data = malloc(new_size);
  
    /* Copy the current buffer to the new buffer */
    memcpy(new_data, buffer->data, buffer->len);
    free(buffer->data);
    buffer->data = new_data;  
    buffer->size = new_size;
  }
  memcpy(buffer->data + buffer->len, text, text_len);
  buffer->len += text_len;
  buffer->data[buffer->len] = '\0';
}

/* free a buffer */
void buffer_free(BUFFER *buffer)
{
  /* Free data */
  free(buffer->data);
 
  /* Free buffer */
  free(buffer);
}

/* Clear a buffer's contents, but do not deallocate anything */
void buffer_clear(BUFFER *buffer)
{
  buffer->len = 0;  
  *buffer->data = '\0';
}

/* print stuff, append to buffer. safe. */
int bprintf(BUFFER *buffer, char *fmt, ...)
{  
  char buf[MAX_BUFFER];
  va_list va;
  int res;
    
  va_start(va, fmt);
  res = vsnprintf(buf, MAX_BUFFER, fmt, va);
  va_end(va);
    
  if (res >= MAX_BUFFER - 1)  
  {
    buf[0] = '\0';
    bug("Overflow when printing string %s", fmt);
  }
  else
    buffer_strcat(buffer, buf);
   
  return res;
}

/* return a pointer to the buffer data */
const char *buffer_string(BUFFER *buffer) {
  return buffer->data;
}

void buffer_format(BUFFER *buffer, bool indent) {
  format_string(&buffer->data, 80, buffer->size, indent);
}
