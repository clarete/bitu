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
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <glob.h>
#include <taningia/taningia.h>
#include <bitu/util.h>
#include <bitu/loader.h>
#include <bitu/conf.h>
#include <bitu/transport.h>

#include "app.h"

#ifdef __APPLE__
#undef daemon
extern int daemon(int, int);
#endif


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


static void
_signal_handler (int sig, siginfo_t *si, void *data)
{
  switch (sig)
    {
    case SIGPIPE:
      /* Doing nothing here. Just avoiding the default behaviour that
       * kills the program. */
      break;

      /* Add more signals here to handle them when needed. */

    default:
      break;
    }
}


static void
_setup_sigaction (bitu_app_t *app)
{
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = _signal_handler;
  sigemptyset (&sa.sa_mask);

  if (sigaction (SIGPIPE, &sa, NULL) == -1)
    ta_log_warn (app->logger, "Unable to install sigaction to catch SIGPIPE");
}

static
int _exec_command (void *data, void *extra_data)
{
  bitu_command_t *command = (bitu_command_t *) data;
  char *line = NULL, *stripped = NULL;
  char *cmd = NULL, *message = NULL;
  char **params = NULL;
  int i, len;

  /* Trim performes changes in place, we need to dup it before to
   * avoid leaking data when we free it */
  line = strdup (bitu_command_get_cmd (command));
  stripped = bitu_util_strstrip (line);

  /* Handling commands, no need to mess with plugins */
  if (stripped[0] == '/')
    {
      if (bitu_util_extract_params (stripped, &cmd, &params, &len) != TA_OK)
        printf ("I should execute an internal command\n");
    }
  else
    {
      printf ("I should execute a plugin\n");
      /* TODO: Try to find the plugin */
      //bitu_plugin_t *plugin = bitu_plugin_ctx_find_for_cmdline (
    }


  /* No answer was returned */
  if (message == NULL)
    message = strdup ("The plugin returned nothing");

  /* Actually sending the stuff */
  if (bitu_transport_send (bitu_command_get_transport (command),
                           message,
                           bitu_command_get_from (command)) != TA_OK)
    /* ta_log_warn (app->logger, */
    /*              "Unable to send a message to the user %s", */
    /*              bitu_command_get_from (command)); */
    ;

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
  ta_list_t *conf = NULL;
  ta_list_t *commands = NULL, *tmp = NULL;
  char *config_file = NULL;
  char *pid_file = NULL;
  int daemonize = 0;
  int c;
  static struct option long_options[] = {
    { "transport", required_argument, NULL, 't' },
    { "pid-file", required_argument, NULL, 'i' },
    { "config-file", required_argument, NULL, 'c' },
    { "daemonize", no_argument, NULL, 'd' },
    { "version", no_argument, NULL, 'v' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0 }
  };

  ta_global_state_setup ();

  _setup_sigaction (NULL);

  while ((c = getopt_long (argc, argv, "t:c:dhvi:",
                           long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 't':
          /* bitu_conn_manager_add (connections, optarg); */
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
          const char *name;
          bitu_command_t *command;

          command = tmp->data;
          name = bitu_command_get_name (command);

          if (strcmp (name, "pid-file") == 0 && pid_file == NULL)
            pid_file = strdup ((bitu_command_get_params (command))[0]);
          else if (strcmp (name, "include") == 0)
            config_file = strdup ((bitu_command_get_params (command))[0]);
          else
            commands = ta_list_append (commands, command);
        }
    }

  /* Creating the app. We'll need a logger sooner than the rest of the
   * things here */
  app = bitu_app_new ();

  /* Loading the configuration file into the app. It's going to execute
   * all the commands declared in the config file. */
  bitu_app_load_config (app, commands);
  ta_list_free (commands);

  /* Running the transports */
  int rolou = bitu_app_run_transports (app);

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

  /* Our current main loop is this crap, sorry. */
  if (rolou == TA_OK)
    while (1)
      sleep (1);

  /* Cleaning things up after shutting the server down */
  if (pid_file != NULL && access (pid_file, X_OK) == 0)
    {
      unlink (pid_file);
      free (pid_file);
    }

  ta_global_state_teardown ();

  return 0;
}
