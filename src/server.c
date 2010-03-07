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
  int can_run;
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
_validate_min_num_params (const char *cmd, int x, int y)
{
  if (y < x)
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

static char *
cmd_list (bitu_server_t *server, char **params, int num_params)
{
  char *action, *ret;
  char *error;
  if ((error = _validate_num_params ("list", 1, num_params)) != NULL)
    return error;

  ret = NULL;
  action = params[0];
  if (strcmp (action, "plugins") == 0)
    {
      ta_list_t *plugins, *tmp;
      char *val, *tmp_val, *current_pos_str;
      size_t val_size, current_pos, full_size = 0;
      plugins = bitu_plugin_ctx_get_list (server->app->plugin_ctx);
      for (tmp = plugins; tmp; tmp = tmp->next)
        {
          val = tmp->data;

          /* This +1 means the \n at the end of each line. */
          val_size = strlen (val) + 1;

          /* Remembering current end of the full string. */
          current_pos = full_size;
          full_size += val_size;

          if ((tmp_val = realloc (ret, full_size)) == NULL)
            {
              free (ret);
              return NULL;
            }
          else
            ret = tmp_val;

          current_pos_str = ret + current_pos;
          memcpy (current_pos_str, val, val_size);
          memcpy (current_pos_str + val_size - 1, "\n", 1);
        }

      /* Removing the last \n. It is not needed in the end of the
       * string */
      memcpy (ret + full_size - 1, "\b", 1);
      ta_list_free (plugins);
    }
  else if (strcmp (action, "commands") == 0)
    {
      void *iter;
      char *val, *tmp, *current_pos_str;
      size_t val_size, current_pos, full_size = 0;
      iter = hashtable_iter (server->commands);
      do
        {
          val = hashtable_iter_key (iter);

          /* This +1 means the \n at the end of each line. */
          val_size = strlen (val) + 1;

          /* Remembering current end of the full string. */
          current_pos = full_size;
          full_size += val_size;

          if ((tmp = realloc (ret, full_size)) == NULL)
            {
              free (ret);
              return NULL;
            }
          else
            ret = tmp;

          current_pos_str = ret + current_pos;
          memcpy (current_pos_str, val, val_size);
          memcpy (current_pos_str + val_size - 1, "\n", 1);
        }
      while ((iter = hashtable_iter_next (server->commands, iter)));

      /* Removing the last \n. It is not needed in the end of the string */
      ret[full_size-1] = '\0';
    }
  return ret;
}

static int
_log_handler (ta_log_t *log, ta_log_level_t level,
              const char *data, void *user_data)
{
  bitu_server_t *server = (bitu_server_t *) user_data;
  write (server->app->logfd, data, strlen (data));
  write (server->app->logfd, "\n", 1);
  return 0;
}

static char *
cmd_set_log_file (bitu_server_t *server, char **params, int nparams)
{
  char *ret = NULL;
  char *error, *logfile;
  int logfd;

  if ((error = _validate_num_params ("set-log-file", 1, nparams)) != NULL)
    return error;

  /* Cleaning possible old values */

  logfile = strdup (params[0]);
  logfd = open (logfile,
                O_WRONLY | O_CREAT | O_APPEND,
                S_IRUSR | S_IWUSR | S_IRGRP);

  if (logfd == -1)
    {
      int size;
      size_t bufsize = 128;
      error = malloc (bufsize);
      size = snprintf (error, bufsize,
                       "Unable to open file log file `%s': %s",
                       server->app->logfile, strerror (errno));
      ta_log_error (server->app->logger, error);
      return error;
    }
  if (server->app->logfile)
    free (server->app->logfile);
  if (server->app->logfd > -1)
    close (server->app->logfd);

  server->app->logfile = logfile;
  server->app->logfd = logfd;

  ta_log_info (server->app->logger, "Setting log file to %s",
               server->app->logfile);
  ta_log_set_handler (server->app->logger,
                      (ta_log_handler_func_t) _log_handler,
                      server);

  return ret;
}

static char *
cmd_set_log_level (bitu_server_t *server, char **params, int nparams)
{
  char *tok = NULL;
  char *error = NULL;
  int flags = 0, i = 0;
  ta_log_t *logger;

  if ((error = _validate_min_num_params ("set-log-level", 1, nparams)) != NULL)
    return error;
  for (i = 0; i < nparams; i++)
    {
      tok = params[i];

      if (strcmp (tok, "DEBUG") == 0)
        flags |= TA_LOG_DEBUG;
      else if (strcmp (tok, "INFO") == 0)
        flags |= TA_LOG_INFO;
      else if (strcmp (tok, "WARN") == 0)
        flags |= TA_LOG_WARN;
      else if (strcmp (tok, "ERROR") == 0)
        flags |= TA_LOG_ERROR;
      else if (strcmp (tok, "CRITICAL") == 0)
        flags |= TA_LOG_CRITICAL;
    }

  logger = server->app->logger;
  ta_log_set_level (logger, flags);

  logger = ta_xmpp_client_get_logger (server->app->xmpp);
  ta_log_set_level (logger, flags);

  return NULL;
}

static char *
cmd_set_log_use_colors (bitu_server_t *server, char **params, int nparams)
{
  char *error = NULL;
  ta_log_t *logger;
  int val;

  if ((error = _validate_num_params ("set-log-use-colors", 1, nparams))
      != NULL)
    return error;

  val = strcmp (params[0], "true") == 0;

  logger = server->app->logger;
  ta_log_set_use_colors (logger, val);

  logger = ta_xmpp_client_get_logger (server->app->xmpp);
  ta_log_set_use_colors (logger, val);

  return NULL;
}

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
  server->can_run = 1;

  /* Registering commands */
  hashtable_set (server->commands, "load", cmd_load);
  hashtable_set (server->commands, "unload", cmd_unload);
  hashtable_set (server->commands, "send", cmd_send);
  hashtable_set (server->commands, "list", cmd_list);
  hashtable_set (server->commands, "set-log-file", cmd_set_log_file);
  hashtable_set (server->commands, "set-log-level", cmd_set_log_level);
  hashtable_set (server->commands, "set-log-use-colors",
                 cmd_set_log_use_colors);

  _setup_sigaction (server);
  return server;
}

void
bitu_server_free (bitu_server_t *server)
{
  ta_log_info (server->app->logger, "Gracefully exiting, see you!");
  ta_xmpp_client_disconnect (server->app->xmpp);
  hashtable_destroy (server->commands);
  server->can_run = 0;
  close (server->sock);
  unlink (server->sock_path);
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
  int msg_size, msgbufsize = 128;
  int num_params;

  answer = malloc (msgbufsize);
  if (cmdline == NULL)
    {
      msg_size =
        snprintf (answer, msgbufsize, "Empty command line");
      ta_log_warn (server->app->logger, answer);
    }
  else
    {
      if (!bitu_util_extract_params (cmdline, &cmd, &params, &num_params))
        {
          msg_size =
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
  int n, sent = 0;
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
  unsigned int sock;
  struct sockaddr_un remote;

  while (server->can_run)
    {
      socklen_t len;
      char *str = NULL, *answer = NULL;
      int full_len, allocated;
      int timeout;

      len = sizeof (struct sockaddr_un);
      sock = accept (server->sock, (struct sockaddr *) &remote, &len);
      if (sock == -1 && errno == EINTR)
        {
          ta_log_info (server->app->logger,
                       "accept() interrupted, stopping main loop");
          break;
        }
      if (sock == -1)
        {
          ta_log_error (server->app->logger,
                        "Error in accept(): %s",
                        strerror (errno));
          continue;
        }
      ta_log_info (server->app->logger, "Client connected");

      /* Initializing variables that are going to be used when
       * allocating space to store the whole command received via
       * socket */

      while (1)
        {
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
