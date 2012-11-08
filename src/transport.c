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
#include <string.h>
#include <bitu/transport.h>
#include <bitu/errors.h>
#include <taningia/taningia.h>


bitu_transport_t *
bitu_transport_new (bitu_server_t *server, const char *uri)
{
  bitu_transport_t *result;
  ta_iri_t *uri_obj;
  const char *scheme;

  uri_obj = ta_iri_new ();
  if (ta_iri_set_from_string (uri_obj, uri) != TA_OK)
    return NULL;

  if ((scheme = ta_iri_get_scheme (uri_obj)) == NULL)
    {
      ta_error_set (BITU_ERROR_TRANSPORT_INVALID_URL,
                    "The url provided is not a valid iri, "
                    "you must provide a scheme");
      return NULL;
    }

  /* Looking for the right transport. Possible values hardcoded by
   * now */
  if (strcmp (scheme, "xmpp") == 0)
    result = _bitu_xmpp_transport (server, uri_obj);
  /* else if (strcmp (scheme, "irc") == 0) */
  /*   result = _bitu_irc_transport (uri_obj); */
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
  return transport->run (transport);
}
