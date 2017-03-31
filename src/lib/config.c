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


#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "common.h"
#include "hairgap.h"
#include "proto.h"

int
hgap_defaults(struct hgap_config *config)
{
    if (config == NULL) {
        return HGAP_ERR_NO_CONFIG;
    }

    // Zero fields by default
    memset(config, 0, sizeof *config);

    config->in = HGAP_DEF_IN_FILE;
    config->out = HGAP_DEF_OUT_FILE;
    config->n_pkt = HGAP_DEF_N_PKT;
    config->pkt_size = HGAP_DEF_PKT_SIZE;
    config->redund = HGAP_DEF_REDUND;
    config->addr = HGAP_DEF_ADDR;
    config->port = HGAP_DEF_PORT;
    config->byterate = HGAP_DEF_BYTERATE;
    config->keepalive = HGAP_DEF_KEEPALIVE;
    config->timeout = HGAP_DEF_TIMEOUT;
    config->mem_limit = HGAP_DEF_MEM_LIMIT;

    return HGAP_SUCCESS;
}

void
hgap_config_dump(const struct hgap_config *config, FILE *out)
{
    char *addr = config->addr != NULL ? config->addr : "<not set>";

    fprintf(out,
            "Hairgap config:\n"
            "    in: %p\n"
            "    out: %p\n"
            "    n_pkt: %"PRIu32"\n"
            "    pkt_size: %zu\n"
            "    redundancy: %lf\n"
            "    receiver addr: %s\n"
            "    receiver port: %hd\n"
            "    byterate: %lf\n"
            "    keepalive: %"PRIu64" ms\n"
            "    timeout: %"PRIu64" us\n"
            "    memory limit: %.3f MB\n",
            config->in,
            config->out,
            config->n_pkt,
            config->pkt_size,
            config->redund,
            addr,
            config->port,
            config->byterate,
            config->keepalive,
            config->timeout,
            config->mem_limit / (1024*1024.));
}

static int
check_addr(const char *addr)
{
    struct addrinfo *res;
    if (addr == NULL) {
        WARN("NULL address\n");
        return HGAP_ERR_INVALID_ADDR;
    }

    int addr_valid = getaddrinfo(addr, NULL, NULL, &res);
    if (addr_valid != 0) {
        WARN("Invalid network address: %s\n", gai_strerror(addr_valid));
        return HGAP_ERR_INVALID_ADDR;
    } else {
        freeaddrinfo(res);
    }

    return HGAP_SUCCESS;
}

// FIXME
static int
check_file(FILE *file)
{
    if (file == NULL || ferror(file) != 0) {
        return HGAP_ERR_BAD_FD;
    }

    return HGAP_SUCCESS;
}

int
hgap_check_config_sender(const struct hgap_config *config)
{
    if (config->pkt_size <= HGAP_HEADER_LEN) {
        WARN("MTU too small: %zu\n", config->pkt_size);
        return HGAP_ERR_MTU_TOO_SMALL;
    }

    if (config->pkt_size > HGAP_MAX_PKT_SIZE) {
        WARN("MTU too big: %zu\n", config->pkt_size);
        return HGAP_ERR_MTU_TOO_BIG;
    }

    int ret = check_addr(config->addr);
    if (ret != HGAP_SUCCESS) {
        return ret;
    }

    if (check_file(config->in) != HGAP_SUCCESS) {
        PWARN("Invalid input file");
        return HGAP_ERR_BAD_IN_FD;
    }

    if (config->n_pkt < 1 || config->n_pkt > HGAP_MAX_N_PKT) {
        WARN("Number of pkt must be between 1 and %d\n", HGAP_MAX_N_PKT);
        return HGAP_ERR_BAD_N_PKT;
    }

    if (config->redund < 1.0) {
        WARN("Redundancy must be >= 1\n");
        return HGAP_ERR_BAD_REDUND;
    }

    return HGAP_SUCCESS;
}

int
hgap_check_config_receiver(const struct hgap_config *config)
{
    if (check_file(config->out) != HGAP_SUCCESS) {
        PWARN("Invalid output file");
        return HGAP_ERR_BAD_OUT_FD;
    }

    return check_addr(config->addr);
}
