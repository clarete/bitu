/* app.h - This file is part of the bitu program
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

#ifndef BITU_APP_H_
#define BITU_APP_H_ 1

#include <taningia/taningia.h>
#include <bitu/transport.h>
#include <bitu/loader.h>
#include <bitu/server.h>

#include "hashtable.h"

typedef struct {
  /* The main components */
  hashtable_t *environment;
  hashtable_t *commands;
  bitu_conn_manager_t *connections;
  bitu_plugin_ctx_t *plugin_ctx;

  /* Logging stuff */
  ta_log_t *logger;
  char *logfile;
  int logfd;
  int logflags;
} bitu_app_t;


bitu_app_t *bitu_app_new (void);
void bitu_app_free (bitu_app_t *app);
int bitu_app_load_config (bitu_app_t *app, ta_list_t *config);
int bitu_app_dump_config (bitu_app_t *app);
int bitu_app_exec_command (bitu_app_t *app, bitu_command_t *command, char **output);
int bitu_app_run_transports (bitu_app_t *app);

#endif /* BITU_APP_H_ */
