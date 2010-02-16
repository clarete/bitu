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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <taningia/taningia.h>
#include <bitu/app.h>
#include <bitu/server.h>
#include <bitu/util.h>

#include "hashtable.h"
#include "hashtable-utils.h"

#define SOCKET_PATH    "/tmp/server.sock"
#define LISTEN_BACKLOG 1

typedef char * (*command_t) (bitu_app_t *, char **, int);

static char *
_validate_num_params (const char *cmd, int x, int y)
{
  if (x != y)
    {
      char *error = malloc (128);
      snprintf (error, 128, "Command `%s' takes %d param(s), %d given",
                cmd, x, y);
      return error;
    }
  return NULL;
}

static char *
cmd_load (bitu_app_t *app, char **params, int num_params)
{
  size_t fullsize;
  char *libname;
  char *error;

  if ((error = _validate_num_params ("load", 1, num_params)) != NULL)
    return error;

  fullsize = strlen (params[0]) + 7; /* lib${bleh}.so\0 */
  if ((libname = malloc (fullsize)) == NULL)
    return NULL;

  snprintf (libname, fullsize, "lib%s.so", params[0]);
  if (bitu_plugin_ctx_load (app->plugin_ctx, libname))
    {
      ta_log_info (app->logger, "Plugin %s loaded", libname);
      return NULL;
    }
  else
    {
      ta_log_warn (app->logger, "Failed to load plugin %s", libname);
      return strdup ("Unable to load module");
    }
}

static char *
cmd_unload (bitu_app_t *app, char **params, int num_params)
{
  char *error;
  if ((error = _validate_num_params ("unload", 1, num_params)) != NULL)
    return error;

  if (bitu_plugin_ctx_unload (app->plugin_ctx, params[0]))
    {
      ta_log_info (app->logger, "Plugin %s unloaded", params[0]);
      return NULL;
    }
  else
    {
      ta_log_warn (app->logger, "Failed to unload plugin %s", params[0]);
      return strdup ("Unable to unload module");
    }
}

void
bitu_server_run (bitu_app_t *app)
{
  int len;
  unsigned int s, s2;
  struct sockaddr_un local, remote;
  hashtable_t *commands;

  commands = hashtable_create (hash_string, string_equal, NULL, NULL);
  hashtable_set (commands, "load", cmd_load);
  hashtable_set (commands, "unload", cmd_unload);

  if ((s = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
      ta_log_critical (app->logger, "Unable to open socket server");
      exit (EXIT_FAILURE);
    }

  local.sun_family = AF_UNIX;
  strcpy (local.sun_path, SOCKET_PATH);
  unlink (local.sun_path);

  len = strlen (local.sun_path) + sizeof (local.sun_family);
  bind (s, (struct sockaddr *) &local, len);
  listen (s, LISTEN_BACKLOG);

  ta_log_info (app->logger, "Local server is waiting for connections");

  while (1)
    {
      int done = 0;
      socklen_t len;
      char *cmd = NULL;
      char **params = NULL;
      int num_params;

      len = sizeof (struct sockaddr_un);
      s2 = accept (s, (struct sockaddr *) &remote, &len);
      ta_log_info (app->logger, "Client connected");

      while (!done)
        {
          int n, msg_size, msgbufsize = 128;
          char str[100];
          char *answer;
          command_t command;

          n = recv (s2, str, 100, 0);
          if (n <= 0)
            {
              done = 1;
              break;
            }
          str[n] = '\0';

          answer = malloc (msgbufsize);
          if (!bitu_util_extract_params (str, &cmd, &params, &num_params))
            {
              msg_size =
                snprintf (answer, msgbufsize, "Command seems to be empty");
              ta_log_warn (app->logger, answer);
            }
          else if ((command = hashtable_get (commands, cmd)) == NULL)
            {
              msg_size =
                snprintf (answer, msgbufsize, "Command `%s' not found", cmd);
              ta_log_warn (app->logger, answer);
            }
          /*
          else if (command.num_params != num_params)
            {
              msg_size =
                snprintf (answer, msgbufsize,
                          "Command `%s' waits for %d args but %d were passed",
                          cmd, command.num_params, num_params);
              ta_log_warn (app->logger, answer);
            }
          */
          else
            {
              answer = command (app, params, num_params);
              if (answer != NULL)
                msg_size = strlen (answer);
              else
                {
                  answer = malloc (1);
                  memset (answer, 0, 1);
                  msg_size = 1;
                }
            }
          send (s2, answer, msg_size, 0);
          if (answer)
            free (answer);
        }
    }
}
