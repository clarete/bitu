/* test-conf.c - This file is part of the bitu program
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
#include <bitu/conf.h>
#include <bitu/transport.h>

int
main ()
{
  ta_list_t *conf, *tmp;
  if ((conf = bitu_conf_read_from_file ("bitu.conf.dist")) == NULL)
    {
      printf ("Error!\n");
      exit (EXIT_FAILURE);
    }
  for (tmp = conf; tmp; tmp = tmp->next)
    {
      int i = 0;
      bitu_command_t *command;
      const char *param;
      const char **params;
      const char *cmd;

      command = tmp->data;
      cmd = bitu_command_get_name (command);
      params = bitu_command_get_params (command);
      printf ("Cmd: %s\n", cmd);
      while ((param = params[i++]) != NULL)
        printf (" * %s\n", param);
    }
  return 0;
}
