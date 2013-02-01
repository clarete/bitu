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
#include <pthread.h>
#include <uuid/uuid.h>
#include <taningia/error.h>
#include <bitu/util.h>


char *
bitu_util_strstrip (const char *string)
{
  char *s = (char *) string;
  int len = strlen (s);

  while (isspace (s[len - 1]))
    --len;
  while (*s && isspace (*s))
    ++s, --len;
  return strndup (s, len);
}

int
bitu_util_extract_params (const char *line, char **cmd,
                          char ***params, int *len)
{
  size_t i;
  char *s;
  int bufsize = 128;
  int inside_quotes = 0;

  char *ecmd = NULL;
  char **tmp, **eparams = NULL;
  int counter = 0;

  size_t full_len;
  int iter = 0, tok_size = 0;
  int allocated = 0;
  char *tok = NULL;

  s = bitu_util_strstrip (line);

  full_len = strlen (s);
  if (full_len == 0)
    {
      free (s);
      return TA_ERROR;
    }
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
                  return TA_ERROR;
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
          if (tok_size == 0 || tok_size > allocated)
            {
              char *tmp;
              while (allocated < tok_size)
                allocated += bufsize;
              if ((tmp = realloc (tok, allocated)) == NULL)
                {
                  free (tok);
                  return TA_ERROR;
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
  if (cmd)
    *cmd = ecmd;
  if (params)
    *params = eparams;
  if (len)
    *len = counter;

  free (s);
  return TA_OK;
}

void
bitu_util_start_new_thread (bitu_util_callback_t callback, void *data)
{
  pthread_t thread;
  pthread_create (&thread, NULL, callback, data);
  pthread_detach (thread);
}

char *
bitu_util_uuid4 (void)
{
  char *buf;
  uuid_t uuid;

  buf = (char *) malloc (36);
  uuid_generate (uuid);
  uuid_unparse (uuid, buf);
  return buf;
}
