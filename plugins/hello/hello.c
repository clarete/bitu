/* hello.c - This file is part of the bitu program
 *
 * Copyright (C) 2012  Lincoln de Sousa <lincoln@comum.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>

#include <bitu/transport.h>
#include "hello.h"

#define PATTERN "Say hello to ([[:alnum:]]+)"

const char *
plugin_name (void)
{
  return "hello";
}

int
plugin_match (const char *cmd)
{
  regex_t re;
  if (regcomp (&re, PATTERN, REG_ICASE | REG_EXTENDED) != 0)
    return 0;
  if (regexec (&re, cmd, 0, NULL, REG_NOSUB) != 0)
    return 0;
  return 1;
}

char *
plugin_execute (bitu_command_t *command)
{
  regex_t re;
  regmatch_t *groups;
  size_t ngroups;
  size_t i;
  size_t cmd_len;
  char *name, *msg;
  const char *cmd = bitu_command_get_cmd (command);

  cmd_len = strlen (cmd);

  if (regcomp (&re, PATTERN, REG_ICASE | REG_EXTENDED) != 0)
    return NULL;

  ngroups = re.re_nsub + 1;
  groups = malloc (ngroups * sizeof (regmatch_t));
  if (regexec (&re, cmd, ngroups, groups, 0) != 0)
    {
      free (groups);
      return NULL;
    }

  /* Skiping the whole string by starting from 1 */
  for (i = 1; i < ngroups; i++)
    {
      char found[cmd_len];

      if (groups[i].rm_so == -1)
        break;

      strcpy (found, cmd);
      found[groups[i].rm_eo] = 0;
      name = found + groups[i].rm_so;
      msg = malloc (cmd_len * sizeof (char));
      snprintf (msg, cmd_len, "Hello %s!", name);
    }

  return msg;
}
