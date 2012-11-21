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
#include <bitu/transport.h>

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
      bitu_command_t *command;

      /* Getting rid of the '\n' */
      line[strlen (line) - 1] = '\0';

      /* Skipping invalid commands */
      if (line[0] == '#' || line[0] == '\0')
        continue;
      if ((command = bitu_command_new (NULL, line, NULL)) == NULL)
        continue;
      config = ta_list_append (config, command);
    }
  fclose (fd);

  return config;
}
