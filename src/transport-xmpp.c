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
#include <bitu/server.h>
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
  char *rawbody, *cmd = NULL, *message = NULL;
  char **params = NULL;
  int i, len;
  iks *answer;
  bitu_server_t *server;

  server = (bitu_server_t *) data;
  if (server == NULL)
    return 0;

  rawbody = iks_find_cdata (pak->x, "body");
  if (rawbody == NULL)
    return 0;

  if (!bitu_util_extract_params (rawbody, &cmd, &params, &len))
    {
      int msgbufsize = 128;
      message = malloc (msgbufsize);
      snprintf (message, msgbufsize, "The message seems to be empty");
    }
  else
    {
      if (cmd[0] == '/')
        {
          char *c = cmd;
          /* skipping the '/' char */
          message = bitu_server_exec_cmd (server, &(*++c), params, len, NULL);
        }
      else
        message = bitu_server_exec_plugin (server, cmd, params, len, NULL);
    }

  /* No answer was returned */
  if (message == NULL)
    message = strdup ("The plugin returned nothing");

  /* Feeding the user back */
  answer = iks_make_msg (IKS_TYPE_CHAT, pak->from->full, message);
  ta_xmpp_client_send (client, answer);

  /* Freeing all parameters collected */
  for (i = 0; i < len; i++)
    free (params[i]);
  free (params);

  /* Freeing all other stuff */
  iks_delete (answer);
  if (message)
    free (message);
  if (cmd)
    free (cmd);
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
    *jid = NULL;

  /* Getting data from the uri and parsing the username to get the
   * password after ':'. */
  user = (char *) ta_iri_get_user (transport->uri);
  host = (char *) ta_iri_get_host (transport->uri);
  password = strchr (user, ':') + 1;
  user = strndup (user, password - user - 1);
  port = ta_iri_get_port (transport->uri);

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
                    ta_iri_to_string (transport->uri));
      return TA_ERROR;
    }

  /* Building the JID again, but now only with user and host. */
  jid_len = strlen (user) + strlen (host) + 2;
  jid = malloc (jid_len * sizeof (char));
  snprintf (jid, jid_len, "%s@%s", user, host);
  free (user);

  /* Configuring xmpp client */
  client = ta_xmpp_client_new (jid, password, server, port);
  free (jid);

  /* Saving the client for further calls */
  transport->data = client;

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
                                transport->server);
  ta_xmpp_client_event_connect (client, "presence-noticed",
                                (ta_xmpp_client_hook_t) presence_noticed_cb,
                                NULL);

  /* Finally, let's connect to the xmpp server and get out! */
  return ta_xmpp_client_connect (client);
}

static int
_xmpp_run (bitu_transport_t *transport)
{
  ta_xmpp_client_t *client = (ta_xmpp_client_t *) transport->data;
  return ta_xmpp_client_run (client, 0);
}

bitu_transport_t *
_bitu_xmpp_transport (bitu_server_t *server, ta_iri_t *uri)
{
  bitu_transport_t *transport;
  transport = malloc (sizeof (bitu_transport_t));
  transport->server = server;
  transport->uri = uri;
  transport->connect = _xmpp_connect;
  transport->run = _xmpp_run;
  return transport;
}
