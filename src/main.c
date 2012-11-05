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

#ifdef __APPLE__
/* Not sure why, but apple folks deprecated the `daemon()' function. It
 * still works pretty good here, so I'll just ignore this fact and go
 * on. In the end the include directives, It is enabled it again.
 */
#define daemon fake_daemon
#endif

#define _GNU_SOURCE
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <glob.h>
#include <iksemel.h>
#include <taningia/taningia.h>
#include <taningia/srv.h>
#include <bitu/app.h>
#include <bitu/util.h>
#include <bitu/loader.h>
#include <bitu/server.h>
#include <bitu/conf.h>

#ifdef __APPLE__
#undef daemon
extern int daemon(int, int);
#endif

#define SOCKET_PATH    "/tmp/bitu.sock"


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

static void
_execute_plugin (bitu_server_t *server,
                 char *cmd,
                 char **params,
                 int len,
                 char **message)
{
  bitu_app_t *app;
  bitu_plugin_ctx_t *plugin_ctx;
  bitu_plugin_t *plugin;
  int msgbufsize = 128;

  app = bitu_server_get_app (server);
  plugin_ctx = app->plugin_ctx;

  if ((plugin = bitu_plugin_ctx_find (plugin_ctx, cmd)) == NULL)
    {
      *message = malloc (msgbufsize);
      snprintf (*message, msgbufsize, "Plugin `%s' not found", cmd);
    }
  else
    {
      /* Validating number of parameters */
      if (bitu_plugin_num_params (plugin) != len)
        {
          *message = malloc (msgbufsize);
          snprintf (*message, msgbufsize,
                    "Wrong number of parameters. `%s' "
                    "receives %d but %d were passed", cmd,
                    bitu_plugin_num_params (plugin),
                    len);
        }
      else
        *message = bitu_plugin_execute (plugin, params);
    }
}

static void
_execute_command (bitu_server_t *server,
                  char *cmd,
                  char **params,
                  int len,
                  char **message)
{
  char *c = cmd;
  /* skipping the '/' */
  *message = bitu_server_exec_cmd (server, &(*++c), params, len, NULL);
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
        _execute_command (server, cmd, params, len, &message);
      else
        _execute_plugin (server, cmd, params, len, &message);
    }

  /* Feeding the user back */
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
  fprintf (stderr,"Someone wants to talk with me... "
           "I'm not sure I should =(\n");
  fprintf (stderr, "His/her name/jid is `%s'.\nThe sysadmin will be wise "
           "enough to decide if I should talk with this new buddy\n",
           pak->from->full);
  return 0;
}


/* Save the pid file */
static void
_save_pid (bitu_app_t *app, const char *pid_file)
{
  struct stat st;
  int pidfd, stresult;
  pid_t pid;
  char pidbuf[32];
  size_t pidlen;

  stresult = stat (pid_file, &st);
  if (stresult == -1 && errno != ENOENT)
    {
      ta_log_warn (app->logger, "Unable to stat pid file: %s",
                   strerror (errno));
      return;
    }
  if ((pidfd = open (pid_file, O_WRONLY | O_CREAT,
                     S_IRUSR | S_IWUSR | S_IRGRP)) == -1)
    {
      ta_log_warn (app->logger, "Unable to write pid to file `%s': %s",
                   pid_file, strerror (errno));
      return;
    }

  pid = getpid ();
  pidlen = snprintf (pidbuf, 32, "%d", pid);
  write (pidfd, pidbuf, pidlen);
  close (pidfd);
}

static void
usage (const char *prname)
{
  printf ("Usage: %s [OPTIONS]\n", prname);
  printf ("  All connection and server (except daemonize) can be described\n");
  printf ("  in a config file. You will need to inform, at least, jid and\n");
  printf ("  password to start bitU.\n\n");
  printf ("General Options:\n");
  printf ("  -c,--config-file=FILE\t\t: Path to the config file\n");
  printf ("  -v,--version\t\t\t: Show current version and exit\n");
  printf ("  -h,--help\t\t\t: Shows this help\n\n");
  printf ("Connection Options:\n");
  printf ("  -j,--jid=JID\t\t\t: Valid and already registered JID\n");
  printf ("  -p,--password=PASSWD\t\t: Jabber password\n");
  printf ("  -H,--host=HOST\t\t: Host addr, use only if host does not support "
          "SRV records\n");
  printf ("  -P,--port=PORT\t\t: Port number. Defaults to 5222\n\n");
  printf ("Server Options:\n");
  printf ("  -s,--server-socket=PATH\t: UNIX socket path of the config "
          "server\n");
  printf ("  -i,--pid-file=PATH\t\t: Path to store the pid. Useful when "
          "running as daemon\n");
  printf ("  -d,--daemonize\t\t: Send the program to background after "
          "starting\n\n");
  printf ("Report bugs to " PACKAGE_BUGREPORT "\n\n");
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

int
main (int argc, char **argv)
{
  bitu_app_t *app;
  bitu_server_t *server;
  ta_list_t *conf;
  ta_list_t *commands = NULL, *tmp = NULL;
  char *jid = NULL, *passwd = NULL, *host = NULL;
  char *config_file = NULL, *sock_path = NULL;
  char *pid_file = NULL;
  int port = 0;
  int daemonize = 0;
  int c;
  static struct option long_options[] = {
    { "jid", required_argument, NULL, 'j' },
    { "password", required_argument, NULL, 'p' },
    { "host", required_argument, NULL, 'H' },
    { "port", required_argument, NULL, 'P' },
    { "server-socket", required_argument, NULL, 's' },
    { "pid-file", required_argument, NULL, 'i' },
    { "config-file", required_argument, NULL, 'c' },
    { "daemonize", no_argument, NULL, 'd' },
    { "version", no_argument, NULL, 'v' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0 }
  };

  ta_global_state_setup ();

  while ((c = getopt_long (argc, argv, "j:p:H:P:c:dhvi:",
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

        case 'i':
          pid_file = strdup (optarg);
          break;

        case 'h':
          usage (argv[0]);
          exit (EXIT_SUCCESS);
          break;

        case 'v':
          printf ("bitU v" VERSION " \n");
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
          else if (strcmp (entry->cmd, "pid-file") == 0 && pid_file == NULL)
            pid_file = strdup (entry->params[0]);
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

      /* Trying to resolve the srv name */
      ta_srv_init ();
      _query_service (id->server, &host, &port);
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

  /* main application logger */
  app->logger = ta_log_new ("bitu-main");

  /* Initializing the local server that will handle configuration
   * interaction. */
  server = bitu_server_new (sock_path, app);
  free (sock_path);

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
                                server);
  ta_xmpp_client_event_connect (app->xmpp, "presence-noticed",
                                (ta_xmpp_client_hook_t) presence_noticed_cb,
                                app->plugin_ctx);

  /* Running all commands found in the config file. */
  for (tmp = commands; tmp; tmp = tmp->next)
    {
      bitu_conf_entry_t *entry;
      char *answer = NULL;
      int answer_size;
      entry = (bitu_conf_entry_t *) tmp->data;
      answer = bitu_server_exec_cmd (server, entry->cmd, entry->params,
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

  /* Saving the pid file, if requested by any configuration file */
  if (pid_file != NULL)
    _save_pid (app, pid_file);

  /* Connecting */
  if (!ta_xmpp_client_connect (app->xmpp))
    {
      const ta_error_t *error = ta_error_last ();
      fprintf (stderr, "%s\n", error->message);
      goto finalize;
    }

  /* Running client */
  if (!ta_xmpp_client_run (app->xmpp, 1))
    {
      const ta_error_t *error = ta_error_last ();
      fprintf (stderr, "%s\n", error->message);
      goto finalize;
    }

  if (bitu_server_connect (server) == TA_OK)
    bitu_server_run (server);

 finalize:
  if (pid_file != NULL && access (pid_file, X_OK) == 0)
    {
      unlink (pid_file);
      free (pid_file);
    }

  ta_global_state_teardown ();

  return 0;
}
