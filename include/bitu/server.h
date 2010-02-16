/* server.h - This file is part of the bitu program
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

#ifndef BITU_SERVER_H_
#define BITU_SERVER_H_ 1

#include <sys/types.h>
#include <bitu/app.h>

typedef struct _bitu_server bitu_server_t;

bitu_server_t *bitu_server_new (const char *sock_path, bitu_app_t *app);
void bitu_server_free (bitu_server_t *server);
int bitu_server_connect (bitu_server_t *server);
void bitu_server_run (bitu_server_t *server);

#endif /* BITU_SERVER_H_ */
