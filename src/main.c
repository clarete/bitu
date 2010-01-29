/* main.c - This file is part of the diagnosis program
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <iksemel.h>
#include <taningia/taningia.h>

static int
connected_cb (ta_xmpp_client_t *client, void *data)
{
  fprintf (stderr, "connected\n");
  return 0;
}

static int
auth_cb (ta_xmpp_client_t *client, void *data)
{
  iks *node;
  fprintf (stderr, "authenticated\n");

  /* Sending presence info */
  node = iks_make_pres (IKS_SHOW_AVAILABLE, "Online");
  ta_xmpp_client_send (client, node);
  iks_delete (node);

  return 0;
}

static int
auth_failed_cb (ta_xmpp_client_t *client, void *data)
{
  fprintf (stderr, "auth-failed\n");
  return 0;
}

static void
usage (const char *prname)
{
  printf ("Usage: %s --jid=JID --password=PASSWD [--host=HOST, --port=PORT]\n",
          prname);
}

static int
message_received_cb (ta_xmpp_client_t *client, void *data)
{
  ikspak *pak;
  char *body;
  pak = (ikspak *) data;

  body = iks_find_cdata (pak->x, "body");
  if (body == NULL)
    return 0;

  printf ("blah: %s\n", body);
  fprintf (stderr, "message-received\n");
  return 0;
}

int
main (int argc, char **argv)
{
  /* xmpp stuff */
  ta_xmpp_client_t *xmpp;
  ta_log_t *logger;
  const char *jid = NULL, *passwd = NULL, *host = NULL;
  int port = 5222;

  /* getopt stuff */
  int c;
  static struct option long_options[] = {
    { "jid", required_argument, NULL, 'j' },
    { "password", required_argument, NULL, 'p' },
    { "host", optional_argument, NULL, 'H' },
    { "port", optional_argument, NULL, 'P' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0 }
  };

  while ((c = getopt_long (argc, argv, "j:p:H:P:", long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 'j':
          jid = strdup (optarg);
          break;

        case 'p':
          passwd = strdup (optarg);
          break;

        case 'H':
          host = strdup (optarg);
          break;

        case 'P':
          port = atoi (optarg);
          break;

        case 'h':
          usage (argv[0]);
          exit (EXIT_SUCCESS);
          break;

        default:
          fprintf (stderr, "Try `%s --help' for more information\n", argv[0]);
          break;
        }
    }

  /* Some param validation */
  if (jid == NULL || passwd == NULL)
    {
      usage (argv[0]);
      exit (EXIT_FAILURE);
    }
  if (host == NULL)
    {
      ikstack *stack = iks_stack_new (255, 64);
      iksid *id = iks_id_new (stack, jid);
      host = id->server;
      iks_stack_delete (stack);
    }

  /* Configuring xmpp client */
  xmpp = ta_xmpp_client_new (jid, passwd, host, port);
  ta_xmpp_client_event_connect (xmpp, "connected",
                                (ta_xmpp_client_hook_t) connected_cb,
                                NULL);
  ta_xmpp_client_event_connect (xmpp, "authenticated",
                                (ta_xmpp_client_hook_t) auth_cb,
                                NULL);
  ta_xmpp_client_event_connect (xmpp, "authentication-failed",
                                (ta_xmpp_client_hook_t) auth_failed_cb,
                                NULL);
  ta_xmpp_client_event_connect (xmpp, "message-received",
                                (ta_xmpp_client_hook_t) message_received_cb,
                                NULL);

  /* Configuring logging stuff */
  logger = ta_xmpp_client_get_logger (xmpp);
  ta_log_set_level (logger, ta_log_get_level (logger) |
                    TA_LOG_INFO | TA_LOG_DEBUG);
  ta_log_set_use_colors (logger, 1);

  /* Connecting */
  if (!ta_xmpp_client_connect (xmpp))
    {
      ta_error_t *error;
      error = ta_xmpp_client_get_error (xmpp);
      fprintf (stderr, "%s: %s\n", ta_error_get_name (error),
               ta_error_get_message (error));
      ta_xmpp_client_free (xmpp);
      return 1;
    }

  /* Running client */
  if (!ta_xmpp_client_run (xmpp, 0))
    {
      ta_error_t *error;
      error = ta_xmpp_client_get_error (xmpp);
      fprintf (stderr, "%s: %s\n", ta_error_get_name (error),
               ta_error_get_message (error));
      ta_xmpp_client_free (xmpp);
      return 1;
    }

  ta_xmpp_client_free (xmpp);
  return 0;
}
