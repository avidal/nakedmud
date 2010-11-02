/*
 * This file contains the dynamic help system.
 * If you wish to update a help file, simply edit
 * the entry in ../help/ and the mud will load the
 * new version next time someone tries to access
 * that help file.
 */
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h> 

/* include main header file */
#include "mud.h"
#include "character.h"
#include "help.h"

HELP_DATA   *   help_list = NULL; /* the linked list of help files     */
char        *   greeting;         /* the welcome greeting              */
char        *   motd;             /* the MOTD help file                */

/*
 * Check_help()
 *
 * This function first sees if there is a valid
 * help file in the help_list, should there be
 * no helpfile in the help_list, it will check
 * the ../help/ directory for a suitable helpfile
 * entry. Even if it finds the helpfile in the
 * help_list, it will still check the ../help/
 * directory, and should the file be newer than
 * the currently loaded helpfile, it will reload
 * the helpfile.
 */
bool check_help(CHAR_DATA *dMob, char *helpfile)
{
  HELP_DATA *pHelp;
  char buf[MAX_HELP_ENTRY + 80];
  char *entry, *hFile;
  bool found = FALSE;

  hFile = capitalize(helpfile);

  for (pHelp = help_list; pHelp; pHelp = pHelp->next)
  {
    if (is_prefix(helpfile, pHelp->keyword))
    {
      found = TRUE;
      break;
    }
  }

  /* If there is an updated version we load it */
  if (found)
  {
    if (last_modified(hFile) > pHelp->load_time)
    {
      free(pHelp->text);
      pHelp->text = strdup(read_help_entry(hFile));
    }
  }
  else /* is there a version at all ?? */
  {
    if ((entry = read_help_entry(hFile)) == NULL)
      return FALSE;
    else
    {
      if ((pHelp = malloc(sizeof(*pHelp))) == NULL)
      { 
        bug("Check_help: Cannot allocate memory.");
        abort();
      }
      pHelp->keyword    =  strdup(hFile);
      pHelp->text       =  strdup(entry);
      pHelp->load_time  =  time(NULL);
      add_help(pHelp);
    }
  }

  sprintf(buf, "=== %s ===\n\r%s", pHelp->keyword, pHelp->text);
  //  text_to_char(dMob, buf);
  page_string(charGetSocket(dMob), buf);
  return TRUE;
}

/*
 * Loads all the helpfiles found in ../help/
 */
void load_helps()
{
  HELP_DATA *new_help;
  char buf[MAX_BUFFER];
  char *s;
  DIR *directory;
  struct dirent *entry;

  log_string("Load_helps: getting all help files.");

  directory = opendir("../help/");
  for (entry = readdir(directory); entry; entry = readdir(directory))
  {
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
      continue;

    sprintf(buf, "../help/%s", entry->d_name);
    s = read_help_entry(buf);

    if (s == NULL)
    {
      bug("load_helps: Helpfile %s does not exist.", buf);
      continue;
    }

    if ((new_help = malloc(sizeof(*new_help))) == NULL)
    {
      bug("Load_helps: Cannot allocate memory.");
      abort();
    }

    new_help->keyword    =  strdup(entry->d_name);
    new_help->text       =  strdup(s);
    new_help->load_time  =  time(NULL);
    add_help(new_help);
  }
  closedir(directory);
}

void add_help(HELP_DATA *help)
{
  HELP_DATA *pHelp = NULL;
  HELP_DATA *prev = NULL;
  bool done = FALSE;
  int i;

  if (help_list == NULL)
  {
    help_list = help;
    help->next = NULL;
  }
  else
  {
    for (pHelp = help_list; pHelp; pHelp = pHelp->next)
    {
      if (toupper(help->keyword[0]) > toupper(pHelp->keyword[0]))
      {
        prev = pHelp;
        continue;
      }
      else if (toupper(help->keyword[0]) == toupper(pHelp->keyword[0]))
      {
        for (i = 0; help->keyword[i] != '\0' && pHelp->keyword[i] != '\0'; i++)
        {
          if (toupper(help->keyword[i]) > toupper(pHelp->keyword[i]))
          {
            prev = pHelp;
            break;
          }

          if (help->keyword[i+1] == '\0')
          {
            done = TRUE;
            break;
          }

          /* Helpfile should previous to this helpfile entry */
          if (toupper(help->keyword[i]) == toupper(pHelp->keyword[i]))
          {
            prev = pHelp;
            break;
          }

          /* less than or at the end of the word */
          if (toupper(help->keyword[i]) < toupper(pHelp->keyword[i]))
          {
            done = TRUE;
            break;
          }
        }

        if (!done)
          continue;
      }

      break;
    }

    if (prev == NULL)
    {
      help->next = help_list;
      help_list = help;
    }
    else
    {
      help->next = prev->next;
      prev->next = help;
    }
  }
}
