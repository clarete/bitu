/* transport-irc.c - This file is part of the bitu program
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
#include <string.h>
#include <libircclient/libircclient.h>
#include <bitu/errors.h>
#include <bitu/transport.h>

#define IRC_PORT 6667


void _irc_event_notice (irc_session_t *session,
                        const char *event,
                        const char *origin,
                        const char **params,
                        unsigned int count)
{
  char buf[512];
  unsigned int cnt;

  buf[0] = '\0';

  for (cnt = 0; cnt < count; cnt++)
    {
      if (cnt)
        strcat (buf, "|");
      strcat (buf, params[cnt]);
    }

  fprintf (stderr, "Event: %s, origin: %s, params: %d [%s]\n", event, origin ? origin : NULL, cnt, buf);
}

void
_irc_event_connect (irc_session_t *session,
                    const char *event,
                    const char *origin,
                    const char **params,
                    unsigned int count)
{
  char *channel = (char *) irc_get_ctx (session);
  printf ("Channel: %s\n", channel);
  irc_cmd_join (session, channel, 0);
}

static irc_session_t *
_irc_get_session (bitu_transport_t *transport)
{
  irc_callbacks_t callbacks;
  irc_session_t *session;

  memset (&callbacks, 0, sizeof(callbacks));
  callbacks.event_connect = _irc_event_connect;
  callbacks.event_notice = _irc_event_notice;
  callbacks.event_channel_notice = _irc_event_notice;
  callbacks.event_privmsg = _irc_event_notice;

  if ((session = irc_create_session (&callbacks)) == NULL)
    {
      ta_error_set (BITU_ERROR_TRANSPORT_IRC_NOSESSION,
                    "Could not create the irc session");
      return NULL;
    }

  irc_set_ctx (session, (void *) ta_iri_get_fragment (transport->uri));
  return session;
}


static int
_irc_connect (bitu_transport_t *transport)
{
  int port;
  char *host, *nick;

  host = (char *) ta_iri_get_host (transport->uri);
  nick = (char *) ta_iri_get_user (transport->uri);
  nick = nick ? nick : "bitU";
  port = ta_iri_get_port (transport->uri);
  port = port ? port : IRC_PORT;

  printf ("Connecting...");
  if (irc_connect (transport->data, host, port, NULL, nick, 0, 0) != 0)
    {
      printf ("bosta: %s\n", irc_strerror (irc_errno (transport->data)));
      ta_error_set (BITU_ERROR_TRANSPORT_IRC_CONN,
                    irc_strerror (irc_errno (transport->data)));
      printf ("error\n");
      return TA_ERROR;
    }
  printf ("ok\n");
  return TA_OK;
}

static int
_irc_run (bitu_transport_t *transport)
{
  printf ("Running...\n");
  if (irc_run (transport->data) != 0)
    {
      printf ("bosta: %s\n", irc_strerror (irc_errno (transport->data)));
      ta_error_set (BITU_ERROR_TRANSPORT_IRC_RUN,
                    irc_strerror (irc_errno (transport->data)));
      printf ("error\n");
      return TA_ERROR;
    }
  printf ("ok\n");
  return TA_OK;
}

bitu_transport_t *
_bitu_irc_transport (ta_iri_t *uri)
{
  irc_session_t *session;
  bitu_transport_t *transport;
  transport = malloc (sizeof (bitu_transport_t));
  transport->uri = uri;
  transport->connect = _irc_connect;
  transport->run = _irc_run;

  if ((session = _irc_get_session (transport)) == NULL)
    {
      free (transport);
      return NULL;
    }
  transport->data = session;
  return transport;
}
