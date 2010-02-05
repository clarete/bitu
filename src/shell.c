/* shell.c - This file is part of the bitu program
 *
 * Copyright (C) 2010  Lincoln de Sousa <lincoln@comum.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <bitu/util.h>

#define SOCKET_PATH    "/tmp/server.sock"
#define PS1            "> "

struct sh_command
{
  const char *name;
  int (*handler) (const char *line);
};

static int
sh_help (const char *line __attribute__ ((unused)))
{
  printf ("Available commands:\n");
  printf (" available\t- List available modules\n");
  printf (" enabled\t- List loaded modules\n");
  printf (" load\t\t- Loads an available module into the running client\n");
  printf (" unload\t\t- Unloads a loaded module from the running client\n");
  return 0;
}

static int
sh_exit (const char *line __attribute__ ((unused)))
{
  return 1;
}

static struct sh_command commands[] = {
  { "help", sh_help },
  { "exit", sh_exit },
  { NULL, NULL }
};

int
main (int argc, char **argv)
{
  int i, running = 1;
  unsigned int s;
  struct sockaddr_un remote;
  socklen_t len;

  if ((s = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
      perror ("socket");
      exit (EXIT_FAILURE);
    }

  remote.sun_family = AF_UNIX;
  strcpy (remote.sun_path, SOCKET_PATH);
  len = strlen (remote.sun_path) + sizeof (remote.sun_family);
  if (connect (s, (struct sockaddr *) &remote, len) == -1)
    {
      fprintf (stderr, "Error: %s\n", strerror (errno));
      return EXIT_FAILURE;
    }

  printf ("bitU interactive shell connected!\n");
  printf ("Type `help' for more information\n");

  while (running)
    {
      char *line;
      char *line_stripped;

      /* Main readline call.*/
      line = readline (PS1);

      if (line == NULL)
        {
          printf ("\n");
          break;
        }

      /* avoid losing the right pointer to free */
      line_stripped = bitu_util_strstrip (line);

      if (strlen (line_stripped) == 0)
        break;
      else
        {
          /* strtok modifies its first param */
          char *line_tok = strdup (line_stripped);
          char *cmd = strtok (line_tok, " ");
          char *params = strtok (NULL, "");
          int len, found = 0;

          /* Let's look for an available command */
          for (i = 0; commands[i].name; i++)
            {
              if (strcmp (commands[i].name, cmd) == 0)
                {
                  found = 1;
                  if (commands[i].handler (params))
                    running = 0;
                }
            }
          if (!found)
            {
              len = strlen (line_stripped);
              if (send (s, line_stripped, len, 0) == -1)
                {
                  perror ("send");
                  exit (EXIT_FAILURE);
                }
            }
          free (line_tok);
        }
      free (line);
    }

  close (s);
  return 0;
}
