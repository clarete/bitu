/* server.c - This file is part of the bitu program
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
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <bitu/server.h>

#define SOCKET_PATH    "/tmp/server.sock"
#define LISTEN_BACKLOG 1

void
bitu_server_run (void)
{
  int len;
  unsigned int s, s2;
  struct sockaddr_un local, remote;

  if ((s = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
      perror ("socket");
      exit (EXIT_FAILURE);
    }

  local.sun_family = AF_UNIX;
  strcpy (local.sun_path, SOCKET_PATH);
  unlink (local.sun_path);

  len = strlen (local.sun_path) + sizeof (local.sun_family);
  bind (s, (struct sockaddr *) &local, len);
  listen (s, LISTEN_BACKLOG);

  printf (":: Waiting for connections\n");

  while (1)
    {
      int done = 0;
      socklen_t len;

      len = sizeof (struct sockaddr_un);
      s2 = accept (s, (struct sockaddr *) &remote, &len);
      printf (" * Client connected\n");

      while (!done)
        {
          int n;
          char str[100];
          n = recv (s2, str, 100, 0);
          if (n <= 0)
            {
              done = 1;
              break;
            }

          /*
          if (!done)
            send (s2, "oi", 2, 0);
          */

          str[n] = '\0';
          printf ("Command received (%d): %s\n", n, str);
        }
    }
}
