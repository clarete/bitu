/* transport-xmpp.c - This file is part of the bitu program
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iksemel.h>
#include <taningia/taningia.h>
#include <bitu/util.h>
#include <bitu/errors.h>
#include <bitu/transport.h>

/* XMPP client callbacks */

static int
connected_cb (ta_xmpp_client_t *client, void *data)
{
  return 0;
}

static int
auth_cb (ta_xmpp_client_t *client, void *data)
{
  iks *node;

  /* Sending presence info */
  node = iks_make_pres (IKS_SHOW_AVAILABLE, "Online");
  ta_xmpp_client_send (client, node);
  iks_delete (node);

  return 0;
}

static int
auth_failed_cb (ta_xmpp_client_t *client, void *data)
{
  ta_xmpp_client_disconnect (client);
  return 0;
}

static int
message_received_cb (ta_xmpp_client_t *client, ikspak *pak, void *data)
{
  char *rawbody = NULL;
  bitu_transport_t *transport = (bitu_transport_t *) data;
  bitu_command_t *command = NULL;

  rawbody = iks_find_cdata (pak->x, "body");
  if (rawbody == NULL)
    return 0;

  command = bitu_command_new (transport, rawbody, pak->from->full);
  if (bitu_transport_queue_command (transport, command) == TA_ERROR)
    {
      bitu_command_free (command);
      bitu_transport_send (transport,
                           "Sorry sir, I couldn't queue your command",
                           pak->from->full);
    }
  return 0;
}

static int
presence_noticed_cb (ta_xmpp_client_t *client, ikspak *pak, void *data)
{
  ikstack *stack;
  iksid *client_id;
  int give_up;

  /* We don't need to handle our own presence request. */
  stack = iks_stack_new (255, 64);
  client_id = iks_id_new (stack, ta_xmpp_client_get_jid (client));
  give_up = strcmp (pak->from->partial, client_id->partial) == 0;
  iks_stack_delete (stack);
  if (give_up)
    return 0;

  /* TODO: A good idea here is to read a whitelist from a file or from
   * the command line parameters with acceptable jids. Now, I'm just
   * complaining that someone is trying to add the bot's contact */
  fprintf (stderr, "Presence: %s\n", pak->from->full);
  return 0;
}

static void
_query_service (const char *query, char **host, int *port)
{
  int i;
  ta_srv_target_t *t = NULL;
  ta_list_t
    *tmp = NULL,
    *answer = ta_srv_query_domain ("_xmpp-client._tcp", query);
  for (tmp = answer, i = 0; tmp; tmp = tmp->next, i++)
    {
      if (i == 0)
        {
          t = (ta_srv_target_t *) answer->data;
          *host = strdup (ta_srv_target_get_host (t));
          *port = ta_srv_target_get_port (t);
        }
      ta_object_unref (t);
    }
  ta_list_free (answer);
}


static int
_xmpp_connect (bitu_transport_t *transport)
{
  ta_xmpp_client_t *client;
  int port;
  size_t jid_len;
  char *user = NULL,
    *host = NULL,
    *server = NULL,
    *password = NULL,
    *resource = NULL,
    *jid = NULL;

  /* Getting data from the uri and parsing the username to get the
   * password after ':'. */
  user = (char *) ta_iri_get_user (bitu_transport_get_uri (transport));
  host = (char *) ta_iri_get_host (bitu_transport_get_uri (transport));
  resource = (char *) ta_iri_get_path (bitu_transport_get_uri (transport));
  password = strchr (user, ':') + 1;
  user = strndup (user, password - user - 1);
  port = ta_iri_get_port (bitu_transport_get_uri (transport));

  /* Performing a SRV query to get the host. I'm always calling the init
   * function to re-read all the config files everytime we try to
   * connect, saving the user the need to kill bitU and open again just
   * cause the resolv.conf file was changed */
  ta_srv_init ();
  _query_service (host, &server, &port);
  port = port ? port : IKS_JABBER_PORT;

  /* We must have all these three variables filled by now, otherwise, we
   * can't connect to the xmpp server */
  if (user == NULL || password == NULL || host == NULL)
    {
      ta_error_set (BITU_ERROR_TRANSPORT_INVALID_URL,
                    "Transport not initialized: %s: "
                    "XMPP transport uri must contain user, password and host",
                    ta_iri_to_string (bitu_transport_get_uri (transport)));
      return TA_ERROR;
    }

  /* Building the JID again, but now only with user and host. */
  if (resource == NULL)
    resource = "/bitU";
  jid_len = strlen (user) + strlen (host) + strlen (resource) + 2;
  jid = malloc (jid_len * sizeof (char));
  snprintf (jid, jid_len, "%s@%s%s", user, host, resource);
  free (user);

  /* Configuring xmpp client */
  client = ta_xmpp_client_new (jid, password, server, port);
  free (jid);

  /* Saving the client for further calls */
  bitu_transport_set_data (transport, client);

  /* Connecting signals */
  ta_xmpp_client_event_connect (client, "connected",
                                (ta_xmpp_client_hook_t) connected_cb,
                                NULL);
  ta_xmpp_client_event_connect (client, "authenticated",
                                (ta_xmpp_client_hook_t) auth_cb,
                                NULL);
  ta_xmpp_client_event_connect (client, "authentication-failed",
                                (ta_xmpp_client_hook_t) auth_failed_cb,
                                NULL);
  ta_xmpp_client_event_connect (client, "message-received",
                                (ta_xmpp_client_hook_t) message_received_cb,
                                transport);
  ta_xmpp_client_event_connect (client, "presence-noticed",
                                (ta_xmpp_client_hook_t) presence_noticed_cb,
                                NULL);

  /* Forwarding the logger */
  bitu_transport_set_logger (transport, ta_xmpp_client_get_logger (client));

  /* Finally, let's connect to the xmpp server and get out! */
  return ta_xmpp_client_connect (client) == TA_OK
    ? BITU_CONN_STATUS_OK
    : BITU_CONN_STATUS_CONNECTION_FAILED;
}


static int
_xmpp_disconnect (bitu_transport_t *transport)
{
  ta_xmpp_client_t *client =
    (ta_xmpp_client_t *) bitu_transport_get_data (transport);

  if (client == NULL)
    return BITU_CONN_STATUS_ALREADY_SHUTDOWN;

  ta_xmpp_client_disconnect (client);
  ta_object_unref (client);
  return BITU_CONN_STATUS_OK;
}


static int
_xmpp_run (bitu_transport_t *transport)
{
  ta_xmpp_client_t *client =
    (ta_xmpp_client_t *) bitu_transport_get_data (transport);
  return ta_xmpp_client_run (client, 0) == TA_OK
    ? BITU_CONN_STATUS_OK
    : BITU_CONN_STATUS_ERROR;
}


static int
_xmpp_is_running (bitu_transport_t *transport)
{
  ta_xmpp_client_t *client =
    (ta_xmpp_client_t *) bitu_transport_get_data (transport);
  return client == NULL ? TA_ERROR : ta_xmpp_client_is_running (client);
}


static int
_xmpp_send (bitu_transport_t *transport, const char *msg, const char *to)
{
  int result;
  iks *answer;
  ta_xmpp_client_t *client =
    (ta_xmpp_client_t *) bitu_transport_get_data (transport);

  /* Feeding the user back */
  answer = iks_make_msg (IKS_TYPE_CHAT, to, msg);
  result = ta_xmpp_client_send (client, answer);

  /* Freeing stuff */
  iks_delete (answer);

  return result;
}


int
_bitu_xmpp_transport (bitu_transport_t *transport)
{
  bitu_transport_set_callback_connect (transport, _xmpp_connect);
  bitu_transport_set_callback_disconnect (transport, _xmpp_disconnect);
  bitu_transport_set_callback_is_running (transport, _xmpp_is_running);
  bitu_transport_set_callback_run (transport, _xmpp_run);
  bitu_transport_set_callback_send (transport, _xmpp_send);
  return TA_OK;
}
