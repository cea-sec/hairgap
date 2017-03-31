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

/*
 * Read block on stdin
 * Write block on network
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "hairgap.h"

#define USAGE\
    "Usage: hairgaps [Options] dest_ip\n"\
    "\n"\
    "Hairgap sender, to reliably send data over a unidirectional network.\n"\
    "\n"\
    "Options:\n"\
    "    -h              Prints this help and exits.\n"\
    "    -p PORT         Destination port.\n"\
    "    -r REDUND       Redundancy ratio (1.2 will send 1.2 times more data\n"\
    "                    than the original).\n"\
    "    -b RATE         Rate limit in MB/s\n"\
    "    -N NUM          Number of UDP packets in an error correction chunk.\n"\
    "                    Default (and ideal) is 1000, increasing it will\n"\
    "                    make the transfer more robust to big loss bursts,\n"\
    "                    but possibly slower. 2 <= NUM <= 64000.\n"\
    "    -M MTU          Size in bytes of the UDP payloads to send.\n"\
    "    -k KEEPALIVE    Keepalive period in ms. Default is 500ms. 0\n"\
    "                    disables keepalives.\n"

    //"    -m MEM_LIMIT    Rough memory limit in megabytes.\n"


int
main(int argc, char *argv[])
{
    struct hgap_config config;
    hgap_defaults(&config);

    int c = 0;
    // TODO: arg control, no atof, etc...
    while ((c = getopt(argc, argv, "p:b:r:N:M:k:h")) != -1) {
        switch (c) {
        case 'p':
            config.port = atoi(optarg);
            break;
        case 'b':
            config.byterate = atof(optarg) * 1024 * 1024;
            break;
        case 'r':
            config.redund = atof(optarg);
            break;
        case 'N':
            config.n_pkt = atoll(optarg);
            break;
        case 'M':
            config.pkt_size = atol(optarg);
            break;
        /*
        case 'm':
            config.mem_limit = atoll(optarg) * 1024 * 1024;
            break;
        */
        case 'k':
            config.keepalive = atoi(optarg);
            break;
        case 'h':
            fputs(USAGE, stdout);
            exit(EXIT_SUCCESS);
        default:
            ERROR("Unknown option %c\n", c);
            ERROR(USAGE);
            exit(EXIT_FAILURE);
        }
    }

    if (argc < 2 || argc < optind) {
        fprintf(stderr, USAGE);
        exit(EXIT_FAILURE);
    }

    config.addr = argv[optind];

    INFO("starting with:\n"
         "    addr: %s\tport: %hd\n"
         "    redundancy: x%.2lf\tratelimit: %.2lf MB/s\n"
         "    N: %d\tMTU: %zu\n"
         "    Memory limit: %zu MB\tkeepalive: %"PRIu64" ms\n",
         config.addr, config.port, config.redund,
         config.byterate/(1024*1024), config.n_pkt, config.pkt_size,
         config.mem_limit/(1024*1024), config.keepalive);

    hgap_config_dump(&config, stderr);
    int ret = hgap_send(&config);

    if (ret != HGAP_SUCCESS) {
        HGAP_PERROR(ret, "Hairgaps failed");
        return ret;
    }

    return ret;
}
