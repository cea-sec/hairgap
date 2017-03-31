/*
 * This file is part of hairgap.
 * Copyright (C) 2017  Florent MONJALET <florent.monjalet@cea.fr>
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


#include "common.h"

#include <stdlib.h>

void *
xmalloc(size_t size)
{
    void *ret = malloc(size);
    CHK_PERROR(ret != NULL);
    return ret;
}

void
dbg_hexdump(void *x, size_t len) {
    uint8_t *buf = x;

    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) {
            fprintf(stderr, "\n");
        } else if (i % 8 == 0) {
            fprintf(stderr, " ");
        }
        fprintf(stderr, "%02x ", buf[i]);
    }
    fprintf(stderr, "\n");
}
