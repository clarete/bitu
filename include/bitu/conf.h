/* conf.h - This file is part of the bitu program
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

#ifndef BITU_CONF_H_
#define BITU_CONF_H_ 1

#include <taningia/taningia.h>

typedef struct
{
  char *cmd;
  char **params;
  int nparams;
} bitu_conf_entry_t;

ta_list_t *bitu_conf_read_from_file (const char *file_path);
void bitu_conf_list_free (ta_list_t *conf);

#endif /* BITU_CONF_H_ */
