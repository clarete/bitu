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
#include <bitu/server.h>

typedef struct bitu_transport bitu_transport_t;
struct bitu_transport
{
  ta_iri_t *uri;
  bitu_server_t *server;
  void *data;
  const char *(*protocol) (void);
  int (*connect) (bitu_transport_t *transport);
  int (*run) (bitu_transport_t *transport);
};


bitu_transport_t *bitu_transport_new (bitu_server_t *server, const char *uri);
int bitu_transport_connect (bitu_transport_t *transport);
int bitu_transport_run (bitu_transport_t *transport);

/* Forward declarations for transports */
extern bitu_transport_t *_bitu_xmpp_transport (bitu_server_t *server, ta_iri_t *uri);

#endif  /* BITU_TRANSPORT_H_ */
