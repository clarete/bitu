/* server-commands.c - This file is part of the bitu program
 *
 * Copyright (C) 2012  Lincoln de Sousa <lincoln@comum.org>
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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <bitu/server.h>

#include "server-priv.h"

/* Forward declarations */
static char *_validate_num_params (const char *cmd, int x, int y);
static char *_validate_min_num_params (const char *cmd, int x, int y);


/* -- Commands -- */

static char *
cmd_set (bitu_server_t *server, char **params, int num_params)
{
  char *error;
  if ((error = _validate_num_params ("set", 2, num_params)) != NULL)
    return error;
  hashtable_set (server->env, strdup (params[0]), strdup (params[1]));
  return NULL;
}

static char *
cmd_get (bitu_server_t *server, char **params, int num_params)
{
  char *error, *val;
  if ((error = _validate_num_params ("set", 1, num_params)) != NULL)
    return error;
  val = hashtable_get (server->env, params[0]);
  return val ? strdup (val) : NULL;
}

static char *
cmd_unset (bitu_server_t *server, char **params, int num_params)
{
  char *error;
  if ((error = _validate_num_params ("unset", 1, num_params)) != NULL)
    return error;
  hashtable_del (server->env, params[0]);
  return NULL;
}

static char *
cmd_env (bitu_server_t *server, char **params, int num_params)
{
  void *iter;
  char *error, *val, *tmp, *pos, *list = NULL;
  size_t val_size, current_size = 0, full_size = 0, step = 256, lastp = 0;
  if ((error = _validate_num_params ("env", 0, num_params)) != NULL)
    return error;
  iter = hashtable_iter (server->env);
  if (iter == NULL)
    return NULL;
  do
    {
      val = hashtable_iter_key (iter);
      val_size = strlen (val);
      current_size += val_size + 1;
      if (full_size < current_size)
        {
          full_size += step;
          if ((tmp = realloc (list, full_size)) == NULL)
            {
              free (list);
              return NULL;
            }
          else
            list = tmp;
        }
      pos = list + lastp;
      memcpy (pos, val, val_size);
      memcpy (pos+val_size, "\n", 1);
      lastp += val_size + 1;
    }
  while ((iter = hashtable_iter_next (server->env, iter)));
  list[current_size-1] = '\0';
  return list;
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
  char *result = NULL;

  if ((error = _validate_num_params ("send", 2, num_params)) != NULL)
    return error;

  jid = params[0];
  msg = params[1];

  xmpp_msg = iks_make_msg (IKS_TYPE_CHAT, jid, msg);
  if (ta_xmpp_client_send (server->app->xmpp, xmpp_msg) != IKS_OK)
    result = strdup ("Message not sent");
  iks_delete (xmpp_msg);
  return result;
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
      if (ret != NULL)
        {
          memcpy (ret + full_size - 1, "\0", 1);
          ta_list_free (plugins);
        }
    }
  else if (strcmp (action, "commands") == 0)
    {
      void *iter;
      char *val, *tmp, *current_pos_str;
      size_t val_size, current_pos, full_size = 0;
      iter = hashtable_iter (server->commands);
      if (iter == NULL)
        return NULL;
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
  else
    ret = strdup ("Possible values are `commands' or `plugins'");
  return ret;
}

static int
_log_handler (ta_log_t *log, ta_log_level_t level,
              const char *data, void *user_data)
{
  bitu_server_t *server = (bitu_server_t *) user_data;
  if (server->can_run)
    {
      write (server->app->logfd, data, strlen (data));
      write (server->app->logfd, "\n", 1);
    }
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
      size_t bufsize = 128;
      error = malloc (bufsize);
      snprintf (error, bufsize,
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
  ta_log_level_t level;
  ta_log_t *logger;

  if ((error = _validate_min_num_params ("set-log-level", 1, nparams)) != NULL)
    return error;

  tok = params[0];

  if (strcmp (tok, "DEBUG") == 0)
    level = TA_LOG_DEBUG;
  else if (strcmp (tok, "INFO") == 0)
    level = TA_LOG_INFO;
  else if (strcmp (tok, "WARN") == 0)
    level = TA_LOG_WARN;
  else if (strcmp (tok, "ERROR") == 0)
    level = TA_LOG_ERROR;
  else if (strcmp (tok, "CRITICAL") == 0)
    level = TA_LOG_CRITICAL;

  logger = server->app->logger;
  ta_log_set_level (logger, level);

  /* TODO: wtf should I do here? */
  /* logger = ta_xmpp_client_get_logger (server->app->xmpp); */
  /* ta_log_set_level (logger, level); */

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

  /* TODO: wtf should I do here? */
  /* logger = ta_xmpp_client_get_logger (server->app->xmpp); */
  /* ta_log_set_use_colors (logger, val); */

  return NULL;
}


/* -- Helper functions -- */

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

/* -- Entry point: add new commands here -- */

void
_server_register_commands (bitu_server_t *server)
{
  hashtable_set (server->commands, "load", cmd_load);
  hashtable_set (server->commands, "set", cmd_set);
  hashtable_set (server->commands, "get", cmd_get);
  hashtable_set (server->commands, "unset", cmd_unset);
  hashtable_set (server->commands, "env", cmd_env);
  hashtable_set (server->commands, "unload", cmd_unload);
  hashtable_set (server->commands, "send", cmd_send);
  hashtable_set (server->commands, "list", cmd_list);
  hashtable_set (server->commands, "set-log-file", cmd_set_log_file);
  hashtable_set (server->commands, "set-log-level", cmd_set_log_level);
  hashtable_set (server->commands, "set-log-use-colors",
                 cmd_set_log_use_colors);
}
