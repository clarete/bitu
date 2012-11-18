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
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <taningia/taningia.h>
#include <bitu/app.h>
#include <bitu/server.h>
#include <bitu/util.h>

#include "server-priv.h"
#include "hashtable.h"
#include "hashtable-utils.h"

#define LISTEN_BACKLOG 1

/* signal handler stuff */

static void
_signal_handler (int sig, siginfo_t *si, void *data)
{
  switch (sig)
    {
    case SIGPIPE:
      /* Doing nothing here. Just avoiding the default behaviour that
       * kills the program. */
      break;

      /* Add more signals here to handle them when needed. */

    default:
      break;
    }
}

static void
_setup_sigaction (bitu_server_t *server)
{
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = _signal_handler;
  sigemptyset (&sa.sa_mask);

  if (sigaction (SIGPIPE, &sa, NULL) == -1)
    ta_log_warn (server->app->logger,
                 "Unable to install sigaction to catch SIGPIPE");
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
  server->env = hashtable_create (hash_string, string_equal, free, free);
  server->can_run = 1;

  /* Registering commands */
  _server_register_commands (server);
  _setup_sigaction (server);
  return server;
}

void
bitu_server_free (bitu_server_t *server)
{
  ta_log_info (server->app->logger, "Gracefully exiting, see you!");

  /* Cleaning up app properties */
  if (server->app->plugin_ctx)
    {
      bitu_plugin_ctx_free (server->app->plugin_ctx);
      server->app->plugin_ctx = NULL;
    }
  if (server->app->xmpp)
    {
      ta_object_unref (server->app->xmpp);
      server->app->xmpp = NULL;
    }
  if (server->app->logger)
    {
      ta_object_unref (server->app->logger);
      server->app->logger = NULL;
    }
  if (server->app->logfile)
    {
      free (server->app->logfile);
      server->app->logfile = NULL;
    }
  if (server->app->logfd > -1)
    {
      close (server->app->logfd);
      server->app->logfd = -1;
    }
  if (server->app)
    {
      free (server->app);
      server->app = NULL;
    }

  /* Cleaning server attrs */
  hashtable_destroy (server->commands);
  hashtable_destroy (server->env);
  server->can_run = 0;
  close (server->sock);
  unlink (server->sock_path);
  free (server->sock_path);
  free (server);
}

bitu_app_t *
bitu_server_get_app (bitu_server_t *server)
{
  return server->app;
}

int
bitu_server_connect (bitu_server_t *server)
{
  int len;
  struct sockaddr_un local;

  if ((server->sock = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
      ta_log_critical (server->app->logger, "Unable to open socket server");
      return TA_ERROR;
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
      return TA_ERROR;
    }
  if (listen (server->sock, LISTEN_BACKLOG) == -1)
    {
      ta_log_critical (server->app->logger,
                       "Error to listen to connections: %s",
                       strerror (errno));
      return TA_ERROR;
    }

  ta_log_info (server->app->logger,
               "Local server bound to the socket in %s",
               server->sock_path);
  return TA_OK;
}

char *
bitu_server_exec_cmd (bitu_server_t *server, const char *cmd,
                      char **params, int nparams, int *answer_size)
{
  char *answer = NULL;
  int msg_size, msgbufsize = 128;
  command_t command;

  answer = malloc (msgbufsize);
  if ((command = hashtable_get (server->commands, cmd)) == NULL)
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
      ta_log_debug (server->app->logger, "Running command `%s'", cmd);
      answer = command (server, params, nparams);
      if (answer != NULL)
        msg_size = strlen (answer);
      else
        {
          answer = malloc (1);
          memset (answer, 0, 1);
          msg_size = 1;
        }
    }

  if (answer_size)
    *answer_size = msg_size;
  return answer;
}

char *
bitu_server_exec_cmd_line (bitu_server_t *server, const char *cmdline)
{
  char *cmd = NULL, *answer = NULL;
  char **params = NULL;
  int msgbufsize = 128;
  int num_params;

  answer = malloc (msgbufsize);
  if (cmdline == NULL)
    {
      snprintf (answer, msgbufsize, "Empty command line");
      ta_log_warn (server->app->logger, answer);
    }
  else
    {
      if (!bitu_util_extract_params (cmdline, &cmd, &params, &num_params))
        {
          snprintf (answer, msgbufsize, "Command seems to be empty");
          ta_log_warn (server->app->logger, answer);
        }
      else
        {
          free (answer);
          answer = bitu_server_exec_cmd (server, cmd, params,
                                         num_params, NULL);
        }
    }
  return answer;
}

char *
bitu_server_exec_plugin (bitu_server_t *server, const char *plugin_name,
                         char **params, int nparams, int *answer_size)
{
  bitu_plugin_ctx_t *plugin_ctx;
  bitu_plugin_t *plugin;
  int msgbufsize = 128;
  char *message = NULL;

  plugin_ctx = server->app->plugin_ctx;

  if ((plugin = bitu_plugin_ctx_find (plugin_ctx, plugin_name)) == NULL)
    {
      message = malloc (msgbufsize);
      snprintf (message, msgbufsize, "Plugin `%s' not found", plugin_name);
    }
  else
    {
      /* Validating number of parameters */
      if (bitu_plugin_num_params (plugin) != nparams)
        {
          message = malloc (msgbufsize);
          snprintf (message, msgbufsize,
                    "Wrong number of parameters. `%s' "
                    "receives %d but %d were passed", plugin_name,
                    bitu_plugin_num_params (plugin),
                    nparams);
        }
      else
        message = bitu_plugin_execute (plugin, params);
    }
  return message;
}


int
bitu_server_recv (bitu_server_t *server, int sock,
                  char *buf, size_t bufsize, int timeout)
{
  int n, r;
  fd_set fds;
  struct timeval tv, *tvptr;

  FD_ZERO (&fds);
  FD_SET (sock, &fds);

  if (timeout == -1)
    tvptr = NULL;
  else
    {
      tv.tv_sec = 0;
      tv.tv_usec = timeout;
      tvptr = &tv;
    }

  r = select (sock + 1, &fds, NULL, NULL, tvptr);
  if (r == -1 && (errno == EAGAIN || errno == EINTR))
    return 0;
  else if (r == -1)
    {
      ta_log_error (server->app->logger, "Error in select(): %s",
                    strerror (errno));
      return -1;
    }
  if (FD_ISSET (sock, &fds))
    {
      do
        n = recv (sock, buf, bufsize, 0);
      while (n == -1 && (errno == EAGAIN));
      if (n < 0)
        {
          ta_log_error (server->app->logger, "Error in recv(): %s",
                        strerror (errno));
          return -1;
        }
      else
        return n;
    }
  return 0;
}

int
bitu_server_send (bitu_server_t *server, int sock,
                  char *buf, size_t bufsize)
{
  int n;
  size_t sent = 0;

  while (sent < bufsize)
    {
      n = send (sock, buf, bufsize, 0);
      if (n == -1 && errno == EAGAIN)
        continue;
      if (n == -1)
        {
          ta_log_error (server->app->logger,
                        "Error in send(): %s",
                        strerror (errno));
          return -1;
        }
      sent += n;
    }
  return 0;
}

void
bitu_server_run (bitu_server_t *server)
{
  int sock;
  struct sockaddr_un remote;

  while (server->can_run)
    {
      socklen_t len;
      char *str = NULL, *answer = NULL;
      int full_len, allocated;
      int timeout;

      len = sizeof (struct sockaddr_un);

      /* EINTR means the the resource is not ready, so we should try
       * agian */
      do
        sock = accept (server->sock, (struct sockaddr *) &remote, &len);
      while (sock == -1 && errno == EINTR);

      /* Unlucky, some thing is wrong. Report and fail */
      if (sock == -1)
        {
          ta_log_error (server->app->logger,
                        "Error in accept(): %s",
                        strerror (errno));
          continue;
        }
      ta_log_info (server->app->logger, "Client connected");

      while (1)
        {
          /* Initializing variables that are going to be used when
           * allocating space to store the whole command received via
           * socket. */
          full_len = allocated = 0;
          str = NULL;
          timeout = -1;

          while (1)
            {
              int n, bufsize = 128;
              char buf[bufsize];
              n = bitu_server_recv (server, sock, buf, bufsize, timeout);
              ta_log_debug (server->app->logger, "recv() returned: %d", n);
              if (n == 0)
                {
                  ta_log_info (server->app->logger, "End of client stream");
                  break;
                }
              if (n < 0)
                break;
              buf[n] = '\0';
              if ((full_len + n) > allocated)
                {
                  char *tmp;
                  while (allocated < (full_len + n))
                    allocated += bufsize;
                  if ((tmp = realloc (str, allocated)) == NULL)
                    {
                      if (str)
                        free (str);
                      break;
                    }
                  else
                    str = tmp;
                }
              memcpy (str + full_len, buf, n);
              full_len += n;
              timeout = 0;
            }
          if (str)
            str[full_len] = '\0';

          /* Client is saying good bye, it means that no command
           * should be run. It is time to get out */
          if ((str == NULL) ||
              (str && strcmp (str, "exit") == 0) ||
              (str && strlen (str) < 2))
            {
              close (sock);
              break;
            }

          /* Finally we try to execute the received command. */
          if ((answer = bitu_server_exec_cmd_line (server, str)) != NULL)
            {
              int ret;
              ret = bitu_server_send (server, sock, answer, strlen (answer));
              if (answer)
                free (answer);
              if (ret == -1)
                break;
            }
          bitu_server_send (server, sock, "\0", 1);
        }
    }
}
