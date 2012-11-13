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

typedef struct bitu_queue bitu_queue_t;
typedef struct bitu_conn_manager bitu_conn_manager_t;
typedef struct bitu_transport bitu_transport_t;
typedef struct bitu_command bitu_command_t;

typedef enum
{
  BITU_CONN_STATUS_OK,
  BITU_CONN_STATUS_ERROR,
  BITU_CONN_STATUS_TRANSPORT_NOT_FOUND,
  BITU_CONN_STATUS_CONNECTION_FAILED,
  BITU_CONN_STATUS_ALREADY_RUNNING,
  BITU_CONN_STATUS_RUNNING_FAILED
} bitu_conn_status_t;


/* Queue api */
typedef int (*bitu_queue_callback_consume_t) (void *data, void *extra_data);

bitu_queue_t *bitu_queue_new (void);
void bitu_queue_free (bitu_queue_t *queue);
void bitu_queue_add (bitu_queue_t *queue, void *data);
void bitu_queue_consume (bitu_queue_t *queue,
                         bitu_queue_callback_consume_t callback,
                         void *data);


/* Transport manager API */
bitu_conn_manager_t *bitu_conn_manager_new (void);
bitu_transport_t *bitu_conn_manager_add (bitu_conn_manager_t *manager, const char *uri);
void bitu_conn_manager_remove (bitu_conn_manager_t *manager, const char *uri);
int bitu_conn_manager_get_n_transports (bitu_conn_manager_t *manager);
ta_list_t *bitu_conn_manager_get_transports (bitu_conn_manager_t *manager);
bitu_transport_t *bitu_conn_manager_get_transport (bitu_conn_manager_t *manager, const char *uri);
bitu_conn_status_t bitu_conn_manager_run (bitu_conn_manager_t *manager, const char *uri);
void bitu_conn_manager_consume (bitu_conn_manager_t *manager,
                                bitu_queue_callback_consume_t callback,
                                void *data);


/* Transport api */
typedef int (*bitu_transport_callback_connect_t) (bitu_transport_t *transport);
typedef int (*bitu_transport_callback_run_t) (bitu_transport_t *transport);
typedef int (*bitu_transport_callback_send_t) (bitu_transport_t *transport,
                                               const char *msg, const char *to);

bitu_transport_t *bitu_transport_new (const char *uri);
ta_iri_t *bitu_transport_get_uri (bitu_transport_t *transport);
int bitu_transport_connect (bitu_transport_t *transport);
int bitu_transport_run (bitu_transport_t *transport);
void *bitu_transport_get_data (bitu_transport_t *transport);
void bitu_transport_set_data (bitu_transport_t *transport, void *data);
void bitu_transport_set_callback_connect (bitu_transport_t *transport,
                                          bitu_transport_callback_connect_t callback);
void bitu_transport_set_callback_run (bitu_transport_t *transport,
                                      bitu_transport_callback_run_t callback);
void bitu_transport_set_callback_send (bitu_transport_t *transport,
                                       bitu_transport_callback_send_t callback);
void bitu_transport_queue_command (bitu_transport_t *transport, bitu_command_t *cmd);
int bitu_transport_send (bitu_transport_t *transport, const char *msg, const char *to);


/* Command api */
bitu_command_t *bitu_command_new (bitu_transport_t *transport,
                                  const char *cmd, const char *from);
void bitu_command_free (bitu_command_t *command);
bitu_transport_t *bitu_command_get_transport (bitu_command_t *command);
const char *bitu_command_get_cmd (bitu_command_t *command);
const char *bitu_command_get_from (bitu_command_t *command);


/* Forward declarations for transports */
extern int _bitu_xmpp_transport (bitu_transport_t *transport);
extern int _bitu_irc_transport (bitu_transport_t *transport);

#endif  /* BITU_TRANSPORT_H_ */
