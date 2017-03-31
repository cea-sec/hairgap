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

#include "sender.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "limiter.h"
#include "proto.h"

struct hgap_sender {
    struct sockaddr_in dstaddr;
    int socket;

    uint32_t keepalive;

    struct hgap_limiter *hlim;
    pthread_t keepalive_thread;
    int cont;
};

static void *
keepalive_loop_thread(void *args)
{
    struct hgap_sender *hs = args;
    uint32_t keepalive = hs->keepalive;
    int *cont = &hs->cont;
    struct hgap_header ka_hdr;
    char ka_pkt[HGAP_HEADER_LEN];
    
    // Generate keepalive packet (header only)
    hgap_header_keepalive(&ka_hdr);
    hgap_write_header(&ka_hdr, ka_pkt);

    // Stops when the shared cont variable is set to 0
    while (*cont) {
        // Wait the appropriate time
        if (keepalive >= 1000) {
            sleep(keepalive / 1000);
        } else {
            usleep(keepalive * 1000);
        }

        // FIXME: CHK_PERROR should disappear
        // Send the keepalive
        CHK_PERROR(hgap_sender_send(hs, ka_pkt, HGAP_HEADER_LEN) >= 0);
    }

    return NULL;
}

struct hgap_sender *
hgap_sender_new(char *host, short port, uint64_t byterate, uint32_t keepalive)
{
    struct hgap_sender *hs = xmalloc(sizeof *hs);
    hs->hlim = hgap_limiter_new(byterate);

    // Open socket
    hs->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (hs->socket < 0) {
        DBG("Socket creation error");
        goto err_sock;
    }

    // Create destination address
    memset(&hs->dstaddr, 0, sizeof(hs->dstaddr));
    hs->dstaddr.sin_family = AF_INET;
    // TODO getaddrinfo
    if ((hs->dstaddr.sin_addr.s_addr = inet_addr(host)) == 0) {
        DBG("Invalid IP in hgap_sender\n");
        goto err;
    }

    hs->dstaddr.sin_port = htons(port);
    hs->keepalive = keepalive;
    hs->cont = 0;

    // Start keepalive
    if (hs->keepalive) {
        // FIXME: ensure no weird race condition can be cause by sharing this
        // non-locked flag with the keepalive_thread
        hs->cont = 1;
        int ret = pthread_create(&hs->keepalive_thread, NULL,
                                 keepalive_loop_thread, hs);
        if (ret != 0) {
            DBG("Keepalive thread creation error\n");
            goto err;
        }
    }

    return hs;

    // Error handling
err:
    close(hs->socket);
err_sock:
    free(hs);
    return NULL;
}

ssize_t
hgap_sender_send(struct hgap_sender *hs, void *pkt, size_t size)
{
    ssize_t ret = sendto(hs->socket, pkt, size, 0,
                         (struct sockaddr *)&hs->dstaddr, sizeof(hs->dstaddr));

    if (ret >= 0) {
        hgap_limiter_limit(hs->hlim, ret);
    }

    return ret;
}

ssize_t
hgap_sender_control(struct hgap_sender *hs, void *pkt, size_t size)
{
    int i = 0;
    ssize_t ret = 0;

    for (i = 0; i < HGAP_SALVE_LEN; i++) {
        ret = hgap_sender_send(hs, pkt, size);

        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

void
hgap_sender_free(struct hgap_sender *hs)
{
    if (hs->cont) {
        hs->cont = 0;
        pthread_join(hs->keepalive_thread, NULL);
    }
    hgap_limiter_free(hs->hlim);
    free(hs);
}
