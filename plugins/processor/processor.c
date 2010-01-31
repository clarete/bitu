/* processor.c - This file is part of the diagnosis program
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
#include <slclient/util.h>
#include <iksemel.h>

#include "processor.h"

#define LINELEN_MAX     255

static void
processor_free (processor_t *processor)
{
  free (processor->vendor_id);
  free (processor->model);
  free (processor);
}

/* Reads information present in /proc/cpuinfo. */
static processor_t **
processor_read_cpuinfo (int *num)
{
  FILE *fd;
  char line[LINELEN_MAX];
  processor_t **processors;
  int counter = 0;

  fd = fopen ("/proc/cpuinfo", "r");
  if (fd == NULL)
    return NULL;

  processors = malloc (sizeof (processor_t));

  while (fgets (line, LINELEN_MAX, fd))
    {
      processor_t *processor;
      char *key, *val;
      key = strtok (line, ":");
      val = strtok (NULL, "\n");

      if (key != NULL)
        {
          char *skey = slc_util_strstrip (key);
          if (strcmp (skey, "processor") == 0)
            {
              processor = malloc (sizeof (processor_t));
              processor->number = atoi (slc_util_strstrip (val));
              processors = realloc (processors,
                                    sizeof (processor_t) * (counter+1));
              processors[counter] = processor;
              counter++;
            }
          else if (strcmp (skey, "vendor_id") == 0)
            processor->vendor_id = strdup (slc_util_strstrip (val));
          else if (strcmp (skey, "model name") == 0)
            processor->model = strdup (slc_util_strstrip (val));
          else if (strcmp (skey, "cpu MHz") == 0)
            processor->clock = atof (slc_util_strstrip (val));
          else if (strcmp (skey, "cache size") == 0)
            processor->cache_size = atoi (slc_util_strstrip (val));
        }
    }

  fclose (fd);

  if (num)
    *num = counter;
  return processors;
}

static void
processor_free_cpuinfo (processor_t **processors, int num)
{
  int i;
  for (i = 0; i < num; i++)
    processor_free (processors[i]);
  free (processors);
}


/* This is the plugin public interface. All other stuff don't need to
 * be public */

const char *
plugin_name (void)
{
  return "processor-info";
}

int
plugin_num_params (void)
{
  return 0;
}

char *
plugin_message_return (void)
{
  char *ret = NULL;
  int num_procs, i;
  processor_t **processors;
  int total_size = 0;

  processors = processor_read_cpuinfo (&num_procs);
  for (i = 0; i < num_procs; i++)
    {
      char *np, *proc;
      int n, size = 100;

      if ((proc = malloc (size)) == NULL)
        {
          processor_free_cpuinfo (processors, num_procs);
          return NULL;
        }

      /* This while builds a single processor string. The `proc' var
       * will be concatenated to the `ret' string after that. This is
       * done to make sure that enought space will be alocated to each
       * processor info.*/
      while (1)
        {
          n = snprintf (proc, size,
                        "Processor %d\n"
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
              processor_free_cpuinfo (processors, num_procs);
              free (proc);
              return NULL;
            }
          else
            proc = np;
        }

      /* Time to concat the current processor to the main info
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
              processor_free_cpuinfo (processors, num_procs);
              return NULL;
            }
          else
            ret = nret;
          strncat (ret, proc, total_size);
        }
      free (proc);
    }
  processor_free_cpuinfo (processors, num_procs);
  return ret;
}
