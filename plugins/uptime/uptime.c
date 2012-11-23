/* uptime.c - This file is part of the bitu program
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
#include <bitu/transport.h>

const char *
plugin_name (void)
{
  return "uptime";
}

char *
plugin_execute (bitu_command_t *command)
{
  int bufsize = 1024;
  char message[bufsize];
  FILE *fd;
  int written;
  if ((fd = popen ("/usr/bin/uptime", "r")) == NULL)
    return NULL;
  written = fread (&message, 1, bufsize, fd);
  message[written] = '\0';
  pclose (fd);
  return strdup (message);
}
