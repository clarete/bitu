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
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <bitu/util.h>

#define SOCKET_PATH    "/tmp/bitu.sock"
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

static char *
_escape_param (const char *param, int *len)
{
  char *ret, *s;
  char c;
  int i = 0;

  if ((ret = malloc (strlen (param) * 2)) == NULL)
    return NULL;

  s = (char *) param;

  while ((c = *s++))
    {
      if (c == ' ' || c == '\"')
        ret[i++] = '\\';
      ret[i++] = c;
    }
  ret[i] = '\0';
  if (len != NULL)
    *len = i;
  return ret;
}

int
main (int argc, char **argv)
{
  int i, running = 1;
  unsigned int s;
  struct sockaddr_un remote;
  socklen_t len;
  char *socket_path = NULL;
  int c;
  static struct option long_options[] = {
    { "server-socket", required_argument, NULL, 's' },
    { 0, 0, 0, 0 }
  };

  while ((c = getopt_long (argc, argv, "s:", long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 's':
          socket_path = optarg;
          break;

        default:
          fprintf (stderr, "Try `%s --help' for more information\n", argv[0]);
          break;
        }
    }

  if (socket_path == NULL)
    socket_path = SOCKET_PATH;

  if ((s = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
      perror ("socket");
      exit (EXIT_FAILURE);
    }

  remote.sun_family = AF_UNIX;
  strcpy (remote.sun_path, socket_path);
  len = strlen (remote.sun_path) + sizeof (remote.sun_family);
  if (connect (s, (struct sockaddr *) &remote, len) == -1)
    {
      fprintf (stderr, "Error when connecting: %s\n", strerror (errno));
      return EXIT_FAILURE;
    }

  /* Interactive shell only appears when no param is received */
  if (argc > 1)
    {
      /* Collecting the command and its parameters to exec them on the
       * server and then feed back the user. */
      char *cmd = NULL, *cmdline = NULL;
      char *param = NULL, *tmp = NULL;
      int i, nparams;
      int cmd_len, full_len;
      int allocated = 0;
      int bufsize = 128;

      cmd = argv[1];
      nparams = argc - 2;
      cmd_len = strlen (cmd);
      full_len = cmd_len;

      allocated = bufsize;
      cmdline = malloc (bufsize);
      memcpy (cmdline, cmd, cmd_len);

      cmdline[cmd_len] = ' ';
      full_len++;

      if (argc > 2)
        {
          int plen;

          /* Yes, we have params to collect. */
          for (i = 2; i < argc; i++)
            {
              /* The `_escape_param()' also returns the size of the
               * string. */
              param = _escape_param (argv[i], &plen);

              /* Allocatting more space if it's needed */
              if (full_len + plen > allocated)
                {
                  if ((tmp = realloc (cmdline, allocated + bufsize)) == NULL)
                    {
                      free (cmdline);
                      close (s);
                      exit (EXIT_FAILURE);
                    }
                  else
                    {
                      cmdline = tmp;
                      allocated += bufsize;
                    }
                }

              /* Copying parameter to the command line string */
              memcpy (cmdline + full_len, param, plen);

              /* this +1 is the space that will be added soon */
              full_len += plen; // + 1;
              cmdline[full_len] = ' ';
              full_len++;

              free (param);
            }
        }
      cmdline[--full_len] = '\0';

      /* Sending the command to the server */
      if (send (s, cmdline, full_len, 0) == -1)
        {
          perror ("send");
          exit (EXIT_FAILURE);
        }
      free (cmdline);

      /* Receiving the answer from the server */
      if (running)
        {
          int n;
          char str[100];
          n = recv (s, str, 100, 0);
          if (n > 1)
            {
              str[n] = '\0';
              printf ("%s\n", str);
            }
        }

      goto finalize;
    }

  printf ("bitU interactive shell connected!\n");
  printf ("Type `help' for more information\n");

  while (running)
    {
      char *line;
      char *line_stripped;
      char str[100];
      ssize_t n;

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
        continue;
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

      /* Receiving the answer from the server */
      if (running)
        {
          n = recv (s, str, 100, 0);
          if (n > 1)
            {
              str[n] = '\0';
              printf ("%s\n", str);
            }
        }
    }

 finalize:
  close (s);
  return 0;
}
