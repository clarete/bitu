/* test-util.c - This file is part of the bitu program
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
#include <assert.h>
#include <bitu/util.h>

void
test_strstrip (void)
{
  char *stripped;
  const char *orig = " to be stripped out! ";
  printf ("Stripping message `%s': ", orig);

  stripped = bitu_util_strstrip (orig);
  assert (strcmp (stripped, "to be stripped out!") == 0);
  printf ("`%s'\n", stripped);

  free (stripped);
}

void
test_extract_params (void)
{
  char *line = "cmd param3 param4 \"single param with spaces\" blehxx";
  char *cmd;
  char **params;
  int len, i;

  printf ("line: %s\n", line);
  if (bitu_util_extract_params (line, &cmd, &params, &len))
    {
      printf ("cmd: %s, num params: %d, params: \n", cmd, len);
      for (i = 0; i < len; i++)
        printf (" - %s\n", params[i]);

      /* To free all collected data, you have to free all array
       * elements and then the char **param itself. Don't forget to
       * free the cmd var too */
      for (i = 0; i < len; i++)
        free (params[i]);
      free (params);

      free (cmd);
    }
}

int
main (int argc, char **argv)
{
  test_strstrip ();
  test_extract_params ();
  return 0;
}
