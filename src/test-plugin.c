/* test-plugin.c - This file is part of the bitu program
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
#include <bitu/loader.h>

/* Little program to test the plugin loader.
 *
 * In the lines bellow, you'll see how it works. It is quite simple,
 * actually. The user must instantiate a `bitu_plugin_ctx_t' instance
 * and then ask it to load a plugin, using `bitu_plugin_ctx_load()'
 * and passing the dynamic library file name a.k.a. the .so file name.
 *
 * After loading a plugin, its file will be opened til it is freed. If
 * you load a plugin using the plugin_ctx stuff you should not free it
 * manually.
 *
 * Our plugin interface counst, currently, with three methods:
 * `plugin_name()', `plugin_num_params()' (currently not used) and
 * `plugin_message_return()'.
 */

int
main (int argc, char **argv)
{
  bitu_plugin_t *plugin;
  bitu_plugin_ctx_t *plugin_ctx;

  plugin_ctx = bitu_plugin_ctx_new ();

  /* `dlopen()' will look at default lib paths and in LD_LIBRARY_PATH
   * if it is set. */
  bitu_plugin_ctx_load (plugin_ctx, "libprocessor.so");

  /* Find a plugin by its name. No need to load it by hand through the
   * plugin API. This is already loaded, only using it here. */
  plugin = bitu_plugin_ctx_find (plugin_ctx, "processor-info");

  /* `bitu_plugin_ctx_find()' returns NULL when the plugin is not
   * found and a message explaining why is printed out in stderr. */
  if (plugin)
    {
      char *message;
      message = bitu_plugin_message_return (plugin);
      printf ("Name: %s, num_params: %d: message:\n%s",
              bitu_plugin_name (plugin),
              bitu_plugin_num_params (plugin),
              message);
      free (message);
    }

  /* This call will free the loaded plugin too. `dlclose()' will be
   * called here for each loaded plugin. All of them keep open til
   * this line. */
  bitu_plugin_ctx_free (plugin_ctx);
  return 0;
}
