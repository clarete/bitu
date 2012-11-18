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
#include <regex.h>
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
  regex_t re;
  size_t i;
  char origin_nick[128];
  char pattern[128];
  const char *cmd;

  bitu_command_t *command = NULL;
  bitu_transport_t *transport = (bitu_transport_t *) irc_get_ctx (session);
  const char *nick = ta_iri_get_user (bitu_transport_get_uri (transport));
  const char *channel = ta_iri_get_fragment (bitu_transport_get_uri (transport));

  /* Is this message to me? */
  snprintf (pattern, sizeof (pattern), "[[:<:]]%s[[:>:]]", nick);
  if (regcomp (&re, pattern, REG_EXTENDED) != 0)
    return;
  if (regexec (&re, &params[1][0], 0, NULL, REG_ENHANCED | REG_NOSUB) != 0)
    return;

  /* Getting the origin nickname */
  if (origin)
    irc_target_get_nick (origin, origin_nick, sizeof (origin_nick));
  else
    strncpy (origin_nick, channel, sizeof (origin_nick));

  /* Sending the command to the queue */
  cmd = params[1];
  for (i = 0; i < strlen (nick) + 1; i++, *cmd++);
  command = bitu_command_new (transport, cmd, origin_nick);
  bitu_transport_queue_command (transport, command);

  /* XXX: Debug trash */
  {
    unsigned int cnt;
    char buf[512];
    buf[0] = '\0';

    for (cnt = 0; cnt < count; cnt++)
      {
        if (cnt)
          strcat (buf, "|");
        strcat (buf, params[cnt]);
      }

    fprintf (stderr,
             "Event: %s, origin: %s, params: %d [%s]\n",
             event, origin_nick, cnt, buf);
  }
}


void
_irc_event_connect (irc_session_t *session,
                    const char *event,
                    const char *origin,
                    const char **params,
                    unsigned int count)
{
  bitu_transport_t *transport = (bitu_transport_t *) irc_get_ctx (session);
  const char *channel = ta_iri_get_fragment (bitu_transport_get_uri (transport));
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
  callbacks.event_channel = _irc_event_notice;
  callbacks.event_channel_notice = _irc_event_notice;
  callbacks.event_privmsg = _irc_event_notice;

  if ((session = irc_create_session (&callbacks)) == NULL)
    {
      ta_error_set (BITU_ERROR_TRANSPORT_IRC_NOSESSION,
                    "Could not create the irc session");
      return NULL;
    }

  irc_set_ctx (session, (void *) transport);
  return session;
}


static int
_irc_connect (bitu_transport_t *transport)
{
  int port;
  char *host, *nick;

  host = (char *) ta_iri_get_host (bitu_transport_get_uri (transport));
  nick = (char *) ta_iri_get_user (bitu_transport_get_uri (transport));
  nick = nick ? nick : "bitU";
  port = ta_iri_get_port (bitu_transport_get_uri (transport));
  port = port ? port : IRC_PORT;

  if (irc_connect (bitu_transport_get_data (transport), host, port, NULL, nick, 0, 0) != 0)
    {
      ta_error_set (BITU_ERROR_TRANSPORT_IRC_CONN,
                    irc_strerror (irc_errno (bitu_transport_get_data (transport))));
      return TA_ERROR;
    }
  return TA_OK;
}


static int
_irc_run (bitu_transport_t *transport)
{
  if (irc_run (bitu_transport_get_data (transport)) != 0)
    {
      ta_error_set (BITU_ERROR_TRANSPORT_IRC_RUN,
                    irc_strerror (irc_errno (bitu_transport_get_data (transport))));
      return TA_ERROR;
    }
  return TA_OK;
}


static int
_irc_send (bitu_transport_t *transport, const char *msg, const char *to)
{
  irc_session_t *session = bitu_transport_get_data (transport);
  printf ("msg: %s: %s\n", to, msg);
  return irc_cmd_msg (session, to, msg) == 0 ? TA_OK : TA_ERROR;
}


int
_bitu_irc_transport (bitu_transport_t *transport)
{
  irc_session_t *session;
  bitu_transport_set_callback_connect (transport, _irc_connect);
  bitu_transport_set_callback_run (transport, _irc_run);
  bitu_transport_set_callback_send (transport, _irc_send);

  if ((session = _irc_get_session (transport)) == NULL)
      return TA_ERROR;
  bitu_transport_set_data (transport, session);
  return TA_OK;
}
