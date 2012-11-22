/* app.c - This file is part of the bitu program
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
#include <bitu/errors.h>
#include <bitu/conf.h>
#include <bitu/transport.h>

#include "hashtable.h"
#include "hashtable-utils.h"
#include "app.h"

/* Forward declarations */

typedef char * (*command_t) (bitu_app_t *, const char **, int);

static char *_validate_num_params (const char *cmd, int x, int y);

static char *_validate_min_num_params (const char *cmd, int x, int y);

static void _register_commands (bitu_app_t *app);


/* App API */


bitu_app_t *
bitu_app_new (void)
{
  bitu_app_t *app;
  if ((app = malloc (sizeof (bitu_app_t))) == NULL)
    return NULL;

  app->logfile = NULL;
  app->logfd = -1;
  app->logflags = 0;
  app->plugin_ctx = bitu_plugin_ctx_new ();
  app->logger = ta_log_new ("bitu-main");
  app->environment = hashtable_create (hash_string, string_equal, free, free);
  app->commands = hashtable_create (hash_string, string_equal, NULL, NULL);
  app->connections = bitu_conn_manager_new ();

  _register_commands (app);
  return app;
}


void
bitu_app_free (bitu_app_t *app)
{
  /* Freeing the main components */
  hashtable_destroy (app->environment);
  hashtable_destroy (app->commands);
  bitu_plugin_ctx_free (app->plugin_ctx);
  /* bitu_conn_manager_free (app->connections); */

  /* Freeing other stuff */
  ta_object_unref (app->logger);
  free (app->logfile);
  close (app->logfd);

  /* Freeing the app itself */
  free (app);
}


int
bitu_app_load_config (bitu_app_t *app, ta_list_t *commands)
{
  ta_list_t *tmp;

  for (tmp = commands; tmp; tmp = tmp->next)
    {
      bitu_command_t *command;
      char *answer = NULL;

      command = (bitu_command_t *) tmp->data;

      /* Commands from config file don't have a transport neither a
       * sender */
      bitu_app_exec_command (app, command, &answer);
      bitu_command_free (command);

      /* Logging stuff */
      if (answer)
        {
          ta_log_info (app->logger, "Running command %s: %s",
                       bitu_command_get_cmd (command), answer);
          free (answer);
        }
    }
  return TA_OK;
}


int
bitu_app_dump_config (bitu_app_t *app)
{
  return TA_OK;
}


int
bitu_app_exec_command (bitu_app_t *app, bitu_command_t *command, char **output)
{
  char answer[128];
  const char *name = bitu_command_get_name (command);
  command_t func;

  if ((func = hashtable_get (app->commands, name)) == NULL)
    {
      snprintf (answer, sizeof (answer), "Command `%s' not found", name);
      ta_log_warn (app->logger, answer);
      *output = strdup (answer);
      return TA_ERROR;
    }

  *output = func (app,
                  bitu_command_get_params (command),
                  bitu_command_get_nparams (command));
  return TA_OK;
}


static
int _exec_command (void *data, void *extra_data)
{
  bitu_app_t *app;
  bitu_command_t *command;
  bitu_transport_t *transport;
  char *output = NULL;
  int status;

  app = (bitu_app_t *) extra_data;
  command = (bitu_command_t *) data;
  status = bitu_app_exec_command (app, command, &output);

  if ((transport = bitu_command_get_transport (command)) != NULL)
    if (bitu_transport_send (transport, output, bitu_command_get_from (command)) != TA_OK)
      ta_log_warn (app->logger,
                   "Unable to send a message to the user %s",
                   bitu_command_get_from (command));

  if (output)
    free (output);
  return status;
}


int
bitu_app_run_transports (bitu_app_t *app)
{
  int connected;
  ta_list_t *transports = NULL, *tmp = NULL;

  /* Walking through all the transports found and trying to connect and
   * run them. */
  transports = bitu_conn_manager_get_transports (app->connections);
  for (tmp = transports; tmp; tmp = tmp->next)
    switch (bitu_conn_manager_run (app->connections, tmp->data))
      {
      case BITU_CONN_STATUS_OK:
        connected++;
        break;
      case BITU_CONN_STATUS_ALREADY_RUNNING:
        ta_log_warn (app->logger, "Transport `%s' already running", tmp->data);
        break;
      case BITU_CONN_STATUS_TRANSPORT_NOT_FOUND:
        ta_log_warn (app->logger, "Transport `%s' not found", tmp->data);
        break;
      case BITU_CONN_STATUS_CONNECTION_FAILED:
        ta_log_warn (app->logger, "Transport `%s' failed to connect", tmp->data);
        break;
      default:
        ta_log_warn (app->logger, "Transport `%s' not initialized", tmp->data);
        break;
      }
  ta_list_free (transports);

  /* Starting the consumer thread */
  if (connected > 0)
    {
      bitu_conn_manager_consume (app->connections,
                                 (bitu_queue_callback_consume_t) _exec_command,
                                 app);
      return TA_OK;
    }
  return TA_ERROR;
}


/* -- Commands -- */


static char *
cmd_help (bitu_app_t *app, char **params, int num_params)
{
  char *message;
  ta_buf_t buf = TA_BUF_INIT;

  ta_buf_alloc (&buf, 256);

  ta_buf_cat (&buf, "Hi, I'm a bitU bot, the first of my kin\n\n");
  ta_buf_cat (&buf, "There are two main ways to interact with me:\n");
  ta_buf_cat (&buf, " 1) type 'list commands' and access all the commands available\n");
  ta_buf_cat (&buf, " 2) type 'list plugins' and see all currently loaded plugins\n\n");
  ta_buf_cat (&buf, "For more information, you can go to my home: ");
  ta_buf_cat (&buf, "http://github.com/clarete/bitu");

  message = strdup (ta_buf_cstr (&buf));
  ta_buf_dealloc (&buf);
  return message;
}


static char *
cmd_set (bitu_app_t *app, char **params, int num_params)
{
  char *error;
  if ((error = _validate_num_params ("set", 2, num_params)) != NULL)
    return error;
  hashtable_set (app->environment, strdup (params[0]), strdup (params[1]));
  return NULL;
}


static char *
cmd_get (bitu_app_t *app, char **params, int num_params)
{
  char *error, *val;
  if ((error = _validate_num_params ("set", 1, num_params)) != NULL)
    return error;
  val = hashtable_get (app->environment, params[0]);
  return val ? strdup (val) : NULL;
}


static char *
cmd_unset (bitu_app_t *app, char **params, int num_params)
{
  char *error;
  if ((error = _validate_num_params ("unset", 1, num_params)) != NULL)
    return error;
  hashtable_del (app->environment, params[0]);
  return NULL;
}


static char *
cmd_env (bitu_app_t *app, char **params, int num_params)
{
  void *iter;
  char *error, *val, *tmp, *pos, *list = NULL;
  size_t val_size, current_size = 0, full_size = 0, step = 256, lastp = 0;
  if ((error = _validate_num_params ("env", 0, num_params)) != NULL)
    return error;
  iter = hashtable_iter (app->environment);
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
  while ((iter = hashtable_iter_next (app->environment, iter)));
  list[current_size-1] = '\0';
  return list;
}


static char *
cmd_transport (bitu_app_t *app, char **params, int num_params)
{
  char *error;
  if ((error = _validate_min_num_params ("transport", 1, num_params)) != NULL)
    return error;

  if (strcmp (params[0], "add") == 0)
    {
      if ((error = _validate_num_params ("transport add", 2, num_params)) != NULL)
        return error;
      bitu_conn_manager_add (app->connections, params[1]);
      return NULL;
    }
  else if (strcmp (params[0], "remove") == 0)
    {
      if ((error = _validate_num_params ("transport remove", 2, num_params)) != NULL)
        return error;
      switch (bitu_conn_manager_remove (app->connections, params[1]))
        {
        case BITU_CONN_STATUS_STILL_RUNNING:
          return strdup ("You have to disconnect the transport before removing");
        case BITU_CONN_STATUS_TRANSPORT_NOT_FOUND:
          return strdup ("Transport not found");
        case BITU_CONN_STATUS_OK:
        default:
          return NULL;
        }
    }
  else if (strcmp (params[0], "list") == 0)
    {
      ta_buf_t buf = TA_BUF_INIT;
      ta_iri_t *uri;
      ta_list_t *transports = NULL, *tmp = NULL;
      bitu_transport_t *transport;
      char *message;
      char status[8];

      ta_buf_alloc (&buf, 32);

      transports = bitu_conn_manager_get_transports (app->connections);
      for (tmp = transports; tmp; tmp = tmp->next)
        {
          transport =
            bitu_conn_manager_get_transport (app->connections, tmp->data);
          if (transport == NULL)
            continue;
          uri = bitu_transport_get_uri (transport);

          if (bitu_transport_is_running (transport) == TA_OK)
            strcpy (status, "running");
          else
            strcpy (status, "stopped");
          ta_buf_catf (&buf, "[%s] %s", status, ta_iri_to_string (uri));

          /* We don't want line breaks in the end of the string */
          if (tmp->next != NULL)
            ta_buf_catf (&buf, "\n");
        }
      message = strdup (ta_buf_cstr (&buf));
      ta_buf_dealloc (&buf);
      return message;
    }
  else if (strcmp (params[0], "connect") == 0)
    {
      bitu_conn_status_t status;
      if ((error = _validate_num_params ("transport connect", 2, num_params)) != NULL)
        return error;
      status = bitu_conn_manager_run (app->connections, params[1]);
      switch (status)
        {
        case BITU_CONN_STATUS_OK:
          return NULL;
        case BITU_CONN_STATUS_TRANSPORT_NOT_FOUND:
          return strdup ("Transport not found, stop wasting my time");
        case BITU_CONN_STATUS_ALREADY_RUNNING:
          return strdup ("Transport already running, can't you see it???");
        default:
          return strdup ("I really don't know wtf just happened");
        }
    }
  else if (strcmp (params[0], "disconnect") == 0)
    {
      bitu_conn_status_t status;
      if ((error = _validate_num_params ("transport disconnect", 2, num_params)) != NULL)
        return error;
      status = bitu_conn_manager_shutdown (app->connections, params[1]);
      switch (status)
        {
        case BITU_CONN_STATUS_OK:
          return NULL;
        case BITU_CONN_STATUS_TRANSPORT_NOT_FOUND:
          return strdup ("Transport not found, stop wasting my time");
        case BITU_CONN_STATUS_ALREADY_SHUTDOWN:
          return strdup ("Transport already shutdown, do you want to kill it twice??");
        default:
          return strdup ("I really don't know wtf just happened");
        }
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


static char *
cmd_send (bitu_app_t *app, char **params, int num_params)
{
  /* const char *jid, *msg; */
  /* iks *xmpp_msg; */
  /* char *error; */
  /* char *result = NULL; */

  /* if ((error = _validate_num_params ("send", 2, num_params)) != NULL) */
  /*   return error; */

  /* jid = params[0]; */
  /* msg = params[1]; */

  /* xmpp_msg = iks_make_msg (IKS_TYPE_CHAT, jid, msg); */
  /* if (ta_xmpp_client_send (app->xmpp, xmpp_msg) != IKS_OK) */
  /*   result = strdup ("Message not sent"); */
  /* iks_delete (xmpp_msg); */
  /* return result; */
  return NULL;
}


static char *
cmd_list (bitu_app_t *app, char **params, int num_params)
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
      plugins = bitu_plugin_ctx_get_list (app->plugin_ctx);
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
      iter = hashtable_iter (app->commands);
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
      while ((iter = hashtable_iter_next (app->commands, iter)));

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
  bitu_app_t *app = (bitu_app_t *) user_data;
  write (app->logfd, data, strlen (data));
  write (app->logfd, "\n", 1);
  return 0;
}


static char *
cmd_set_log_file (bitu_app_t *app, char **params, int nparams)
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
                app->logfile, strerror (errno));
      ta_log_error (app->logger, error);
      return error;
    }
  if (app->logfile)
    free (app->logfile);
  if (app->logfd > -1)
    close (app->logfd);

  app->logfile = logfile;
  app->logfd = logfd;

  ta_log_info (app->logger, "Setting log file to %s", app->logfile);
  ta_log_set_handler (app->logger, (ta_log_handler_func_t) _log_handler, app);

  return ret;
}


static char *
cmd_set_log_level (bitu_app_t *app, char **params, int nparams)
{
  char *tok = NULL;
  char *error = NULL;
  ta_log_level_t level;
  ta_list_t *transports = NULL, *tmp = NULL;

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

  ta_log_set_level (app->logger, level);

  /* Iterating over all the transports setting their logger
   * configuration (if available). */
  transports = bitu_conn_manager_get_transports (app->connections);
  for (tmp = transports; tmp; tmp = tmp->next)
    {
      ta_log_t *logger;
      bitu_transport_t *transport;
      if ((transport = bitu_conn_manager_get_transport (app->connections, tmp->data)) == NULL)
        return NULL;
      if ((logger = bitu_transport_get_logger (transport)) != NULL)
        ta_log_set_level (logger, level);
    }

  return NULL;
}


static char *
cmd_set_log_use_colors (bitu_app_t *app, char **params, int nparams)
{
  char *error = NULL;
  ta_log_t *logger;
  int val;
  ta_list_t *transports = NULL, *tmp = NULL;

  if ((error = _validate_num_params ("set-log-use-colors", 1, nparams))
      != NULL)
    return error;

  val = strcmp (params[0], "true") == 0;

  logger = app->logger;
  ta_log_set_use_colors (logger, val);

  /* Iterating over all the transports setting their logger
   * configuration (if available). */
  transports = bitu_conn_manager_get_transports (app->connections);
  for (tmp = transports; tmp; tmp = tmp->next)
    {
      bitu_transport_t *transport = tmp->data;
      if ((transport = bitu_conn_manager_get_transport (app->connections, tmp->data)) == NULL)
        return NULL;
      if ((logger = bitu_transport_get_logger (transport)) != NULL)
        ta_log_set_use_colors (logger, val);
    }

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


static void
_register_commands (bitu_app_t *app)
{
  hashtable_set (app->commands, "help", cmd_help);
  hashtable_set (app->commands, "set", cmd_set);
  hashtable_set (app->commands, "get", cmd_get);
  hashtable_set (app->commands, "unset", cmd_unset);
  hashtable_set (app->commands, "env", cmd_env);
  hashtable_set (app->commands, "transport", cmd_transport);
  hashtable_set (app->commands, "load", cmd_load);
  hashtable_set (app->commands, "unload", cmd_unload);
  hashtable_set (app->commands, "send", cmd_send);
  hashtable_set (app->commands, "list", cmd_list);
  hashtable_set (app->commands, "set-log-file", cmd_set_log_file);
  hashtable_set (app->commands, "set-log-level", cmd_set_log_level);
  hashtable_set (app->commands, "set-log-use-colors", cmd_set_log_use_colors);
}
