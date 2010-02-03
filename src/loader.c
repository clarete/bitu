/* loader.c - This file is part of the bitu program
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
#include <dlfcn.h>
#include <bitu/util.h>
#include <bitu/loader.h>
#include "hashtable.h"
#include "hashtable-utils.h"

#define LINELEN_MAX 255

struct bitu_plugin
{
  void *handle;
  const char *(*name) (void);
  int (*num_params) (void);
  char *(*message_return) (void);
};

struct bitu_plugin_ctx
{
  hashtable_t *plugins;
};

const char *
bitu_plugin_name (bitu_plugin_t *plugin)
{
  return plugin->name ();
}

int
bitu_plugin_num_params (bitu_plugin_t *plugin)
{
  return plugin->num_params ();
}

char *
bitu_plugin_message_return (bitu_plugin_t *plugin)
{
  return plugin->message_return ();
}

void
bitu_plugin_free (bitu_plugin_t *plugin)
{
  dlclose (plugin->handle);
  free (plugin);
}

bitu_plugin_t *
bitu_plugin_load (const char *lib)
{
  bitu_plugin_t *plugin;
  if ((plugin = malloc (sizeof (bitu_plugin_t))) == NULL)
    return NULL;

  plugin->handle = dlopen (lib, RTLD_LAZY);
  if (!plugin->handle)
    {
      fprintf (stderr, "%s\n", dlerror ());
      return NULL;
    }

  dlerror ();

  plugin->name = dlsym (plugin->handle, "plugin_name");
  plugin->num_params = dlsym (plugin->handle, "plugin_num_params");
  plugin->message_return = dlsym (plugin->handle, "plugin_message_return");

  return plugin;
}

bitu_plugin_ctx_t *
bitu_plugin_ctx_new (void)
{
  bitu_plugin_ctx_t *plugin_ctx;
  if ((plugin_ctx = malloc (sizeof (plugin_ctx))) == NULL)
    {
      return NULL;
    }

  plugin_ctx->plugins = hashtable_create (hash_string, string_equal, free,
                                          (void *) bitu_plugin_free);
  if (plugin_ctx->plugins == NULL)
    {
      free (plugin_ctx);
      return NULL;
    }
  return plugin_ctx;
}

void
bitu_plugin_ctx_free (bitu_plugin_ctx_t *plugin_ctx)
{
  hashtable_destroy (plugin_ctx->plugins);
  free (plugin_ctx);
}

int
bitu_plugin_ctx_load (bitu_plugin_ctx_t *plugin_ctx, const char *lib)
{
  bitu_plugin_t *plugin;
  plugin = bitu_plugin_load (lib);
  if (plugin == NULL)
    return 0;
  if (hashtable_set (plugin_ctx->plugins,
                     strdup (bitu_plugin_name (plugin)),
                     plugin))
    {
      bitu_plugin_free (plugin);
      return 0;
    }
  return 1;
}

bitu_plugin_t *
bitu_plugin_ctx_find (bitu_plugin_ctx_t *plugin_ctx, const char *name)
{
  return hashtable_get (plugin_ctx->plugins, name);
}

int
bitu_plugin_ctx_load_from_file (bitu_plugin_ctx_t *plugin_ctx,
                                const char *fname)
{
  FILE *fd;
  char line[LINELEN_MAX];

  if ((fd = fopen (fname, "r")) == NULL)
    return 0;
  while (fgets (line, LINELEN_MAX, fd))
    {
      const char *lib;
      if (line[0] == '#')
        continue;
      lib = (const char *) bitu_util_strstrip (line);
      bitu_plugin_ctx_load (plugin_ctx, lib);
    }
  fclose (fd);
  return 1;
}
