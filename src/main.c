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
#include <taningia/taningia.h>
#include <bitu/app.h>
#include <bitu/util.h>
#include <bitu/loader.h>
#include <bitu/server.h>
#include <bitu/conf.h>
#include <bitu/transport.h>

#ifdef __APPLE__
#undef daemon
extern int daemon(int, int);
#endif

#define SOCKET_PATH    "/tmp/bitu.sock"

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
  printf ("  -t,--transport=URI\t\t: URI to connect to a server it "
          "can be a valid JID but must start with 'xmpp:' or an irc uri\n\n");
  printf ("Server Options:\n");
  printf ("  -s,--server-socket=PATH\t: UNIX socket path of the config "
          "server\n");
  printf ("  -i,--pid-file=PATH\t\t: Path to store the pid. Useful when "
          "running as daemon\n");
  printf ("  -d,--daemonize\t\t: Send the program to background after "
          "starting\n\n");
  printf ("Report bugs to " PACKAGE_BUGREPORT "\n\n");
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


static
int _exec_command (void *data, void *extra_data)
{
  bitu_command_t *command = (bitu_command_t *) data;
  bitu_server_t *server = (bitu_server_t *) extra_data;
  char *cmd = NULL, *message = NULL;
  char **params = NULL;
  int i, len;

  if (!bitu_util_extract_params (bitu_command_get_cmd (command), &cmd, &params, &len))
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

  /* Actually sending the stuff */
  bitu_transport_send (bitu_command_get_transport (command),
                       message,
                       bitu_command_get_from (command));

  /* Cleaning up the command and the message */
  bitu_command_free (command);
  free (message);

  /* Freeing the command and all parameters collected */
  for (i = 0; i < len; i++)
    free (params[i]);
  free (params);
  if (cmd)
    free (cmd);

  return 0;
}


int
main (int argc, char **argv)
{
  bitu_app_t *app;
  bitu_server_t *server;
  ta_list_t *conf = NULL, *transports = NULL;
  ta_list_t *commands = NULL, *tmp = NULL;
  bitu_conn_manager_t *connections = NULL;
  char *config_file = NULL, *sock_path = NULL;
  char *pid_file = NULL;
  int daemonize = 0;
  int c;
  static struct option long_options[] = {
    { "transport", required_argument, NULL, 't' },
    { "server-socket", required_argument, NULL, 's' },
    { "pid-file", required_argument, NULL, 'i' },
    { "config-file", required_argument, NULL, 'c' },
    { "daemonize", no_argument, NULL, 'd' },
    { "version", no_argument, NULL, 'v' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0 }
  };

  ta_global_state_setup ();

  connections = bitu_conn_manager_new ();

  while ((c = getopt_long (argc, argv, "t:c:dhvi:",
                           long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 't':
          bitu_conn_manager_add (connections, optarg);
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
          if (strcmp (entry->cmd, "transport") == 0)
            bitu_conn_manager_add (connections, entry->params[0]);
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
  if (bitu_conn_manager_get_n_transports (connections) == 0)
    {
      usage (argv[0]);
      ta_list_free (commands);
      exit (EXIT_FAILURE);
    }
  if (sock_path == NULL)
    sock_path = strdup (SOCKET_PATH);

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

  /* List of connections */
  app->connections = connections;

  /* Preparing the plugin context */
  app->plugin_ctx = bitu_plugin_ctx_new ();

  /* main application logger */
  app->logger = ta_log_new ("bitu-main");

  /* Initializing the local server that will handle configuration
   * interaction. */
  server = bitu_server_new (sock_path, app);
  free (sock_path);

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

  /* Walking through all the transports found and trying to connect and
   * run them. */
  transports = bitu_conn_manager_get_transports (connections);
  for (tmp = transports; tmp; tmp = tmp->next)
    if ((bitu_conn_manager_run (connections, tmp->data)) != BITU_CONN_STATUS_OK)
      ta_log_warn (app->logger, "Transport `%s' not initialized", tmp->data);
  ta_list_free (transports);
  bitu_conn_manager_consume (connections, (bitu_queue_callback_consume_t) _exec_command, server);

  /* Connecting our local management server */
  if (bitu_server_connect (server) == TA_OK)
    bitu_server_run (server);

  /* Cleaning things up after shutting the server down */
  if (pid_file != NULL && access (pid_file, X_OK) == 0)
    {
      unlink (pid_file);
      free (pid_file);
    }

  ta_global_state_teardown ();

  return 0;
}
