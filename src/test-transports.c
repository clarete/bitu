/* test-transports.c - This file is part of the bitu program
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
#include <bitu/transport.h>

int
main (int argc, char **argv)
{
  bitu_transport_t *transport;

  if (argc < 2)
    {
      fprintf (stderr, "Usage: %s <transport-uri>\n", argv[0]);
      return EXIT_FAILURE;
    }
  if ((transport = bitu_transport_new (argv[1])) == NULL)
    {
      fprintf (stderr, "Your transport suck, bro\n");
      return EXIT_FAILURE;
    }
  switch (bitu_transport_connect (transport))
    {
    case BITU_CONN_STATUS_OK:
      printf ("connect: Ok bro, we're in!\n");
      break;
    case BITU_CONN_STATUS_CONNECTION_FAILED:
      printf ("connect: Connection failed\n");
      break;
    case BITU_CONN_STATUS_ALREADY_RUNNING:
      printf ("connect: transport already running. Seems wrong, I know.\n");
      break;
    default:
      printf ("connect: I don't know what is the problem");
      break;
    }
  bitu_transport_run (transport);
  return EXIT_SUCCESS;
}
