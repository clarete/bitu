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
#include <stdio.h>
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
  size_t i;
  char *s;
  int bufsize = 64;
  int inside_quotes = 0;

  char *line_tmp, *ecmd = NULL;
  char **tmp, **eparams = NULL;
  int counter = 0;

  size_t full_len;
  int iter = 0, tok_size = 0;
  char *tok = NULL;

  line_tmp = strdup (line);
  s = bitu_util_strstrip (line_tmp);

  full_len = strlen (s);
  for (i = 0; i <= full_len; i++)
    {
      if (s[i] == '\"' && s[i-1] != '\\')
        {
          /* starting/ending quotes */
          inside_quotes = !inside_quotes;
        }

      if (((!inside_quotes && s[i] == ' ' && s[i-1] != '\\') || i == full_len)
          && tok != NULL)
        {
          size_t elen = (sizeof (char *) * (counter+1));

          /* Preparing tok to be stored in the param array. */
          tok[tok_size] = '\0';

          /* Seems to be the first token, so it is the command. The
           * rest is going to be the parameter list */
          if (ecmd == NULL)
            ecmd = strdup (tok);
          else
            {
              if ((tmp = realloc (eparams, elen)) == NULL)
                {
                  /* TODO: free eparams and its items */
                  return 0;
                }
              else
                eparams = tmp;
              eparams[counter] = strdup (tok);

              /* Setting number of params */
              counter++;
            }

          /* Freeing already collected token. */
          free (tok);
          tok = NULL;

          /* Resseting counters. */
          tok_size = 0;
          iter = 0;
        }
      else
        {
          if (tok_size == 0 || iter > tok_size)
            {
              char *tmp;
              if ((tmp = realloc (tok, tok_size + bufsize)) == NULL)
                {
                  free (tok);
                  return 0;
                }
              else
                tok = tmp;
            }

          /* Making sure that escape char will not be catched */
          if ((s[i] == '\\' && s[i-1] != '\\') ||
              (s[i] == '\"' && s[i-1] != '\\'))
            continue;

          tok[iter++] = s[i];
          tok_size++;
        }
    }

  /* Filling all out params */
  *cmd = ecmd;
  *params = eparams;
  *len = counter;

  free (line_tmp);
  return 1;
}
