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

iks *
get_node (void)
{
  int num_procs, i;
  iks *root, *proc;
  char snum[256], sclock[256], scache[256];
  processor_t **processors;

  root = iks_new ("processors");
  processors = processor_read_cpuinfo (&num_procs);
  for (i = 0; i < num_procs; i++)
    {
      /* Convert non strings in strings to build xml nodes */
      sprintf (snum, "%d", processors[i]->number);
      sprintf (sclock, "%f", processors[i]->clock);
      sprintf (scache, "%d", processors[i]->cache_size);

      /* Building a processor node */
      proc = iks_new ("processor");
      iks_insert_attrib (proc, "number", snum);
      iks_insert_attrib (proc, "vendor_id", processors[i]->vendor_id);
      iks_insert_attrib (proc, "model", processors[i]->model);
      iks_insert_attrib (proc, "clock", sclock);
      iks_insert_attrib (proc, "cache_size", scache);

      /* Finally associating the processor node to the root one */
      iks_insert_node (root, proc);
    }

  processor_free_cpuinfo (processors, num_procs);
  return root;
}
