/* main.c - This file is part of the bitu program
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
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <glob.h>
#include <iksemel.h>
#include <taningia/taningia.h>
#include <bitu/app.h>
#include <bitu/util.h>
#include <bitu/loader.h>
#include <bitu/server.h>
#include <bitu/conf.h>

#define SOCKET_PATH    "/tmp/bitu.sock"

/* This global var will only be used to manipulate the only server
 * instance running in the signal handler callback that don't accept
 * extra params. The only operation done with it is the graceful
 * shutdown of the application. */

static bitu_server_t *main_server;

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
  char *rawmsg, *cmd = NULL, *message = NULL;
  char **params = NULL;
  int i, len;
  iks *answer;
  bitu_plugin_t *plugin;
  bitu_plugin_ctx_t *plugin_ctx;
  int msgbufsize = 128;

  plugin_ctx = (bitu_plugin_ctx_t *) data;

  rawmsg = strdup (iks_find_cdata (pak->x, "body"));
  if (!bitu_util_extract_params (rawmsg, &cmd, &params, &len))
    {
      message = malloc (msgbufsize);
      snprintf (message, msgbufsize, "The message seems to be empty");
    }
  else if ((plugin = bitu_plugin_ctx_find (plugin_ctx, cmd)) == NULL)
    {
      message = malloc (msgbufsize);
      snprintf (message, msgbufsize, "Plugin `%s' not found", cmd);
    }
  else
    {
      /* Validating number of parameters */
      if (bitu_plugin_num_params (plugin) != len)
        {
          message = malloc (msgbufsize);
          snprintf (message, msgbufsize,
                    "Wrong number of parameters. `%s' "
                    "receives %d but %d were passed", cmd,
                    bitu_plugin_num_params (plugin),
                    len);
        }
      else
        message = bitu_plugin_message_return (plugin);
    }

  /* Feeding back the user */
  answer = iks_make_msg (IKS_TYPE_CHAT, pak->from->full, message);
  ta_xmpp_client_send (client, answer);

  /* Freeing all parameters collected */
  for (i = 0; i < len; i++)
    free (params[i]);
  free (params);

  /* Freeing all other stuff */
  iks_delete (answer);
  free (message);
  if (cmd)
    free (cmd);
  return 0;
}

static int
presence_noticed_cb (ta_xmpp_client_t *client, ikspak *pak, void *data)
{
  /* We don't need to handle our own presence request. */
  if (strcmp (pak->from->partial, ta_xmpp_client_get_jid (client)) == 0)
    return 0;

  /* TODO: A good idea here is to read a whitelist from a file or from
   * the command line parameters with acceptable jids. Now, I'm just
   * complaining that someone is trying to add the bot's contact */
  fprintf (stderr,"Someone wants to talk with me... "
           "I'm not sure I should =(\n");
  fprintf (stderr, "His/her name/jid is `%s'.\nThe sysadmin will be wise "
           "enough to decide if I should talk with this new buddy\n",
           pak->from->full);
  return 0;
}

/* Stuff to catch SIGINT and stop the bot gracefully. */

static void
_signal_handler (int sig, siginfo_t *si, void *data)
{
  bitu_server_free (main_server);
}

static void
_setup_sigaction (bitu_app_t *app)
{
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = _signal_handler;
  sigemptyset (&sa.sa_mask);
  if (sigaction (SIGINT, &sa, NULL) == -1)
    ta_log_warn (app->logger, "Unable to install sigaction to catch SIGINT");
}

static void
usage (const char *prname)
{
  printf ("Usage: %s [--jid=JID --password=PASSWD --host=HOST, --port=PORT, --config-file=FILE]\n",
          prname);
}

int
main (int argc, char **argv)
{
  bitu_app_t *app;
  bitu_server_t *server;
  ta_list_t *conf;
  ta_list_t *commands = NULL, *tmp = NULL;
  char *jid = NULL, *passwd = NULL, *host = NULL;
  char *config_file = NULL, *sock_path = NULL;
  int port = 0;
  int daemonize = 0;
  int c;
  static struct option long_options[] = {
    { "jid", required_argument, NULL, 'j' },
    { "password", required_argument, NULL, 'p' },
    { "host", required_argument, NULL, 'H' },
    { "port", required_argument, NULL, 'P' },
    { "server-socket", required_argument, NULL, 's' },
    { "config-file", required_argument, NULL, 'c' },
    { "daemonize", no_argument, NULL, 'd' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0 }
  };

  while ((c = getopt_long (argc, argv, "j:p:H:P:c:d",
                           long_options, NULL)) != -1)
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

        case 's':
          sock_path = strdup (optarg);
          break;

        case 'c':
          config_file = strdup (optarg);
          break;

        case 'h':
          usage (argv[0]);
          exit (EXIT_SUCCESS);
          break;

        case 'd':
          daemonize = 1;
          break;

        default:
          fprintf (stderr, "Try `%s --help' for more information\n", argv[0]);
          exit (EXIT_FAILURE);
          break;
        }
    }

  /* Reading configuration from file */
  while (config_file != NULL)
    {
      glob_t globbuf;
      size_t globi;
      if (glob (config_file, GLOB_NOSORT | GLOB_ERR, NULL, &globbuf) != 0)
        {
          fprintf (stderr, "Warn while loading config file: ");
          fprintf (stderr, "Could not expand glob `%s'\n", config_file);
          free (config_file);
          config_file = NULL;
          break;
        }
      else
        {
          for (globi = 0; globi < globbuf.gl_pathc; globi++)
            {
              char *single_file = globbuf.gl_pathv[globi];
              if ((conf = bitu_conf_read_from_file (single_file)) == NULL)
                exit (EXIT_FAILURE);
            }
        }
      globfree (&globbuf);
      free (config_file);
      config_file = NULL;

      for (tmp = conf; tmp; tmp = tmp->next)
        {
          bitu_conf_entry_t *entry;
          entry = tmp->data;
          if (strcmp (entry->cmd, "jid") == 0 && jid == NULL)
            jid = entry->params[0];
          else if (strcmp (entry->cmd, "password") == 0 && passwd == NULL)
            passwd = entry->params[0];
          else if (strcmp (entry->cmd, "host") == 0 && host == NULL)
            host = entry->params[0];
          else if (strcmp (entry->cmd, "port") == 0 && port == 0)
            port = atoi (entry->params[0]);
          else if (strcmp (entry->cmd, "server-sock-path") == 0)
            sock_path = entry->params[0];
          else if (strcmp (entry->cmd, "include") == 0)
            config_file = strdup (entry->params[0]);
          else
            commands = ta_list_append (commands, entry);
        }
    }

  /* Some param validation */
  if (jid == NULL || passwd == NULL)
    {
      usage (argv[0]);
      ta_list_free (commands);
      exit (EXIT_FAILURE);
    }
  if (host == NULL)
    {
      ikstack *stack = iks_stack_new (255, 64);
      iksid *id = iks_id_new (stack, jid);
      host = strdup (id->server);
      iks_stack_delete (stack);
    }
  if (sock_path == NULL)
    sock_path = strdup (SOCKET_PATH);
  if (port == 0)
    port = 5222;                /* Default xmpp port */

  /* Allocating the application context */
  if ((app = malloc (sizeof (bitu_app_t))) == NULL)
    {
      ta_list_free (commands);
      exit (EXIT_FAILURE);
    }

  /* Preparing log file stuff */
  app->logfile = NULL;
  app->logfd = -1;
  app->logflags = 0;

  /* Preparing the plugin context */
  app->plugin_ctx = bitu_plugin_ctx_new ();

  /* Configuring xmpp client */
  app->xmpp = ta_xmpp_client_new (jid, passwd, host, port);

  free (jid);
  free (passwd);
  free (host);

  ta_xmpp_client_event_connect (app->xmpp, "connected",
                                (ta_xmpp_client_hook_t) connected_cb,
                                NULL);
  ta_xmpp_client_event_connect (app->xmpp, "authenticated",
                                (ta_xmpp_client_hook_t) auth_cb,
                                NULL);
  ta_xmpp_client_event_connect (app->xmpp, "authentication-failed",
                                (ta_xmpp_client_hook_t) auth_failed_cb,
                                NULL);
  ta_xmpp_client_event_connect (app->xmpp, "message-received",
                                (ta_xmpp_client_hook_t) message_received_cb,
                                app->plugin_ctx);
  ta_xmpp_client_event_connect (app->xmpp, "presence-noticed",
                                (ta_xmpp_client_hook_t) presence_noticed_cb,
                                app->plugin_ctx);

  /* main application logger */
  app->logger = ta_log_new ("bitu-main");

  /* Initializing the local server that will handle configuration
   * interaction. */
  server = bitu_server_new (sock_path, app);
  main_server = server;
  free (sock_path);

  /* Function that install the sigaction that will handle signals */
  _setup_sigaction (app);

  /* Running all commands found in the config file. */
  for (tmp = commands; tmp; tmp = tmp->next)
    {
      bitu_conf_entry_t *entry;
      char *answer = NULL;
      int answer_size;
      entry = (bitu_conf_entry_t *) tmp->data;
      bitu_server_exec_cmd (server, entry->cmd, entry->params,
                            entry->nparams, &answer_size);
      if (answer && answer_size > 1)
        ta_log_warn (app->logger, "Warn running command %s: %s",
                     entry->cmd, answer);
      free (answer);
    }
  ta_list_free (commands);

  /* Sending the rest of the program to the background if requested */
  if (daemonize)
    {
      ta_log_info (app->logger, "Going to the background, as requested");
      if (daemon (0, 0) == -1)
        {
          ta_log_warn (app->logger, "Unable to background the process: %s",
                       strerror (errno));
          exit (EXIT_FAILURE);
        }
    }

  /* Connecting */
  if (!ta_xmpp_client_connect (app->xmpp))
    {
      ta_error_t *error;
      error = ta_xmpp_client_get_error (app->xmpp);
      fprintf (stderr, "%s: %s\n", ta_error_get_name (error),
               ta_error_get_message (error));
      goto finalize;
      return 1;
    }

  /* Running client */
  if (!ta_xmpp_client_run (app->xmpp, 1))
    {
      ta_error_t *error;
      error = ta_xmpp_client_get_error (app->xmpp);
      fprintf (stderr, "%s: %s\n", ta_error_get_name (error),
               ta_error_get_message (error));
      goto finalize;
      return 1;
    }

  bitu_server_connect (server);
  bitu_server_run (server);

 finalize:
  bitu_plugin_ctx_free (app->plugin_ctx);
  ta_xmpp_client_free (app->xmpp);
  ta_log_free (app->logger);
  if (app->logfile)
    free (app->logfile);
  if (app->logfd > 0)
    close (app->logfd);
  free (app);

  return 0;
}
