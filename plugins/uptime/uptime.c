/* uptime.c - This file is part of the slclient program
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
#include <unistd.h>
#include <string.h>

const char *
plugin_name (void)
{
  return "uptime";
}

int
plugin_num_params (void)
{
  return 0;
}

char *
plugin_message_return (void)
{
  char message[1024];
  FILE *fd;
  if ((fd = popen ("/usr/bin/uptime", "r")) == NULL)
    return NULL;
  fread (&message, 1, 1024, fd);
  pclose (fd);
  return strdup (message);
}
