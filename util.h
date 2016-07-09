/*
 * sfp-driver - SFP driver
 *
 * Copyright (C) 2016 Jernej Kos <jernej@kos.mx>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef SFP_DRIVER_UTIL_H
#define SFP_DRIVER_UTIL_H

#include <string.h>
#include <ctype.h>

static inline void trim(char *string) {
  char *p = string;
  size_t length = strlen(p);
  if (!length) {
    return;
  }

  while (isspace(p[length - 1])) p[--length] = 0;
  while (*p && isspace(*p)) ++p, --length;

  memmove(string, p, length + 1);
}

#endif
