/* util.c - This file is part of the bitu program
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <bitu/util.h>

char *
bitu_util_strstrip (char *string)
{
  char *s;
  int len = strlen (string);

  while (len--)
    {
      if (isspace (string[len]))
        string[len] = '\0';
      else
        break;
    }

  for (s = string; isspace (*s); s++);
  return s;
}

int
bitu_util_extract_params (const char *line, char **cmd,
                          char ***params, int *len)
{
  char *line_tmp, *body, *ecmd;
  char **eparams = NULL;
  int counter = 0;

  line_tmp = strdup (line);
  body = bitu_util_strstrip (line_tmp);
  ecmd = strtok (body, " ");
  if (ecmd == NULL)
    {
      free (body);
      return 0;
    }
  do
    {
      char **tmp;
      char *param = strtok (NULL, " ");
      size_t elen = (sizeof (char *) * (counter+1));
      if ((tmp = realloc (eparams, elen)) == NULL)
        {
          free (body);
          break;
        }
      else
        eparams = tmp;
      if (param == NULL)
        {
          eparams[counter] = NULL;
          break;
        }
      eparams[counter] = strdup (param);
      counter++;
    }
  while (1);

  /* Filling all out params */
  *cmd = strdup (ecmd);
  *params = eparams;
  *len = counter;

  /* Time to free the duplicated string */
  free (line_tmp);
  return 1;
}
