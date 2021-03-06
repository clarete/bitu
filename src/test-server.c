/* test-server.c - This file is part of the bitu program
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <taningia/taningia.h>
#include <bitu/server.h>
#include <bitu/loader.h>

int
main ()
{
  bitu_server_t *server;
  bitu_server_callbacks_t callbacks;

  memset (&callbacks, 0, sizeof (callbacks));
  server = bitu_server_new ("/tmp/bleh.sock", callbacks);
  bitu_server_connect (server);

  /* I don't like this way. I think we should have a
   * `bitu_server_recv()' method and let the user to decide when
   * calling it (to be able to select/polling). Soon it will be
   * done */
  bitu_server_run (server);

  /* Time to free everything related to the server and app stuff. */
  bitu_server_free (server);    /* app will not be touched here */

  /* We're done =P */
  return 0;
}
