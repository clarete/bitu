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
#include <errno.h>
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

#define LISTEN_BACKLOG 1

typedef char * (*command_t) (bitu_server_t *, char **, int);

struct _bitu_server
{
  char *sock_path;
  bitu_app_t *app;
  unsigned int sock;
  hashtable_t *commands;
};

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
cmd_load (bitu_server_t *server, char **params, int num_params)
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
  if (bitu_plugin_ctx_load (server->app->plugin_ctx, libname))
    {
      ta_log_info (server->app->logger, "Plugin %s loaded", libname);
      return NULL;
    }
  else
    {
      ta_log_warn (server->app->logger, "Failed to load plugin %s", libname);
      return strdup ("Unable to load module");
    }
}

static char *
cmd_unload (bitu_server_t *server, char **params, int num_params)
{
  char *error;
  if ((error = _validate_num_params ("unload", 1, num_params)) != NULL)
    return error;

  if (bitu_plugin_ctx_unload (server->app->plugin_ctx, params[0]))
    {
      ta_log_info (server->app->logger, "Plugin %s unloaded", params[0]);
      return NULL;
    }
  else
    {
      ta_log_warn (server->app->logger, "Failed to unload plugin %s",
                   params[0]);
      return strdup ("Unable to unload module");
    }
}

static char *
cmd_send (bitu_server_t *server, char **params, int num_params)
{
  const char *jid, *msg;
  iks *xmpp_msg;
  char *error;

  if ((error = _validate_num_params ("send", 2, num_params)) != NULL)
    return error;

  jid = params[0];
  msg = params[1];

  xmpp_msg = iks_make_msg (IKS_TYPE_CHAT, jid, msg);
  ta_xmpp_client_send (server->app->xmpp, xmpp_msg);
  iks_delete (xmpp_msg);
  return NULL;
}

/* Public API */

bitu_server_t *
bitu_server_new (const char *sock_path, bitu_app_t *app)
{
  bitu_server_t *server;
  if ((server = malloc (sizeof (bitu_server_t))) == NULL)
    return NULL;
  server->sock_path = strdup (sock_path);
  server->app = app;
  server->commands = hashtable_create (hash_string, string_equal, NULL, NULL);

  /* Registering commands */
  hashtable_set (server->commands, "load", cmd_load);
  hashtable_set (server->commands, "unload", cmd_unload);
  hashtable_set (server->commands, "send", cmd_send);
  return server;
}

void
bitu_server_free (bitu_server_t *server)
{
  hashtable_destroy (server->commands);
  free (server->sock_path);
  free (server);
}

int
bitu_server_connect (bitu_server_t *server)
{
  int len;
  struct sockaddr_un local;

  if ((server->sock = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
      ta_log_critical (server->app->logger, "Unable to open socket server");
      return 0;
    }

  local.sun_family = AF_UNIX;
  strcpy (local.sun_path, server->sock_path);
  unlink (local.sun_path);

  len = strlen (local.sun_path) + sizeof (local.sun_family);
  if (bind (server->sock, (struct sockaddr *) &local, len) == -1)
    {
      ta_log_critical (server->app->logger,
                       "Error when binding to socket %s: %s",
                       server->sock_path,
                       strerror (errno));
      return 0;
    }
  if (listen (server->sock, LISTEN_BACKLOG) == -1)
    {
      ta_log_critical (server->app->logger,
                       "Error to listen to connections: %s",
                       strerror (errno));
      return 0;
    }

  ta_log_info (server->app->logger, "Local server is waiting for connections");
  return 1;
}

void
bitu_server_run (bitu_server_t *server)
{
  unsigned int sock;
  struct sockaddr_un remote;

  while (1)
    {
      int done = 0;
      socklen_t len;
      char *cmd = NULL;
      char **params = NULL;
      int num_params;

      len = sizeof (struct sockaddr_un);
      sock = accept (server->sock, (struct sockaddr *) &remote, &len);
      ta_log_info (server->app->logger, "Client connected");

      while (!done)
        {
          int n, msg_size, msgbufsize = 128;
          char str[100];
          char *answer;
          command_t command;

          n = recv (sock, str, 100, 0);
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
              ta_log_warn (server->app->logger, answer);
            }
          else if ((command = hashtable_get (server->commands, cmd)) == NULL)
            {
              msg_size =
                snprintf (answer, msgbufsize, "Command `%s' not found", cmd);
              ta_log_warn (server->app->logger, answer);
            }
          /*
          else if (command.num_params != num_params)
            {
              msg_size =
                snprintf (answer, msgbufsize,
                          "Command `%s' waits for %d args but %d were passed",
                          cmd, command.num_params, num_params);
              ta_log_warn (server->app->logger, answer);
            }
          */
          else
            {
              answer = command (server, params, num_params);
              if (answer != NULL)
                msg_size = strlen (answer);
              else
                {
                  answer = malloc (1);
                  memset (answer, 0, 1);
                  msg_size = 1;
                }
            }
          send (sock, answer, msg_size, 0);
          if (answer)
            free (answer);
        }
    }
}
