/* util.h - This file is part of the bitu program
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

#ifndef BITU_UTIL_H_
#define BITU_UTIL_H_ 1

typedef void *(*bitu_util_callback_t) (void *);

char *bitu_util_strstrip (const char *string);
int bitu_util_extract_params (const char *line, char **cmd,
                              char ***params, int *len);
void bitu_util_start_new_thread (bitu_util_callback_t callback, void *data);
char *bitu_util_uuid4 (void);

#endif /* BITU_UTIL_H_ */
