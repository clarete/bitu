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
#include <sys/select.h>
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
  int (*handler) (char **, int);
  int num_params;
};

static int
sh_exit (char **params, int num_params)
{
  return 1;
}

static struct sh_command commands[] = {
  { "exit", sh_exit, 0 },
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

static int
_shell_recv (int sock, char *buf, size_t bufsize, int timeout)
{
  int n, r;
  fd_set fds;
  struct timeval tv, *tvptr;

  if (timeout == -1)
    tvptr = NULL;
  else
    {
      tv.tv_sec = 0;
      tv.tv_usec = timeout;
      tvptr = &tv;
    }

  FD_ZERO (&fds);
  FD_SET (sock, &fds);
  r = select (sock + 1, &fds, NULL, NULL, tvptr);
  if (r == -1 && errno == EAGAIN)
    return 0;
  else if (r == -1)
    {
      printf ("Error in select(): %s\n", strerror (errno));
      return -1;
    }
  if (FD_ISSET (sock, &fds))
    {
      do
        n = recv (sock, buf, bufsize, 0);
      while (n == -1 && (errno == EAGAIN));
      if (n > 0)
        return n;
      else if (n < 0)
        {
          printf ("Error in recv(): %s\n", strerror (errno));
          return -1;
        }
    }
  return 0;
}

int
bitu_shell_recv (int sock)
{
  int full_size = 0;
  int n, hasdata = 0;
  int bufsize = 128;
  char buf[bufsize];
  int timeout = 1000;
  while (1)
    {
      n = _shell_recv (sock, buf, bufsize, timeout);
      if (n < 0)        /* Something wrong happened */
        break;
      if (n == 0)       /* End of data */
        break;
      if (n == 1)       /* Nothing to be printed out */
        break;
      buf[n] = '\0';
      printf (buf);
      hasdata = 1;
      full_size += n;
      timeout = 0;
    }
  if (hasdata)
    printf ("\n");
  return full_size;
}


int
main (int argc, char **argv)
{
  int i, running = 1;
  unsigned int s;
  struct sockaddr_un remote;
  socklen_t len;
  char *socket_path = NULL;
  int arglen = argc - 1;
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
          arglen--;
          break;

        default:
          fprintf (stderr, "Try `%s --help' for more information\n", argv[0]);
          exit (EXIT_FAILURE);
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
  if (arglen > 0)
    {
      /* Collecting the command and its parameters to exec them on the
       * server and then feed back the user. */
      char *cmd = NULL, *cmdline = NULL;
      char *param = NULL, *tmp = NULL;
      int cmd_len, full_len;
      int allocated = 0;
      int bufsize = 128;
      int start_pos = argc - arglen;
      int plen, i;

      cmd = argv[start_pos];
      cmd_len = strlen (cmd);
      full_len = cmd_len;

      allocated = bufsize;
      cmdline = malloc (bufsize);
      memcpy (cmdline, cmd, cmd_len);

      cmdline[cmd_len] = ' ';
      full_len++;

      /* Yes, we have params to collect. */
      for (i = 1; i < arglen; i++)
        {
          /* The `_escape_param()' also returns the size of the
           * string. */
          param = _escape_param (argv[start_pos + i], &plen);

          /* Allocatting more space if it's needed */
          if ((full_len + plen) > allocated)
            {
              while (allocated < full_len + plen)
                allocated += bufsize;
              if ((tmp = realloc (cmdline, allocated)) == NULL)
                {
                  free (cmdline);
                  close (s);
                  exit (EXIT_FAILURE);
                }
              else
                cmdline = tmp;
            }

          /* Copying parameter to the command line string */
          memcpy (cmdline + full_len, param, plen);

          /* Adding the space after param */
          full_len += plen;
          cmdline[full_len] = ' ';
          full_len++;

          free (param);
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
        bitu_shell_recv (s);
      goto finalize;
    }

  printf ("bitU interactive shell connected!\n");
  printf ("Type `help' for more information\n");

  while (running)
    {
      char *line;
      char *cmd;
      char **params;
      int num_params;

      /* Main readline call.*/
      line = readline (PS1);

      if (line == NULL)
        {
          printf ("\n");
          break;
        }

      if (!bitu_util_extract_params (line, &cmd, &params, &num_params))
        {
          printf ("Command seems to be empty\n");
          continue;
        }
      else
        {
          /* Let's look for local commands */
          for (i = 0; commands[i].name; i++)
            if (strcmp (commands[i].name, cmd) == 0)
              if (commands[i].handler (params, num_params))
                running = 0;

          if (running)
            {
              int sent;
              len = strlen (line);
              sent = send (s, line, len, 0);
              if (sent == -1)
                {
                  fprintf (stderr, "Error in send(): %s\n",
                           strerror (errno));
                  goto finalize;
                }
            }
        }
      free (line);

      /* Receiving the answer from the server */
      if (running)
        bitu_shell_recv (s);
    }

 finalize:
  close (s);
  return 0;
}
