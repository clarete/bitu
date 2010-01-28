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

int
main (int argc, char **argv)
{
  /*
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
  */

  return 0;
}
