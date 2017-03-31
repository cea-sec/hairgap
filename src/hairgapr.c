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


#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "hairgap.h"

#define USAGE\
    "Usage: hairgapr [-h] [-m MEM_LIMIT] [-p PORT] [-t TIMEOUT] bind_ip\n"\
    "\n"\
    "Hairgap receiver, to reliably receive data over a unidirectional "\
    "network.\n"\
    "\n"\
    "Options:\n"\
    "    -h              Prints this help and exits.\n"\
    "    -m MEM_LIMIT    Rough memory limit in megabytes.\n"\
    "    -p PORT         Bind port port.\n"\
    "    -t TIMEOUT      Set timeout in seconds. If no packets are received \n"\
    "                    for <timeout> seconds, the transfer is interrupted.\n"

int
main(int argc, char* argv[])
{
    struct hgap_config config;
    hgap_defaults(&config);

    int c = 0;
    while ((c = getopt(argc, argv, "p:t:m:h")) != -1) {
        switch (c) {
        case 'p':
            // FIXME: atoi
            config.port = atoi(optarg);
            break;
        case 't':
            config.timeout = atoi(optarg);
            break;
        case 'm':
            config.mem_limit = atoll(optarg) * 1024 * 1024;
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

    INFO("starting with:\n    addr: %s    port: %hd\n    timeout: %"PRIu64"\n",
         config.addr, config.port, config.timeout);

    hgap_config_dump(&config, stderr);
    int ret = hgap_receive(&config);

    if (ret != HGAP_SUCCESS) {
        HGAP_PERROR(ret, "Hairgapr failed");
        return ret;
    }

    return ret;
}
