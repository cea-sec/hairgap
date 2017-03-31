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

#include "hairgap.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <wirehair.h>

#include "channel.h"
#include "common.h"
#include "encoding.h"

#define HGAPR_WRITE_SYNC_THRESHOLD (100 * 1024 * 1024)

static int
hgapr_open_udp_socket(char *addr, short port)
{
    struct sockaddr_in servaddr;
    socklen_t socklen = sizeof(servaddr);
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        goto err;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    if ((servaddr.sin_addr.s_addr = inet_addr(addr)) == INADDR_NONE) {
        goto err;
    }

    servaddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *) &servaddr, socklen) == -1) {
        perror("socket");
        goto err;
    }

    if (0) {
err:
        if (sockfd != -1) {
            close(sockfd);
        }
        sockfd = -1;
    }

    return sockfd;
}

static void
hgapr_set_socket_timeout(int sockfd, uint64_t timeout) {
    struct timeval tv;

    tv.tv_sec = timeout / 1000000;
    tv.tv_usec = timeout % 1000000;

    CHK_PERROR(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
                &tv, sizeof(tv)) != -1);
}

static int
hgapr_net_reader(struct channel *chan, char *addr, short port, uint64_t timeout)
{
    struct sized_buf *pkt = NULL;
    int retval = HGAP_SUCCESS;
    size_t mtu = channel_elt_size(chan) - sizeof (struct sized_buf);
    int sockfd;
    int started = 0;

    if ((sockfd = hgapr_open_udp_socket(addr, port)) == -1) {
        retval = HGAP_ERR_NETWORK;
        ERROR("Could not open socket\n");
        goto closing;
    }

    for (;;) {
        pkt = channel_reserve(chan);
        CHK(pkt);
        pkt->data = pkt->content;
        ssize_t pkt_size = recvfrom(sockfd, pkt->data, mtu, 0, NULL, NULL);

        if (pkt_size == -1) {
            if (errno == ETIMEDOUT || errno == EAGAIN) {
                ERROR("End of reception, socket timed out\n");
                retval = HGAP_ERR_TIMEOUT;
            } else {
                perror("recvfrom");
                retval = HGAP_ERR_NETWORK;
            }
            break;
        }

        pkt->size = pkt_size;
        enum hgap_pkt_t pkt_type = hgap_pkt_type(pkt->data, pkt->size);

        if (pkt_type == HGAP_PKT_BEGIN && !started) {
            started = 1;
            hgapr_set_socket_timeout(sockfd, timeout);
        }

        // Always send to next thread that really handles the hairgap protocol
        if (!channel_send_reserved(chan, pkt)) {
            DBG("chan_net2dec send error\n");
            retval = HGAP_ERR_IPC;
            break;
        }

        if (pkt_type == HGAP_PKT_END) {
            break;
        }
    }

closing:
    if (sockfd != -1) {
        close(sockfd);
    }

    // Poison pill
    pkt = channel_reserve(chan);
    if (pkt != NULL) {
        pkt->data = NULL;
        DBG("Net receiver poison pill\n");
        if (!channel_send_reserved(chan, pkt)) {
            DBG("chan_net2dec send poison error\n");
            retval = HGAP_ERR_IPC;
        }
    }

    return retval;
}

struct decloop_arg {
    struct hgap_decoder *dec;
    struct channel *chan_net2dec;
    struct channel *chan_dec2out;
};

static void *
decloop(struct decloop_arg *args)
{
    struct hgap_decoder *dec = args->dec;
    struct channel *chan_net2dec = args->chan_net2dec;
    struct channel *chan_dec2out = args->chan_dec2out;

    // Received from net thread
    struct sized_buf *pkt = NULL;
    // Sent to write thread
    struct sized_buf chunk = SBUF_NULL;
    int retval = HGAP_SUCCESS;

    for (;;) {
        if ((pkt = channel_peek(chan_net2dec)) == NULL) {
            DBG("chan_net2dec receive error\n");
            retval = HGAP_ERR_IPC;
            break;
        }

        // Poison pill
        if (pkt->data == NULL) {
            break;
        }

        ssize_t dec_ret = hgap_decoder_read(dec, pkt->data, pkt->size);
        if (!channel_ack(chan_net2dec, pkt)) {
            DBG("chan_net2dec receive error\n");
            retval = HGAP_ERR_IPC;
            break;
        }

        // More to read
        if (dec_ret == 0) {
            continue;
        } else if (dec_ret == -HGAP_EOT) {
            break;
        } else if (dec_ret < 0) {
            HGAP_PERROR(-dec_ret, "Error when decoding");
            retval = -dec_ret;
            channel_poison(chan_net2dec);
            break;
        }

        // Chunk ready to be emitted
        CHK(dec_ret > 0);
        chunk.size = dec_ret;
        chunk.data = xmalloc(chunk.size);

        int emit_ret = hgap_decoder_emit(dec, chunk.data, chunk.size);

        if (emit_ret == HGAP_SUCCESS) {
            if (!channel_send(chan_dec2out, &chunk)) {
                DBG("chan_dec2out send error\n");
                retval = HGAP_ERR_IPC;
                break;
            }
        } else {
            free(chunk.data);
            SBUF_RESET(chunk);
            HGAP_PERROR(emit_ret, "Fatal error when decoding");
            channel_poison(chan_net2dec);
            retval = emit_ret;
            break;
        }
    }

    INFO("No more data.\n");
    DBG("Decoder poison pill\n");
    chunk.data = NULL;
    if (!channel_send(chan_dec2out, &chunk)) {
        DBG("chan_dec2out send poison error\n");

        if (retval == HGAP_SUCCESS) {
            retval = HGAP_ERR_IPC;
        }
    }
    DBG("Poison pill sent\n");

    return (void *) (intptr_t) retval;
}

struct writer_arg {
    struct channel *chan_dec2out;
    FILE *out;
};

static void *
writer(struct writer_arg* args)
{
    struct channel *chan = args->chan_dec2out;
    FILE *out = args->out;

    struct sized_buf chunk;
    ssize_t wr_ret = 0;
    int retval = HGAP_SUCCESS;

    int fd = fileno(out);
    if (fd != -1) {
        posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE);
    }

    size_t data_written = 0;
    size_t data_written_total = 0;

    while (channel_recv(chan, &chunk)) {
        if (chunk.data == NULL) {
            break;
        }

        data_written += chunk.size;
        data_written_total += chunk.size;

#if 0
        DBG("Writing %02hhx %02hhx %02hhx %02hhx\n",
                chunk.data[0], chunk.data[1], chunk.data[2], chunk.data[3]);
#endif

        if (fd != -1) {
            wr_ret = write(fd, chunk.data, chunk.size);
        } else {
            wr_ret = fwrite(chunk.data, 1, chunk.size, out);
        }

        if (wr_ret < (ssize_t) chunk.size) {
            DBG("Write error (potentially badly handled :)");
            retval = HGAP_ERR_BAD_OUT_FD;
            free(chunk.data);
            break;
        }

        if (data_written >= HGAPR_WRITE_SYNC_THRESHOLD) {
            data_written = 0;
            if (fd != -1) {
                fsync(fd);
            } else {
                fflush(out);
            }
        }

        free(chunk.data);
        SBUF_RESET(chunk);
    }

    if (fd != -1) {
        fsync(fd);
    } else {
        fflush(out);
    }

    INFO("Wrote %ld bytes.\n", data_written_total);
    DBG("Output flushed\n");

    return (void *) (intptr_t) retval;
}

int
hgap_receive(const struct hgap_config *config)
{
    int err = hgap_check_config_receiver(config);
    if (err != HGAP_SUCCESS) {
        return err;
    }

    if (wirehair_init() == 0) {
        return HGAP_ERR_WIREHAIR_ERROR;
    }
    DBG("wirehair initialized\n");

    struct hgap_decoder *dec = hgap_decoder_new();
    CHK(dec);

    size_t pkt_size = sizeof (struct sized_buf) + config->pkt_size;
    size_t pkt_chan_size = (config->mem_limit / 2) / pkt_size;
    size_t chunk_chan_size = MAX(256,
                                 (config->mem_limit / 2) / HGAP_MAX_CHUNK_SIZE);

    struct channel *chan_net2dec = channel_new(pkt_size, pkt_chan_size);
    struct channel *chan_dec2out = channel_new(sizeof(struct sized_buf),
                                               chunk_chan_size);
    CHK(chan_net2dec);
    CHK(chan_dec2out);

    // Writer thread
    pthread_t wr_thread;
    struct writer_arg wr_args = {
        .chan_dec2out=chan_dec2out,
        .out=config->out
    };
    CHK_PERROR(pthread_create(&wr_thread, NULL,
                       (void*(*)(void*))writer, &wr_args) == 0);

    // Decoder thread
    pthread_t dec_thread;
    struct decloop_arg dec_args = {
        .dec=dec,
        .chan_net2dec=chan_net2dec,
        .chan_dec2out=chan_dec2out
    };
    CHK_PERROR(pthread_create(&dec_thread, NULL,
                       (void*(*)(void*)) decloop, &dec_args) == 0);

    int retval = hgapr_net_reader(chan_net2dec, config->addr, config->port,
                                  config->timeout);

    void *tmp_ret = (void *) HGAP_SUCCESS;
    DBG("net reader ended\n");

    pthread_join(dec_thread, &tmp_ret);
    DBG("decode thread joined\n");
    if (tmp_ret != (void *) HGAP_SUCCESS) {
        retval = HGAP_SELECT_ERROR((int) (uintptr_t) tmp_ret, retval);
    }

    pthread_join(wr_thread, &tmp_ret);
    DBG("Writer joined\n");
    if (tmp_ret != (void *) HGAP_SUCCESS) {
        retval = HGAP_SELECT_ERROR((int) (uintptr_t) tmp_ret, retval);
    }

    channel_free(chan_dec2out);
    channel_free(chan_net2dec);
    hgap_decoder_free(dec);

    return retval;
}
