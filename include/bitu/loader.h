/* util.h - This file is part of the bitu program
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

#ifndef BITU_LOADER_H_
#define BITU_LOADER_H_ 1

typedef struct bitu_plugin bitu_plugin_t;
typedef struct bitu_plugin_ctx bitu_plugin_ctx_t;

bitu_plugin_t *bitu_plugin_load (const char *lib);
void bitu_plugin_free (bitu_plugin_t *plugin);
const char *bitu_plugin_name (bitu_plugin_t *plugin);
int bitu_plugin_num_params (bitu_plugin_t *plugin);
char *bitu_plugin_message_return (bitu_plugin_t *plugin);

bitu_plugin_ctx_t *bitu_plugin_ctx_new (void);
void bitu_plugin_ctx_free (bitu_plugin_ctx_t *plugin_ctx);
int bitu_plugin_ctx_load (bitu_plugin_ctx_t *plugin_ctx, const char *lib);
bitu_plugin_t *bitu_plugin_ctx_find (bitu_plugin_ctx_t *plugin_ctx,
                                     const char *name);
int bitu_plugin_ctx_load_from_file (bitu_plugin_ctx_t *plugin_ctx,
                                    const char *fname);

#endif /* BITU_LOADER_H_ */
