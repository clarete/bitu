/* transport-local.c - This file is part of the bitu program
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
#include <string.h>
#include <bitu/transport.h>
#include <bitu/server.h>


static void _message_received (bitu_server_t *server,
                               const char *event,
                               const char *client,
                               const char *message);


static void
_load_server (bitu_transport_t *transport)
{
  bitu_server_callbacks_t callbacks;
  const char *sock_path;
  bitu_server_t *server;
  ta_iri_t *uri;

  uri = bitu_transport_get_uri (transport);
  sock_path = ta_iri_get_path (uri);

  /* Filling out server event callbacks */
  memset (&callbacks, 0, sizeof (callbacks));
  callbacks.message_received = _message_received;

  /* And now, we need to create an instance of our server and set the
   * transport in the free slot that the server API gives us in case we
   * need to store random things to use with the server callbacks.
   *
   * Such a nice coincidence! The transport API has the same empty
   * slot. Let's use it to save the server and pass it through the
   * connect/run/send transport functions */
  server = bitu_server_new (sock_path, callbacks);
  bitu_server_set_data (server, transport);
  bitu_transport_set_data (transport, server);
  bitu_transport_set_logger (transport, bitu_server_get_logger (server));
}


/* Callback connected to the "message-received" event in the bitu_server
 * that we instantiate for the local transport. It basically just
 * forwards the received command to the consumer queue. */
static void
_message_received (bitu_server_t *server,
                   const char *event,
                   const char *client,
                   const char *message)
{
  bitu_command_t *command = NULL;
  bitu_transport_t *transport = bitu_server_get_data (server);
  command = bitu_command_new (transport, message, client);
  if (bitu_transport_queue_command (transport, command) == TA_ERROR)
    {
      bitu_command_free (command);
      bitu_transport_send (transport,
                           "Sorry sir, I couldn't queue your command",
                           client);
    }
}


/* Just returns the bitu_server_connect call with the server */
static int
_local_connect (bitu_transport_t *transport)
{
  bitu_server_t *server = bitu_transport_get_data (transport);
  if (server == NULL)
    {
      _load_server (transport);
      server = bitu_transport_get_data (transport);

      if (bitu_server_connect (server) != TA_OK)
        return BITU_CONN_STATUS_CONNECTION_FAILED;
      else
        return BITU_CONN_STATUS_OK;
    }
  return BITU_CONN_STATUS_ALREADY_RUNNING;
}


/* Shuts the server down */
static int
_local_disconnect (bitu_transport_t *transport)
{
  int status;
  bitu_server_t *server = bitu_transport_get_data (transport);

  /* There's no need to free it twice, right? */
  if (server == NULL)
    return BITU_CONN_STATUS_ALREADY_SHUTDOWN;

  status = bitu_server_disconnect (server);

  /* We'll actually allocate another one if the user decides to connect
   * again. */
  bitu_server_free (server);

  /* Resetting the server in the transport user_data field */
  bitu_transport_set_data (transport, NULL);

  return status == TA_OK ? BITU_CONN_STATUS_OK : BITU_CONN_STATUS_ERROR;
}


/* Once again, just calling the server api to run our internal server
 * instance */
static int
_local_run (bitu_transport_t *transport)
{
  return bitu_server_run (bitu_transport_get_data (transport));
}


/* Server status */
static int
_local_is_running (bitu_transport_t *transport)
{
  bitu_server_t *server = bitu_transport_get_data (transport);
  return (server == NULL) ? TA_ERROR : bitu_server_is_running (server);
}


/* Forwarding the function that sends messages to the local clients */
static int
_local_send (bitu_transport_t *transport, const char *msg, const char *to)
{
  bitu_server_t *server = bitu_transport_get_data (transport);
  return bitu_server_send (server, msg, to);
}


int
_bitu_local_transport (bitu_transport_t *transport)
{
  /* Filling out the transport api with the local callbacks to
   * connect, run the server and send messages */
  bitu_transport_set_callback_connect (transport, _local_connect);
  bitu_transport_set_callback_disconnect (transport, _local_disconnect);
  bitu_transport_set_callback_is_running (transport, _local_is_running);
  bitu_transport_set_callback_run (transport, _local_run);
  bitu_transport_set_callback_send (transport, _local_send);
  return TA_OK;
}
