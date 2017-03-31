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

#include "encoding.h"

#include <stdlib.h>
#include <string.h>
#include <wirehair.h>

#include "common.h"
#include "hairgap.h"
#include "proto.h"

struct hgap_enc_chunk {
    uint64_t num;
    size_t len;
    size_t pkt_size;
   
    uint64_t next_pkt_id;

    // Contains the encoded version of the data to redund
    wirehair_state wh_state;
    // Copy of the data to redund if it is too small to be wirehair encoded
    void *data;

    size_t total_gen;
};

struct hgap_encoder {
    size_t pkt_size;
    uint64_t next_chunk_num;
};

struct hgap_decoder {
    size_t pkt_size;
    struct hgap_enc_chunk *chunk;
    int chunk_complete;
    int chunk_emitted;

    enum hgap_decoder_state state;
};


// -----------------------------------------------------------------------------
// Chunk functions
// -----------------------------------------------------------------------------

static size_t
hgap_enc_chunk_pkt_payload_size(struct hgap_enc_chunk *chunk)
{
    return chunk->pkt_size - HGAP_HEADER_LEN;
}

static int
hgap_enc_chunk_is_small(struct hgap_enc_chunk *chunk)
{
    return chunk->len <= hgap_enc_chunk_pkt_payload_size(chunk);
}

static void
hgap_enc_chunk_purge(struct hgap_enc_chunk *chunk)
{
    if (chunk->data != NULL) {
        free(chunk->data);
        chunk->data = NULL;
    }
}

struct hgap_enc_chunk *
hgap_enc_chunk_new(void)
{
    struct hgap_enc_chunk *chunk = xmalloc(sizeof *chunk);
    memset(chunk, 0, sizeof *chunk);
    return chunk;
}

void
hgap_enc_chunk_free(struct hgap_enc_chunk *chunk)
{
    hgap_enc_chunk_purge(chunk);

    if (chunk->wh_state != NULL) {
        wirehair_free(chunk->wh_state);
    }

    free(chunk);
}

int
hgap_enc_chunk_init(struct hgap_encoder *enc, struct hgap_enc_chunk *chunk,
                    const void *to_enc, size_t size)
{
    chunk->data = NULL;
    chunk->num = enc->next_chunk_num++;
    chunk->len = size;
    chunk->pkt_size = MIN(enc->pkt_size, size + HGAP_HEADER_LEN);
    chunk->next_pkt_id = 0;

    // FIXME: Always copy? :(
    chunk->data = xmalloc(size);
    memcpy(chunk->data, to_enc, size);

    if (!hgap_enc_chunk_is_small(chunk)) {
        size_t wh_block_size = hgap_enc_chunk_pkt_payload_size(chunk);
        chunk->wh_state = wirehair_encode(chunk->wh_state, chunk->data, size,
                                          wh_block_size);
        if (chunk->wh_state == NULL) {
            return HGAP_ERR_WIREHAIR_ERROR;
        }
    }

    return HGAP_SUCCESS;
}

double
hgap_enc_chunk_emit(struct hgap_enc_chunk *chunk, void *pkt, size_t *size)
{
    uint64_t id = chunk->next_pkt_id++;
    uint64_t payload_size = 0;

    //DBG("Send chunk %ld pkt %ld\n", chunk->num, id);

    // Handle payload
    if (hgap_enc_chunk_is_small(chunk)) {
        payload_size = chunk->len;
        CHK(payload_size + HGAP_HEADER_LEN == chunk->pkt_size);

        // Zero the buffer
        memset(pkt, 0, chunk->pkt_size);

        // Copy the data on the begninng of the payload
        memcpy((char *) pkt + HGAP_HEADER_LEN, chunk->data, payload_size);
    } else {
        payload_size = hgap_enc_chunk_pkt_payload_size(chunk);
        if (!wirehair_write(chunk->wh_state, (int) id,
                            (char *) pkt + HGAP_HEADER_LEN)) {
            return -1.0;
        }
    }

    // Handle header
    struct hgap_header hdr = {
        .chunk_num=chunk->num,
        .chunk_size=chunk->len,
        .data_id=id,
        .data_size=payload_size,
    };
    hgap_write_header(&hdr, pkt);

#if 0
    if (1 || id == 0) {
        struct hgap_pkt p2;
        hgap_pkt_parse(&p2, pkt, chunk->pkt_size);
        DBG("Emit    %02hhx %02hhx %02hhx %02hhx    Chunk num: %lx\n",
                p2.data[0], p2.data[1], p2.data[2], p2.data[3], chunk->num);
        HGAP_DUMP(&p2);
    }
#endif

    // Update *size
    *size = chunk->pkt_size;

    // Compute redundancy
    chunk->total_gen += payload_size;
    double redund = ((double) chunk->total_gen) / ((double) chunk->len);

    return redund;
}


// Decoding part

static int
hgap_dec_chunk_init(struct hgap_enc_chunk *chunk, struct hgap_pkt *pkt)
{
    chunk->data = NULL;
    chunk->num = pkt->hdr.chunk_num;
    chunk->len = pkt->hdr.chunk_size;
    chunk->pkt_size = pkt->hdr.data_size + HGAP_HEADER_LEN;
    chunk->next_pkt_id = pkt->hdr.data_id;

    if (!hgap_enc_chunk_is_small(chunk)) {
        size_t wh_block_size = hgap_enc_chunk_pkt_payload_size(chunk);
        chunk->wh_state = wirehair_decode(chunk->wh_state, chunk->len,
                                          wh_block_size);
        if (chunk->wh_state == NULL) {
            return HGAP_ERR_WIREHAIR_ERROR;
        }
    }

    return HGAP_SUCCESS;
}

static ssize_t
hgap_dec_chunk_read(struct hgap_enc_chunk *chunk, struct hgap_pkt *pkt)
{
    // Already ready
    if (chunk->data != NULL) {
        return chunk->len;
    }

    chunk->next_pkt_id = pkt->hdr.data_id;
    uint64_t id = chunk->next_pkt_id++;

    if (hgap_enc_chunk_is_small(chunk)) {
        chunk->data = xmalloc((size_t) chunk->len);
        memcpy(chunk->data, pkt->data, chunk->len);
        return chunk->len;
    } else  {
        if (chunk->wh_state == NULL) {
            // Error :(
            return -1;
        }
        if (wirehair_read(chunk->wh_state, id, pkt->data)) {
            // Ready to reassemble
            return chunk->len;
        }
    }

    // Need more data to reassemble
    return 0;
}


// -----------------------------------------------------------------------------
// Encoder part (FIXME: name)
// -----------------------------------------------------------------------------

struct
hgap_encoder *hgap_encoder_new(size_t pkt_size)
{
    struct hgap_encoder *enc = xmalloc(sizeof *enc);

    enc->pkt_size = pkt_size;
    enc->next_chunk_num = 0;
    return enc;
}

void
hgap_encoder_free(struct hgap_encoder *enc)
{
    free(enc);
}

int
hgap_encoder_handwave(struct hgap_encoder *enc, void *pkt, size_t *size)
{
    struct hgap_header hdr;

    // Will be used later
    FAKE_USE(enc);

    if (*size < HGAP_MIN_BUF) {
        return HGAP_ERR_BUFFER_TOO_SMALL;
    }

    hgap_header_begin(&hdr);
    hgap_write_header(&hdr, pkt);
    *size = HGAP_HEADER_LEN;
    return HGAP_SUCCESS;
}

int
hgap_encoder_teardown(struct hgap_encoder *enc, void *pkt, size_t *size)
{
    struct hgap_header hdr;

    // Will be used later
    FAKE_USE(enc);

    if (*size < HGAP_MIN_BUF) {
        return HGAP_ERR_BUFFER_TOO_SMALL;
    }

    hgap_header_end(&hdr);
    hgap_write_header(&hdr, pkt);
    *size = HGAP_HEADER_LEN;
    return HGAP_SUCCESS;
}


// -----------------------------------------------------------------------------
// Decoder functions
// -----------------------------------------------------------------------------

struct hgap_decoder *
hgap_decoder_new()
{
    struct hgap_decoder *dec = xmalloc(sizeof *dec);

    dec->pkt_size = HGAP_MAX_PKT_SIZE;
    dec->chunk = hgap_enc_chunk_new();
    dec->chunk->num = -1;
    dec->chunk_complete = 1;
    dec->chunk_emitted = 1;
    dec->state = T_NEW;
    return dec;
}

void
hgap_decoder_free(struct hgap_decoder *dec)
{
    hgap_enc_chunk_free(dec->chunk);
    free(dec);
}

// 1 if packet has to be handled, 0 otherwise
static int
hgap_decoder_update_state(struct hgap_decoder *dec, void *raw_pkt, size_t size)
{
    switch (hgap_pkt_type(raw_pkt, size)) {
    case HGAP_PKT_BEGIN:
        if (dec->state == T_NEW) {
            INFO("Begin transfer...\n");
            dec->state = T_STARTED;
        }
        return 0;

    case HGAP_PKT_DATA:
        if (dec->state == T_STARTED) {
            INFO("Incoming data...\n");
            dec->state = T_DATA;
        }
        return 1;

    case HGAP_PKT_END:
        // End of transfer
        if (dec->state >= T_STARTED) {
            INFO("End of transfer\n");
            dec->state = T_STOPPED;
        }
        return 0;

    case HGAP_PKT_UNKNOWN:
        INFO("Unknown packet\n");
        /* FALLTHROUGH */
    case HGAP_PKT_KEEPALIVE:
        /* FALLTHROUGH */
    default:
        return 0;
    }
}

ssize_t
hgap_decoder_read(struct hgap_decoder *dec, void *raw_pkt, size_t len)
{
    // TODO: track lost packets
    struct hgap_pkt pkt;
    hgap_pkt_parse(&pkt, raw_pkt, len);

    int handle_pkt = hgap_decoder_update_state(dec, raw_pkt, len);

    if (dec->state < T_DATA) {
        return 0;
    }

    if (dec->state == T_STOPPED) {
        return -HGAP_EOT;
    }

    if (!handle_pkt) {
        return 0;
    }

#if 0
    if (1 || pkt.hdr.data_id == 0) {
        DBG("Packet  %02hhx %02hhx %02hhx %02hhx    Chunk num: %lx\n",
                pkt.data[0], pkt.data[1], pkt.data[2], pkt.data[3], pkt.hdr.chunk_num);
        HGAP_DUMP(&pkt);
    }
#endif

    // New chunk number
    if (pkt.hdr.chunk_num != dec->chunk->num) {
        // If incoherent chunk, error (new chunk but previous is incomplete)
        if (!dec->chunk_complete) {
            ERROR("Error: missed too many packets "
                  "(cur chunk: %lu, last_chunk: %lu, cur_id: %u\n",
                  pkt.hdr.chunk_num, dec->chunk->num, pkt.hdr.data_id);
            return -HGAP_ERR_INCOMPLETE_CHUNK;
        }

        if (pkt.hdr.chunk_size > HGAP_MAX_CHUNK_SIZE) {
            return -HGAP_ERR_BAD_CHUNK;
        }

        // Purge the chunk (enc/dec is the same struct)
        hgap_enc_chunk_purge(dec->chunk);
        // Reinit chunk from first packet of the new chunk
        hgap_dec_chunk_init(dec->chunk, &pkt);
        dec->chunk_complete = 0;
        dec->chunk_emitted = 0;
    } else if (dec->chunk_complete) {
        // Already ready
        if (dec->chunk_emitted) {
            return 0;
        } else {
            return dec->chunk->len;
        }
    }

    // Incorporate this packet in the decoding state of the current chunk
    ssize_t ready = hgap_dec_chunk_read(dec->chunk, &pkt);
    if (ready > 0) {
        dec->chunk_complete = 1;
    }

    return ready;
}

int
hgap_decoder_emit(struct hgap_decoder *dec, void *out_buf, size_t len)
{
#if 0
    DBG("Reassemble chunk %lu of size %zu\n", dec->chunk->num, dec->chunk->len);
#endif
    if (!dec->chunk_complete) {
        return HGAP_ERR_INCOMPLETE_CHUNK;
    }

    if (len < dec->chunk->len) {
        return HGAP_ERR_BUFFER_TOO_SMALL;
    }

    // Actual data emission
    if (hgap_enc_chunk_is_small(dec->chunk)) {
        CHK(dec->chunk->data);
        memcpy(out_buf, dec->chunk->data, dec->chunk->len);
    } else if (!wirehair_reconstruct(dec->chunk->wh_state, out_buf)) {
        return HGAP_ERR_WIREHAIR_ERROR;
    }

    dec->chunk_emitted = 1;

    return HGAP_SUCCESS;
}
