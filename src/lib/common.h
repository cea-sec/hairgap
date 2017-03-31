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

#ifndef HGAP_COMMON_H
#define HGAP_COMMON_H

#include <stdio.h>

#include "hairgap.h"

//#define DEBUG

// To avoid unused compiler warning
#define FAKE_USE(x) ((void) (x))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#ifdef DEBUG
#define DBG(...) fprintf(stderr, "[DEBUG] - " __VA_ARGS__)
#else
#define DBG(...)
#endif

#define INFO(...) fprintf(stderr, "[INFO]  - " __VA_ARGS__)
#define WARN(...) fprintf(stderr, "[WARN]  - " __VA_ARGS__)
#define PWARN(s) perror("[WARN]  - " s)

#define ERROR(...) do {\
    fprintf(stderr, "[ERROR] (at " __FILE__ ":%d) - ", __LINE__);\
    fprintf(stderr, __VA_ARGS__);\
} while (0)

#define HGAP_PERROR(err, ...) do {\
    fprintf(stderr, "[ERROR] (at " __FILE__ ":%d) - ", __LINE__);\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, ": ");\
    fprintf(stderr, hgap_err_str(err));\
    fprintf(stderr, "\n");\
} while (0)

#define CHK(x) do {\
    if (!(x)) {\
        ERROR("%s", #x);\
        exit(EXIT_FAILURE);\
    }\
} while (0)
#define CHK_MSG(x, ...) do {\
    if (!(x)) {\
        ERROR(__VA_ARGS__);\
        exit(EXIT_FAILURE);\
    }\
} while (0)
#define CHK_PERROR(x) do if (!(x)) { perror(#x); exit(EXIT_FAILURE); } while (0)

/**
 * Helper struct to handle sized bufs
 */
struct sized_buf {
    size_t size;
    void *data;
    /* Optional */
    char content[0];
};

#define SBUF_NULL { .data=NULL, .size=0 }
#define SBUF_RESET(x) do { (x).data = NULL; (x).size = 0; } while (0)

/**
 * Select the best error to report among multiple ones
 */
#define HGAP_SELECT_ERROR(e1, e2) \
    ((e1) == HGAP_SUCCESS ? \
        (e2) : ((e2) == HGAP_SUCCESS ? \
            (e1) : MIN((e1), (e2))))

/**
 * Malloc that exits on failure.
 */
void *xmalloc(size_t size);

void dbg_hexdump(void *, size_t);

#endif // HGAP_COMMON_H
