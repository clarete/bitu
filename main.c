/* main.c - This file is part of the diagnosis program
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
#include <dlfcn.h>
#include <string.h>
#include <ctype.h>

#define LINELEN_MAX     255

/* -- utils.c */

char *
util_strstrip (char *string)
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

  for (s = string; isspace (*s); *s++);
  return s;
}

/* -- processor.h -- */

typedef struct {
  int number;
  char *vendor_id;
  char *model;
  float clock;                  /* MHz */
  int cache_size;               /* KB */
} processor_t;

/* -- processor.c -- */

processor_t *
processor_new (int number,
               const char *vendor_id,
               const char *model,
               int clock,
               int cache_size)
{
  processor_t *ret;
  ret = malloc (sizeof (processor_t));
  ret->number = number;
  ret->vendor_id = strdup (vendor_id);
  ret->model = strdup (model);
  ret->clock = clock;
  ret->cache_size = cache_size;
  return ret;
}

void
processor_free (processor_t *processor)
{
  free (processor->vendor_id);
  free (processor->model);
  free (processor);
}

/* Reads information present in /proc/cpuinfo. */
processor_t **
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
          char *skey = util_strstrip (key);
          if (strcmp (skey, "processor") == 0)
            {
              processor = malloc (sizeof (processor_t));
              processor->number = atoi (util_strstrip (val));
              processors = realloc (processors,
                                    sizeof (processor_t) * (counter+1));
              processors[counter] = processor;
              counter++;
            }
          else if (strcmp (skey, "vendor_id") == 0)
            processor->vendor_id = strdup (util_strstrip (val));
          else if (strcmp (skey, "model name") == 0)
            processor->model = strdup (util_strstrip (val));
          else if (strcmp (skey, "cpu MHz") == 0)
            processor->clock = atof (util_strstrip (val));
          else if (strcmp (skey, "cache size") == 0)
            processor->cache_size = atoi (util_strstrip (val));
        }
    }

  fclose (fd);

  if (num)
    *num = counter;
  return processors;
}

void
processor_free_cpuinfo (processor_t **processors, int num)
{
  int i;
  for (i = 0; i < num; i++)
    processor_free (processors[i]);
  free (processors);
}

int
main (int argc, char **argv)
{
  int num;
  int i;
  processor_t **processors = processor_read_cpuinfo (&num);

  for (i = 0; i < num; i++)
    {
      printf ("Processor %d\n", processors[i]->number);
      printf ("VendorID: %s\n", processors[i]->vendor_id);
      printf ("Model: %s\n", processors[i]->model);
      printf ("Cpu MHz: %f\n", processors[i]->clock);
      printf ("Cache size: %d\n", processors[i]->cache_size);
      printf ("\n");
    }
  processor_free_cpuinfo (processors, num);

  return 0;
}
