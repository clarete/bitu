/* transport.c - This file is part of the bitu program
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <taningia/taningia.h>
#include <bitu/util.h>
#include <bitu/errors.h>
#include <bitu/transport.h>

#include "hashtable.h"
#include "hashtable-utils.h"


#define COMMAND_QUEUE_SIZE 10


struct bitu_queue
{
  ta_list_t *list;
  int running;
  int maxsize;
  pthread_mutex_t *mutex;
  pthread_cond_t *not_full;
  pthread_cond_t *not_empty;
};


struct bitu_conn_manager
{
  hashtable_t *transports;
  bitu_queue_t *commands;
};

typedef struct {
  bitu_queue_t *queue;
  bitu_queue_callback_consume_t callback;
  void *data;
} _bitu_consumer_params_t;


struct bitu_transport
{
  ta_log_t *logger;
  bitu_queue_t *commands;
  ta_iri_t *uri;
  void *data;
  int (*connect) (bitu_transport_t *transport);
  int (*disconnect) (bitu_transport_t *transport);
  int (*is_running) (bitu_transport_t *transport);
  int (*run) (bitu_transport_t *transport);
  int (*send) (bitu_transport_t *transport,
               const char *msg,
               const char *to);
};


struct bitu_command
{
  bitu_transport_t *transport;
  char *from;
  char *cmd;
  char *name;
  char **params;
  int nparams;
};


static void
_bitu_transport_free (bitu_transport_t *transport)
{
  ta_object_unref (transport->uri);
  free (transport);
}


bitu_conn_manager_t *
bitu_conn_manager_new (void)
{
  bitu_conn_manager_t *manager = malloc (sizeof (bitu_conn_manager_t));
  manager->commands = bitu_queue_new ();
  manager->transports =
    hashtable_create (hash_string,
                      string_equal,
                      (free_fn) free,
                      (free_fn) _bitu_transport_free);
  return manager;
}


bitu_transport_t *
bitu_conn_manager_add (bitu_conn_manager_t *manager, const char *uri)
{
  bitu_transport_t *transport;

  /* We cannot override the current */
  if ((transport = hashtable_get (manager->transports, uri)) != NULL)
    return transport;

  if ((transport = bitu_transport_new (uri)) == NULL)
    return NULL;

  /* All transports write to the same command queue */
  transport->commands = manager->commands;

  /* Saving the transport to the manager */
  hashtable_set (manager->transports, strdup (uri), transport);
  return transport;
}


bitu_conn_status_t
bitu_conn_manager_remove (bitu_conn_manager_t *manager, const char *uri)
{
  bitu_transport_t *transport;
  if ((transport = hashtable_get (manager->transports, uri)) == NULL)
    return BITU_CONN_STATUS_TRANSPORT_NOT_FOUND;

  /* We can't disconnect while the transport is running */
  if (bitu_transport_is_running (transport) == TA_OK)
    return BITU_CONN_STATUS_STILL_RUNNING;

  /* This will also free the transport using the _bitu_transport_free()
   * function, assigned in the creation of the hash table. */
  hashtable_del (manager->transports, uri);
  return BITU_CONN_STATUS_OK;
}


int
bitu_conn_manager_get_n_transports (bitu_conn_manager_t *manager)
{
  return manager->transports->size;
}


ta_list_t *
bitu_conn_manager_get_transports (bitu_conn_manager_t *manager)
{
  void *iter = NULL;
  ta_list_t *keys = NULL;

  if ((iter = hashtable_iter (manager->transports)) == NULL)
    return NULL;
  do
    keys = ta_list_append (keys, hashtable_iter_key (iter));
  while ((iter = hashtable_iter_next (manager->transports, iter)) != NULL);
  return keys;
}


bitu_transport_t *
bitu_conn_manager_get_transport (bitu_conn_manager_t *manager, const char *uri)
{
  bitu_transport_t *transport;
  transport = hashtable_get (manager->transports, uri);
  return transport;
}


/* This function is just a thin wrapper to log stuff if the */
static void
_bitu_conn_manager_run_transport (bitu_transport_t *transport)
{
  if ((bitu_transport_run (transport) == TA_ERROR))
    if (transport->logger)
      ta_log_warn (transport->logger, "Failed to run the transport: %s",
                   transport->uri);
}


bitu_conn_status_t
bitu_conn_manager_run (bitu_conn_manager_t *manager, const char *uri)
{
  int status;
  bitu_transport_t *transport;
  if ((transport = bitu_conn_manager_get_transport (manager, uri)) == NULL)
    return BITU_CONN_STATUS_TRANSPORT_NOT_FOUND;

  /* Connecting && running the client */
  if ((status = bitu_transport_connect (transport)) == BITU_CONN_STATUS_OK)
    {
      bitu_util_start_new_thread ((bitu_util_callback_t) _bitu_conn_manager_run_transport,
                                  transport);
      return BITU_CONN_STATUS_SPAWNED;
    }
  else
    {
      /* We couldn't run the transport, let's disconnect it */
      if (transport->logger)
        ta_log_warn (transport->logger, "Problems when connecting the transport: %s",
                     transport->uri);
      bitu_conn_manager_shutdown (manager, uri);
      return BITU_CONN_STATUS_CONNECTION_FAILED;
    }
  return status;
}


bitu_conn_status_t
bitu_conn_manager_shutdown (bitu_conn_manager_t *manager, const char *uri)
{
  bitu_transport_t *transport;
  if ((transport = bitu_conn_manager_get_transport (manager, uri)) == NULL)
    return BITU_CONN_STATUS_TRANSPORT_NOT_FOUND;

  /* Really shutting down */
  if (transport->logger)
    ta_log_info (transport->logger, "Shutting down transport: %s", uri);
  if (bitu_transport_disconnect (transport) == TA_OK)
    return BITU_CONN_STATUS_OK;

  return BITU_CONN_STATUS_ALREADY_SHUTDOWN;
}


void *
_do_bitu_conn_manager_consume (void *data)
{
  _bitu_consumer_params_t *params = (_bitu_consumer_params_t *) data;
  bitu_queue_consume (params->queue, params->callback, params->data);
  free (params);
  return NULL;
}


void
bitu_conn_manager_consume (bitu_conn_manager_t *manager,
                           bitu_queue_callback_consume_t callback,
                           void *data)
{
  _bitu_consumer_params_t *params = malloc (sizeof (_bitu_consumer_params_t));
  params->queue = manager->commands;
  params->callback = callback;
  params->data = data;
  bitu_util_start_new_thread (_do_bitu_conn_manager_consume, params);
}


/* The transport api */


bitu_transport_t *
bitu_transport_new (const char *uri)
{
  bitu_transport_t *transport;
  ta_iri_t *uri_obj;
  const char *scheme;

  /* Parsing the given uri */
  uri_obj = ta_iri_new ();
  if (ta_iri_set_from_string (uri_obj, uri) != TA_OK)
    return NULL;

  /* No schema, no game here */
  if ((scheme = ta_iri_get_scheme (uri_obj)) == NULL)
    {
      ta_error_set (BITU_ERROR_TRANSPORT_INVALID_URL,
                    "You must provide a scheme, like xmpp://");
      return NULL;
    }

  /* Allocating memory for the new transport */
  transport = malloc (sizeof (bitu_transport_t));
  transport->data = NULL;
  transport->uri = uri_obj;
  transport->logger = ta_log_new (uri);

  /* Looking for the right transport. Possible values hardcoded by
   * now */
  if (strcmp (scheme, "file") == 0)
    {
      if (_bitu_local_transport (transport) != TA_OK)
        goto error;
    }
  else if (strcmp (scheme, "xmpp") == 0)
    {
      if (_bitu_xmpp_transport (transport) != TA_OK)
        goto error;
    }
  else if (strcmp (scheme, "irc") == 0)
    {
      if (_bitu_irc_transport (transport) != TA_OK)
        goto error;
    }
  else
    goto error;

  return transport;

 error:
  ta_object_unref (uri_obj);
  ta_error_set (BITU_ERROR_TRANSPORT_NOT_SUPPORTED,
                "There is no transport to handle the protocol %s",
                scheme);
  free (transport);
  return NULL;
}


int
bitu_transport_queue_command (bitu_transport_t *transport, bitu_command_t *cmd)
{
  if (transport->commands == NULL)
    return TA_ERROR;
  if (transport->logger)
    ta_log_info (transport->logger, "Command received via transport (%s): %s",
                 transport->uri, cmd->cmd);
  bitu_queue_add (transport->commands, cmd);
  return TA_OK;
}


ta_iri_t *
bitu_transport_get_uri (bitu_transport_t *transport)
{
  return transport->uri;
}

ta_log_t *
bitu_transport_get_logger (bitu_transport_t *transport)
{
  return transport->logger;
}

void
bitu_transport_set_logger (bitu_transport_t *transport, ta_log_t *logger)
{
  transport->logger = logger;
}

void *
bitu_transport_get_data (bitu_transport_t *transport)
{
  return transport->data;
}

void
bitu_transport_set_data (bitu_transport_t *transport, void *data)
{
  transport->data = data;
}

void
bitu_transport_set_callback_connect (bitu_transport_t *transport,
                                     bitu_transport_callback_connect_t callback)
{
  transport->connect = callback;
}

void
bitu_transport_set_callback_disconnect (bitu_transport_t *transport,
                                        bitu_transport_callback_disconnect_t callback)
{
  transport->disconnect = callback;
}

void
bitu_transport_set_callback_is_running (bitu_transport_t *transport,
                                        bitu_transport_callback_is_running_t callback)
{
  transport->is_running = callback;
}

void
bitu_transport_set_callback_run (bitu_transport_t *transport,
                                 bitu_transport_callback_run_t callback)
{
  transport->run = callback;
}

void
bitu_transport_set_callback_send (bitu_transport_t *transport,
                                  bitu_transport_callback_send_t callback)
{
  transport->send = callback;
}

int
bitu_transport_connect (bitu_transport_t *transport)
{
  if (transport->logger)
    ta_log_info (transport->logger, "Connecting transport");
  return transport->connect (transport);
}

int
bitu_transport_disconnect (bitu_transport_t *transport)
{
  if (transport->logger)
    ta_log_info (transport->logger, "Disconnecting transport: %s",
                 transport->uri);
  return transport->disconnect (transport);
}

int
bitu_transport_is_running (bitu_transport_t *transport)
{
  return transport->is_running (transport);
}

int
bitu_transport_send (bitu_transport_t *transport, const char *msg, const char *to)
{
  return transport->send (transport, msg, to);
}

int
bitu_transport_run (bitu_transport_t *transport)
{
  if (transport->logger)
    ta_log_info (transport->logger, "Starting transport main loop: %s",
                 transport->uri);
  return transport->run (transport);
}


/* -- Command API -- */


bitu_command_t *
bitu_command_new (bitu_transport_t *transport, const char *cmd, const char *from)
{
  bitu_command_t *command;
  char *name;
  char **params;
  int nparams;

  if ((command = malloc (sizeof (bitu_command_t))) == NULL)
    return NULL;

  command->transport = transport;
  command->cmd = bitu_util_strstrip (cmd);
  command->from = from ? strdup (from) : NULL;

  /* Filling out some optional arguments after parsing a command */
  if (bitu_util_extract_params (cmd, &name, &params, &nparams) == TA_OK)
    {
      command->name = name;
      command->params = params;
      command->nparams = nparams;
    }
  else
    {
      command->name = NULL;
      command->params = NULL;
      command->nparams = -1;
    }

  return command;
}


void
bitu_command_free (bitu_command_t *command)
{
  free (command->cmd);
  if (command->from)
    free (command->from);
  free (command);
}


bitu_transport_t *
bitu_command_get_transport (bitu_command_t *command)
{
  return command->transport;
}

const char *
bitu_command_get_from (bitu_command_t *command)
{
  return (const char *) command->from;
}

const char *
bitu_command_get_cmd (bitu_command_t *command)
{
  return (const char *) command->cmd;
}

const char *
bitu_command_get_name (bitu_command_t *command)
{
  return (const char *) command->name;
}

const char **
bitu_command_get_params (bitu_command_t *command)
{
  return (const char **) command->params;
}

int
bitu_command_get_nparams (bitu_command_t *command)
{
  return command->nparams;
}


/* -- Queue api -- */


bitu_queue_t *
bitu_queue_new (void)
{
  bitu_queue_t *queue;
  if ((queue = malloc (sizeof (bitu_queue_t))) == NULL)
    return NULL;

  queue->maxsize = COMMAND_QUEUE_SIZE;
  queue->running = 0;
  queue->list = NULL;
  queue->mutex = malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (queue->mutex, NULL);
  queue->not_full = malloc (sizeof (pthread_cond_t));
  pthread_cond_init (queue->not_full, NULL);
  queue->not_empty = malloc (sizeof (pthread_cond_t));
  pthread_cond_init (queue->not_empty, NULL);

  return queue;
}


void
bitu_queue_free (bitu_queue_t *queue)
{
  pthread_mutex_destroy (queue->mutex);
  free (queue->mutex);

  pthread_cond_destroy (queue->not_full);
  free (queue->not_full);

  pthread_cond_destroy (queue->not_empty);
  free (queue->not_empty);

  free (queue);
}


void
bitu_queue_add (bitu_queue_t *queue, void *data)
{
  fprintf (stderr, "queue::add() called\n");

  /* Exclusive access */
  pthread_mutex_lock (queue->mutex);
  fprintf (stderr, "queue::add() locked the mutex\n");

  /* The queue is full, let's wait until it has free slots to add new
   * commands again */
  while (ta_list_len (queue->list) >= queue->maxsize)
    pthread_cond_wait (queue->not_full, queue->mutex);

  /* Adding the new entry to our queue */
  queue->list = ta_list_prepend (queue->list, data);

  /* Unlocking the mutex and forwarding the signal saying that our list
   * is not empty anymore */
  fprintf (stderr, "queue::add() unlocked the mutex\n");
  pthread_mutex_unlock (queue->mutex);
  pthread_cond_signal (queue->not_empty);
}


static void *
_bitu_queue_pop (bitu_queue_t *queue)
{
  void *data = NULL;
  ta_list_t *popped_out = NULL;
  queue->list = ta_list_pop (queue->list, &popped_out);

  if (popped_out != NULL)
    {
      data = popped_out->data;
      ta_list_free (popped_out);
    }
  return data;
}


void
bitu_queue_consume (bitu_queue_t *queue,
                    bitu_queue_callback_consume_t callback,
                    void *extra_data)
{
  void *data;

  fprintf (stderr, "queue::consume() called\n");

  queue->running = 1;

  while (queue->running)
    {
      /* Exclusive access */
      pthread_mutex_lock (queue->mutex);
      fprintf (stderr, "queue::consume() locked the mutex\n");

      /* The queue is full, let's wait until it has free slots to add new
       * commands again */
      while (ta_list_len (queue->list) == 0)
        pthread_cond_wait (queue->not_empty, queue->mutex);

      /* Popping the last element out of the list */
      if ((data = _bitu_queue_pop (queue)) != NULL)
        callback (data, extra_data);

      fprintf (stderr, "queue::consume() unlocked the mutex\n");
      pthread_mutex_unlock (queue->mutex);
      pthread_cond_signal (queue->not_full);
    }
}
