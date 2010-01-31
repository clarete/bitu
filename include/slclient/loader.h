/* util.h - This file is part of the diagnosis program
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

#ifndef SLC_LOADER_H_
#define SLC_LOADER_H_ 1

typedef struct slc_plugin slc_plugin_t;
typedef struct slc_plugin_ctx slc_plugin_ctx_t;

slc_plugin_t *slc_plugin_load (const char *lib);
void slc_plugin_free (slc_plugin_t *plugin);
const char *slc_plugin_name (slc_plugin_t *plugin);
int slc_plugin_num_params (slc_plugin_t *plugin);
char *slc_plugin_message_return (slc_plugin_t *plugin);

slc_plugin_ctx_t *slc_plugin_ctx_new (void);
void slc_plugin_ctx_free (slc_plugin_ctx_t *plugin_ctx);
int slc_plugin_ctx_load (slc_plugin_ctx_t *plugin_ctx, const char *lib);
slc_plugin_t *slc_plugin_ctx_find (slc_plugin_ctx_t *plugin_ctx,
                                   const char *name);

#endif /* SLC_LOADER_H_ */
