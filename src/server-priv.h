/* server-priv.h - This file is part of the bitu program
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

#ifndef BITU_SERVER_PRIV_H_
#define BITU_SERVER_PRIV_H_ 1

#include <taningia/taningia.h>
#include "hashtable.h"

struct bitu_server
{
  ta_log_t *logger;
  char *sock_path;
  int sock;
  hashtable_t *clients;
  bitu_server_callbacks_t callbacks;
  const char *data;
  int can_run;
};


typedef struct
{
  char *id;
  int socket;
} bitu_client_t;


/* Client API */
bitu_client_t *bitu_client_new (int socket);
void bitu_client_free (bitu_client_t *client);
const char *bitu_client_get_id (bitu_client_t *client);
int bitu_client_get_socket (bitu_client_t *client);

#endif  /* BITU_SERVER_PRIV_H_ */
