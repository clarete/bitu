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
#include <taningia/taningia.h>
#include <bitu/server.h>
#include <bitu/app.h>
#include <bitu/loader.h>

int
main (int argc, char **argv)
{
  bitu_app_t *app;

  /* We actually don't have a constructor (nor destructor) to the app
   * struct.  This is a test program, the only app struct instantiated
   * should be placed in the main call of the whole program. Look at
   * the `main.c' file for this. */
  if ((app = malloc (sizeof (bitu_app_t))) == NULL)
    return EXIT_FAILURE;

  /* Here I'm filling all fields of the app struct. */
  app->logger = ta_log_new ("test-server");
  app->xmpp = ta_xmpp_client_new ("jid@invalid", "fakepasswd", NULL, 5222);
  app->plugin_ctx = bitu_plugin_ctx_new ();

  /* I don't like this way. I think we should have a
   * `bitu_server_recv()' method and let the user to decide when
   * calling it (to be able to select/polling). Soon it will be
   * done */
  bitu_server_run (app);

  /* Time to free everything related to the app stuff. */
  ta_log_free (app->logger);
  ta_xmpp_client_free (app->xmpp);
  bitu_plugin_ctx_free (app->plugin_ctx);
  free (app);

  /* We're done =P */
  return 0;
}
