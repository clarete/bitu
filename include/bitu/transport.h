/* transport.c - This file is part of the bitu program
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

#ifndef BITU_TRANSPORT_H_
#define BITU_TRANSPORT_H_ 1

#include <taningia/taningia.h>

typedef struct bitu_conn_manager bitu_conn_manager_t;
typedef struct bitu_transport bitu_transport_t;
struct bitu_transport
{
  ta_iri_t *uri;
  void *data;
  const char *(*protocol) (void);
  int (*connect) (bitu_transport_t *transport);
  int (*run) (bitu_transport_t *transport);
};

typedef enum
{
  BITU_CONN_STATUS_OK,
  BITU_CONN_STATUS_ERROR,
  BITU_CONN_STATUS_TRANSPORT_NOT_FOUND,
  BITU_CONN_STATUS_CONNECTION_FAILED,
  BITU_CONN_STATUS_ALREADY_RUNNING,
  BITU_CONN_STATUS_RUNNING_FAILED
} bitu_conn_status_t;


/* Transport api */
bitu_transport_t *bitu_transport_new (const char *uri);
int bitu_transport_connect (bitu_transport_t *transport);
int bitu_transport_run (bitu_transport_t *transport);


/* Transport manager API */
bitu_conn_manager_t *bitu_conn_manager_new (void);
bitu_transport_t *bitu_conn_manager_add (bitu_conn_manager_t *manager, const char *uri);
void bitu_conn_manager_remove (bitu_conn_manager_t *manager, const char *uri);
int bitu_conn_manager_get_n_transports (bitu_conn_manager_t *manager);
ta_list_t *bitu_conn_manager_get_transports (bitu_conn_manager_t *manager);
bitu_transport_t *bitu_conn_manager_get_transport (bitu_conn_manager_t *manager, const char *uri);
bitu_conn_status_t bitu_conn_manager_run (bitu_conn_manager_t *manager, const char *uri);

/* Forward declarations for transports */
extern bitu_transport_t *_bitu_xmpp_transport (ta_iri_t *uri);
extern bitu_transport_t *_bitu_irc_transport (ta_iri_t *uri);

#endif  /* BITU_TRANSPORT_H_ */
