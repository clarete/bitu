/* conf.c - This file is part of the bitu program
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
#include <bitu/util.h>
#include <bitu/conf.h>

#define LINE_LEN_MAX 255

ta_list_t *
bitu_conf_read_from_file (const char *file_path)
{
  FILE *fd;
  char line[LINE_LEN_MAX];
  ta_list_t *config = NULL;
  if ((fd = fopen (file_path, "r")) == NULL)
    return NULL;
  while (fgets (line, LINE_LEN_MAX, fd))
    {
      int nparams;
      char *cmd;
      char **params;
      bitu_conf_entry_t *entry;
      if (line[0] == '#' || line[0] == '\n')
        continue;
      if (!bitu_util_extract_params (line, &cmd, &params, &nparams))
        continue;
      if ((entry = malloc (sizeof (bitu_conf_entry_t))) == NULL)
        {
          int i;
          for (i = 0; i < nparams; i++)
            free (params[i]);
          free (params);
          continue;
        }
      entry->cmd = cmd;
      entry->params = params;
      entry->nparams = nparams;
      config = ta_list_append (config, entry);
    }
  fclose (fd);

  return config;
}

void
bitu_conf_list_free (ta_list_t *conf)
{
  ta_list_t *node ;
  bitu_conf_entry_t *entry;
  int i;
  for (node = conf; node; node = node->next)
    {
      entry = (bitu_conf_entry_t *) node->data;
      for (i = 0; i < entry->nparams; i++)
        free (entry->params[i]);
      free (entry->params);
      free (entry->cmd);
      free (entry);
      free (node);
    }
}
