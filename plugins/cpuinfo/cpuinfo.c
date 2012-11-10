/* cpuinfo.c - This file is part of the bitu program
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
#include <bitu/util.h>
#include <iksemel.h>

#include "cpuinfo.h"

#define LINELEN_MAX     255

static void
cpuinfo_free (cpuinfo_t *cpuinfo)
{
  free (cpuinfo->vendor_id);
  free (cpuinfo->model);
  free (cpuinfo);
}

/* Reads information present in /proc/cpuinfo. */
static cpuinfo_t **
cpuinfo_read_cpuinfo (int *num)
{
  FILE *fd;
  char line[LINELEN_MAX];
  cpuinfo_t **cpuinfos;
  int counter = 0;

  if ((fd = fopen ("/proc/cpuinfo", "r")) == NULL)
    return NULL;

  cpuinfos = malloc (sizeof (cpuinfo_t));

  while (fgets (line, LINELEN_MAX, fd))
    {
      cpuinfo_t *cpuinfo;
      char *key, *val;
      key = strtok (line, ":");
      val = strtok (NULL, "\n");

      if (key != NULL)
        {
          char *skey = bitu_util_strstrip (key);
          if (strcmp (skey, "processor") == 0)
            {
              cpuinfo = malloc (sizeof (cpuinfo_t));
              cpuinfo->number = atoi (bitu_util_strstrip (val));
              cpuinfos = realloc (cpuinfos,
                                    sizeof (cpuinfo_t) * (counter+1));
              cpuinfos[counter] = cpuinfo;
              counter++;
            }
          else if (strcmp (skey, "vendor_id") == 0)
            cpuinfo->vendor_id = strdup (bitu_util_strstrip (val));
          else if (strcmp (skey, "model name") == 0)
            cpuinfo->model = strdup (bitu_util_strstrip (val));
          else if (strcmp (skey, "cpu MHz") == 0)
            cpuinfo->clock = atof (bitu_util_strstrip (val));
          else if (strcmp (skey, "cache size") == 0)
            cpuinfo->cache_size = atoi (bitu_util_strstrip (val));
        }
    }

  fclose (fd);

  if (num)
    *num = counter;
  return cpuinfos;
}

static void
cpuinfo_free_cpuinfo (cpuinfo_t **cpuinfos, int num)
{
  int i;
  for (i = 0; i < num; i++)
    cpuinfo_free (cpuinfos[i]);
  free (cpuinfos);
}


/* This is the plugin public interface. All other stuff don't need to
 * be public */

const char *
plugin_name (void)
{
  return "cpuinfo";
}

int
plugin_num_params (void)
{
  return 0;
}

char *
plugin_execute (void)
{
  char *ret = NULL;
  int num_procs, i;
  cpuinfo_t **processors;
  int total_size = 0;

#if __APPLE__
  return strdup ("Hey kid, I'm running on a Mac OS X, there's no /proc/cpuinfo here");
#endif

  if ((processors = cpuinfo_read_cpuinfo (&num_procs)) == NULL)
    return NULL;

  for (i = 0; i < num_procs; i++)
    {
      char *np, *proc;
      int n, size = 100;

      if ((proc = malloc (size)) == NULL)
        {
          cpuinfo_free_cpuinfo (processors, num_procs);
          return NULL;
        }

      /* This while builds a single cpuinfo string. The `proc' var
       * will be concatenated to the `ret' string after that. This is
       * done to make sure that enought space will be alocated to each
       * cpuinfo info.*/
      while (1)
        {
          n = snprintf (proc, size,
                        "Cpuinfo %d\n"
                        "Vendor ID: %s\n"
                        "Model: %s\n"
                        "Clock: %f\n"
                        "Cache size: %d\n\n",
                        processors[i]->number,
                        processors[i]->vendor_id,
                        processors[i]->model,
                        processors[i]->clock,
                        processors[i]->cache_size);

          /* Success when writting, let's continue our job out of this
           * loop. */
          if (n > -1 && n < size)
            {
              total_size += n;
              break;
            }

          /* Not enought space, let's try again with more space. */
          if (n > -1)
            size = n + 1;
          else
            size *= 2;
          if ((np = realloc (proc, size)) == NULL)
            {
              cpuinfo_free_cpuinfo (processors, num_procs);
              free (proc);
              return NULL;
            }
          else
            proc = np;
        }

      /* Time to concat the current cpuinfo to the main info
       * string */
      if (ret == NULL)          /* First one */
        ret = strdup (proc);
      else                      /* Next ones */
        {
          char *nret;
          if ((nret = realloc (ret, total_size+1)) == NULL)
            {
              free (proc);
              free (ret);
              cpuinfo_free_cpuinfo (processors, num_procs);
              return NULL;
            }
          else
            ret = nret;
          strncat (ret, proc, total_size);
        }
      free (proc);
    }
  cpuinfo_free_cpuinfo (processors, num_procs);
  return ret;
}
