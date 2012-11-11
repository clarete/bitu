/* transport.c - This file is part of the bitu program
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <taningia/taningia.h>
#include <bitu/util.h>
#include <bitu/errors.h>
#include <bitu/transport.h>

#include "hashtable.h"
#include "hashtable-utils.h"


struct bitu_conn_manager
{
  hashtable_t *transports;
};


static void
_bitu_transport_free (bitu_transport_t *transport)
{
  ta_object_unref (transport->uri);
  free (transport);
}


bitu_conn_manager_t *
bitu_conn_manager_new (void)
{
  bitu_conn_manager_t *manager = malloc (sizeof (bitu_conn_manager_t));
  manager->transports =
    hashtable_create (hash_string,
                      string_equal,
                      (free_fn) free,
                      (free_fn) _bitu_transport_free);
  return manager;
}


bitu_transport_t *
bitu_conn_manager_add (bitu_conn_manager_t *manager, const char *uri)
{
  bitu_transport_t *transport;
  if ((transport = bitu_transport_new (uri)) == NULL)
    return NULL;
  hashtable_set (manager->transports, strdup (uri), transport);
  return transport;
}


void
bitu_conn_manager_remove (bitu_conn_manager_t *manager, const char *uri)
{
  /* This will also free the transport using the _bitu_transport_free()
   * function, assigned in the creation of the hash table. */
  hashtable_del (manager->transports, uri);
}

int
bitu_conn_manager_get_n_transports (bitu_conn_manager_t *manager)
{
  return manager->transports->size;
}

ta_list_t *
bitu_conn_manager_get_transports (bitu_conn_manager_t *manager)
{
  void *iter;
  ta_list_t *keys = NULL;
  if ((iter = hashtable_iter (manager->transports)) == NULL)
    return NULL;
  do
    keys = ta_list_append (keys, hashtable_iter_key (iter));
  while ((iter = hashtable_iter_next (manager->transports, iter)));
  return keys;
}


bitu_transport_t *
bitu_conn_manager_get_transport (bitu_conn_manager_t *manager, const char *uri)
{
  bitu_transport_t *transport;
  transport = hashtable_get (manager->transports, uri);
  return transport;
}


bitu_conn_status_t
bitu_conn_manager_run (bitu_conn_manager_t *manager, const char *uri)
{
  bitu_transport_t *transport;
  if ((transport = bitu_conn_manager_get_transport (manager, uri)) == NULL)
    return BITU_CONN_STATUS_TRANSPORT_NOT_FOUND;

  /* Connecting && running the client */
  if (bitu_transport_connect (transport) == TA_OK)
    {
      if (bitu_transport_run (transport) == TA_OK)
        return BITU_CONN_STATUS_OK;
      else
        return BITU_CONN_STATUS_ERROR;
    }
  return BITU_CONN_STATUS_CONNECTION_FAILED;
}

/* The transport api */


bitu_transport_t *
bitu_transport_new (const char *uri)
{
  bitu_transport_t *result;
  ta_iri_t *uri_obj;
  const char *scheme;

  /* Parsing the given uri */
  uri_obj = ta_iri_new ();
  if (ta_iri_set_from_string (uri_obj, uri) != TA_OK)
    return NULL;

  /* No schema, no game here */
  if ((scheme = ta_iri_get_scheme (uri_obj)) == NULL)
    {
      ta_error_set (BITU_ERROR_TRANSPORT_INVALID_URL,
                    "You must provide a scheme, like xmpp://");
      return NULL;
    }

  /* Looking for the right transport. Possible values hardcoded by
   * now */
  if (strcmp (scheme, "xmpp") == 0)
    result = _bitu_xmpp_transport (uri_obj);
  else if (strcmp (scheme, "irc") == 0)
    result = _bitu_irc_transport (uri_obj);
  else
    {
      ta_error_set (BITU_ERROR_TRANSPORT_NOT_SUPPORTED,
                    "There is no transport to handle the protocol %s",
                    scheme);
      return NULL;
    }
  return result;
}


int
bitu_transport_connect (bitu_transport_t *transport)
{
  return transport->connect (transport);
}


int
bitu_transport_run (bitu_transport_t *transport)
{
  bitu_util_start_new_thread ((bitu_util_callback_t) transport->run, transport);
  return TA_OK;
}
