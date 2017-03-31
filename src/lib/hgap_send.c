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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "channel.h"
#include "common.h"
#include "sender.h"
#include "encoding.h"

struct read_loop_arg {
    const struct hgap_config *config;
    struct channel *chan;
};

static void
read_chunk(FILE *in, struct sized_buf *buf)
{
    size_t to_read = buf->size;
    buf->size = 0;
    ssize_t read_ret = 0;
    while (to_read > 0 &&
           (read_ret = fread(buf->data + buf->size, 1, to_read, in)) > 0) {
        buf->size += read_ret;
        to_read -= read_ret;
    }
}


static void *
read_loop(const struct read_loop_arg *args)
{
    struct channel *chan = args->chan;
    const struct hgap_config *config = args->config;
    FILE *in_file = config->in;

    int more_data = 1;
    struct sized_buf *buf = NULL;
    size_t buf_size = channel_elt_size(chan) - sizeof(struct sized_buf);

    int retval = HGAP_SUCCESS;

    if (in_file == NULL) {
        ERROR("Bad input file descriptor\n");
        retval = HGAP_ERR_BAD_IN_FD;
        more_data = 0;
    }

    while (more_data) {
        buf = channel_reserve(chan);
        CHK(buf);
        buf->size = buf_size;
        buf->data = buf->content;

        read_chunk(in_file, buf);
        if (ferror(in_file)) {
            PWARN("Error while reading input file");
            retval = HGAP_ERR_FILE_READ;
            break;
        }

        if (!channel_send_reserved(chan, buf)) {
            DBG("chan_in2enc send error\n");
            retval = HGAP_ERR_IPC;
            break;
        }

        if (feof(in_file)) {
            DBG("eof");
            break;
        }
    }

    // Poison chunk
    buf = channel_reserve(chan);
    if (buf != NULL) {
        SBUF_RESET(*buf);
        if (!channel_send_reserved(chan, buf)) {
            DBG("chan_in2enc send poison error\n");
            retval = HGAP_ERR_IPC;
        }
    }

    return (void *) (intptr_t) retval;
}

struct encode_loop_arg {
    struct hgap_encoder *enc;
    struct channel *chan_in2enc;
    struct channel *chan_enc2net;
};

static void *
encode_loop(const struct encode_loop_arg* args)
{
    struct hgap_encoder *enc = args->enc;
    struct channel *chan_in2enc = args->chan_in2enc;
    struct channel *chan_enc2net = args->chan_enc2net;

    struct hgap_enc_chunk *chunk = NULL;
    struct sized_buf *to_enc = NULL;
    int retval = HGAP_SUCCESS;
    int ret;

    // Continue while reading from the channel is possible
    while ((to_enc = channel_peek(chan_in2enc)) != NULL) {
        // Poison
        if (to_enc->data == NULL) {
            break;
        }

        // New chunk, will be freed by next thread
        chunk = hgap_enc_chunk_new();
        if (chunk == NULL) {
            DBG("Error while allocating a chunk.\n");
            retval = HGAP_ERR_INTERNAL;
            break;
        }

#if 0
        DBG("Reading %02hhx %02hhx %02hhx %02hhx\n", to_enc.data[0], to_enc.data[1], to_enc.data[2], to_enc.data[3]);
#endif

        // Pre-encoding
        ret = hgap_enc_chunk_init(enc, chunk, to_enc->data, to_enc->size);
        if (ret != HGAP_SUCCESS) {
            HGAP_PERROR(ret, "Error while encoding chunk");
            retval = ret;
            break;
        }

        if (!channel_send(chan_enc2net, &chunk)) {
            DBG("chan_enc2net send error\n");
            retval = HGAP_ERR_IPC;
            break;
        }

        chunk = NULL;

        // Encoded, tell the channel that it can reuse the buffer
        channel_ack(chan_in2enc, to_enc);
    }

    if (chunk != NULL) {
        hgap_enc_chunk_free(chunk);
    }

    // Propagate poison
    chunk = NULL;
    if (!channel_send(chan_enc2net, &chunk)) {
        DBG("chan_enc2net send poison error\n");
        retval = HGAP_ERR_IPC;
    }

    return (void *) (intptr_t) retval;
}

static int
send_loop(const struct hgap_config *config, struct hgap_encoder *enc,
          struct channel *chan_enc2net)
{
    size_t data_sent = 0;
    double cur_redund = 0;
    double redund = config->redund;
    size_t pkt_size = config->pkt_size;
    // Temporary var to receive actual length of packet
    size_t send_size = pkt_size;
    void *pkt = xmalloc(pkt_size);
    memset(pkt, 0, pkt_size);

    int more_data = 1;
    int retval = HGAP_SUCCESS;

    struct hgap_sender *hs = hgap_sender_new(config->addr, config->port,
                                             config->byterate,
                                             config->keepalive);
    if (hs == NULL) {
        retval = HGAP_ERR_INTERNAL;
        goto send_loop_fail;
    }

    struct hgap_enc_chunk *chunk = NULL;

    // Handwave (send control salve to announce the transfer)
    int ret;
    if ((ret = hgap_encoder_handwave(enc, pkt, &send_size)) != HGAP_SUCCESS) {
        HGAP_PERROR(ret, "Handwave");
        retval = HGAP_ERR_BUFFER_TOO_SMALL;
        goto send_loop_fail;
    }
    
    if (hgap_sender_control(hs, pkt, send_size) != 0) {
        perror("Panic: unexpected network error");
        retval = HGAP_ERR_NETWORK;
        goto send_loop_fail;
    }
    send_size = pkt_size;

    while (more_data) {
        // Get input chunk
        if (!channel_recv(chan_enc2net, (void *)&chunk)) {
            DBG("chan_enc2net receive error\n");
            retval = HGAP_ERR_IPC;
            goto send_loop_fail;
        }

        // Poison (NULL) chunk => end of transfer
        if (chunk == NULL) {
            more_data = 0;
            break;
        }

        // Generate and send all packets for this encoding chunk
        do { 
            cur_redund = hgap_enc_chunk_emit(chunk, pkt, &send_size);
            if (cur_redund < 0) {
                retval = HGAP_ERR_WIREHAIR_ERROR;
                more_data = 0;
            } else {
                size_t sent = hgap_sender_send(hs, pkt, send_size);
                data_sent += sent;
            }
            send_size = pkt_size;
        } while (cur_redund < redund);

        hgap_enc_chunk_free(chunk);
        chunk = NULL;
    }

    INFO("Sent all chunks.\n");
    INFO("%ld  bytes sent.\n", data_sent);
    send_size = pkt_size;

    // Proper teardown only on proper exit
    // TODO: err handling?
    if (!more_data) {
        hgap_encoder_teardown(enc, pkt, &send_size);
        hgap_sender_control(hs, pkt, send_size);
    }

send_loop_fail:
    free(pkt);
    hgap_sender_free(hs);

    return retval;
}

int
hgap_send(const struct hgap_config *config)
{
    int err;
    if ((err = hgap_check_config_sender(config)) != HGAP_SUCCESS) {
        return err;
    }

    if (wirehair_init() == 0) {
        return HGAP_ERR_WIREHAIR_ERROR;
    }
    DBG("wirehair initialized\n");

    // Alternatively, read only multiple of pages
    // size_t buf_size = PAGE_ROUND_DOWN(config->n_pkt * config->pkt_size);
    size_t buf_size = config->n_pkt * (config->pkt_size - HGAP_HEADER_LEN);

    // Shared structure allocation
    struct hgap_encoder *enc = hgap_encoder_new(config->pkt_size);
    CHK(enc != NULL);

    // FIXME: hardcoded channel size, should depend on config (mem_limit)
    struct channel *chan_in2enc = channel_new(
            sizeof (struct sized_buf) + buf_size, 16);
    struct channel *chan_enc2net = channel_new(
            sizeof (struct hgap_enc_chunk *), 16);
    CHK(chan_in2enc);
    CHK(chan_enc2net);

    pthread_t read_thread;
    const struct read_loop_arg rdargs = {
        .chan=chan_in2enc,
        .config=config,
    };
    DBG("Create read_thread\n");
    CHK_PERROR(pthread_create(&read_thread, NULL,
                       (void*(*)(void*)) read_loop, (void *)&rdargs) == 0);

    pthread_t encode_thread;
    const struct encode_loop_arg encargs = {
        .enc=enc,
        .chan_in2enc=chan_in2enc,
        .chan_enc2net=chan_enc2net,
    };
    DBG("Create encode_thread\n");
    CHK_PERROR(pthread_create(&encode_thread, NULL,
                       (void*(*)(void*)) encode_loop, (void *)&encargs) == 0);

    DBG("Start send_loop\n");
    int retval = send_loop(config, enc, chan_enc2net);
    void *tmp_ret = (void *) HGAP_SUCCESS;

    pthread_join(read_thread, &tmp_ret);
    if (tmp_ret != (void *) HGAP_SUCCESS) {
        retval = HGAP_SELECT_ERROR((int) (uintptr_t) tmp_ret, retval);
    }

    pthread_join(encode_thread, (void **)&tmp_ret);
    if (tmp_ret != (void *) HGAP_SUCCESS) {
        retval = HGAP_SELECT_ERROR((int) (uintptr_t) tmp_ret, retval);
    }

    channel_free(chan_enc2net);
    channel_free(chan_in2enc);
    hgap_encoder_free(enc);

    return retval;
}
