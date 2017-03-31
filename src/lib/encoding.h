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

#ifndef HGAP_ENCODING_H
#define HGAP_ENCODING_H

#include "proto.h"

/**
 * This file provides the main interfaces to hande a hairgap encoding session.
 * An "encoding session" is the operation that transforms a stream of raw data
 * chunks into network packets.
 * A "decoding session" is the operation that transforms a stream of network
 * packets into a stream of raw data to be output.
 *
 * This module contains:
 *
 *  - hgap_encoder: an encoding session, handles packet generation. The packet
 *    network sending is delegated to the user for more flexibility (see
 *    hgap_sender for a helper class).
 *  - hgap_decoder: a receiver session that eats raw hairgap network packets and
 *    emits decoded data chunks, performing error correction.
 *  - helpers to handle the handwave and teardown of the session on the sender
 *    and receiver side.
 */

// ==========================================================================
// Encoding
// ==========================================================================

/**
 * Turns chunks of data into initialized hgap_enc_chunk-s. It cannot be used
 * for multiple successive transfers.
 * It would probably be named HairgapEncodedChunkFactorySession in Java.
 *
 * In turn, an hgap_enc_chunk will emit a stream of raw hairgap packets ready
 * to be sent on the network.
 *
 * The API exposes the chunking of the datastream because initializing a
 * hgap_enc_chunk is relatively costy and may be done in a separate thread from
 * the one that actually emits packets.
 *
 * Typical (simplistic) code would be:
 *
 * #include "encoding.h"
 * #include "sender.h"
 *
 * char pkt[UDP_MTU];
 * double redund;
 * size_t size_to_send = SIZE_TO_SEND;
 * size_t pkt_sz = 0;
 * void *to_send = get_data(size_to_send);
 *
 * struct hgap_encoder *enc = hgap_encoder_new(UDP_MTU);
 * struct hgap_enc_chunk *chunk = hgap_enc_chunk_new();
 * struct hgap_sender *snd = hgap_sender_new(HOST, PORT, BYTERATE, KEEPALIVE);
 *
 * // Protocol handwave: generated with the encoder, sent with the sender
 * hgap_encoder_handwave(enc, pkt, &pkt_sz);
 * hgap_sender_control(snd, pkt, pkt_sz);
 *
 * // Modulo last chunk size handling
 * // (this is valid iff SIZE_TO_SEND % CHUNK_SIZE == 0)
 * for (int i = 0; i < SIZE_TO_SEND; i += CHUNK_SIZE) {
 *     hgap_enc_chunk_init(enc, chunk, to_send + i, CHUNK_SIZE);
 *     do {
 *         pkt_sz = UDP_MTU;
 *         redund = hgap_enc_chunk_emit(chunk, pkt, &pkt_sz);
 *         if (redund < 0) {
 *             // Error
 *         }
 *         hgap_sender_send(snd, pkt, pkt_sz);
 *     } while (redund < WANTED_REDUND);
 * }
 *
 * // Protocol teardown
 * hgap_encoder_teardown(enc, pkt, &pkt_sz);
 * hgap_sender_control(snd, pkt, pkt_sz);
 *
 * hgap_encoder_free(enc);
 * hgap_enc_chunk_free(chunk);
 * hgap_sender_free(snd);
 */
struct hgap_encoder;
struct hgap_enc_chunk;

/**
 * Initialize an hgap_encoder that will emit packets of size pkt_size.
 */
struct hgap_encoder *hgap_encoder_new(size_t pkt_size);
void hgap_encoder_free(struct hgap_encoder *enc);

/**
 * Dynamically allocates a safely initialized hgap_enc_chunk.
 */
struct hgap_enc_chunk *hgap_enc_chunk_new(void);

/**
 * Initializes a hgap_enc_chunk that will emit packets encoding the to_enc
 * buffer of size size as part of enc's stream.
 *
 * No reference is kept on to_enc.
 *
 * @param chunk can be a reused hgap_enc_chunk instance (allowing one
 *     allocation for multiple uses).
 */
int hgap_enc_chunk_init(struct hgap_encoder *enc, struct hgap_enc_chunk *chunk,
                        const void *to_enc, size_t size);

/**
 * Free memory allocated in this hgap_enc_chunk.
 */
void hgap_enc_chunk_free(struct hgap_enc_chunk *chunk);


/**
 * Write a ready-to-send hairgap raw packet to pkt. The number of calls to
 * this function is potentially unlimited: emit as many packets as you need to
 * meet your redundancy criteria.
 *
 * @param pkt its length must be at least *size
 * @param size is filled with the actual packet length
 * @return the redundancy of data after this chunk is emitted or < 0 on error.
 */
double hgap_enc_chunk_emit(struct hgap_enc_chunk *chunk, void *pkt,
                           size_t *size);

/**
 * Creates a handwave packet in pkt.
 *
 * @param pkt should be at least of len HGAP_MIN_BUF.
 * @param size *size is the available size in the pkt buffer and is filled with
 *     the actual size of the packet produced.
 * @return HGAP_SUCCESS on success or HGAP_ERR_BUFFER_TOO_SMALL if *size is too
 *     small to contain the produced packet.
 */
int hgap_encoder_handwave(struct hgap_encoder *enc, void *pkt, size_t *size);

/**
 * Same as hgap_encoder_handwave but with the teardown control packet.
 */
int hgap_encoder_teardown(struct hgap_encoder *enc, void *pkt, size_t *size);


// ==========================================================================
// Decoding
// ==========================================================================

/**
 * Receive and decode packets from the network. This API is simpler since all
 * the parameters are deduced from the packets received on the network.
 *
 * It is basically an iterator that eats raw hairgap packets and emits decoded
 * chunks of data. It is always safe to use a packet buffer of
 * HGAP_MAX_PKT_SIZE bytes.
 *
 * Typical (simplistic) code would be:
 *
 * #include "encoding.h"
 *
 * char pkt[UDP_MTU];
 * size_t pkt_sz = 0;
 * struct hgap_decoder *dec = hgap_decoder_new();
 *
 * while(RECEIVE_PKT(&pkt, &pkt_sz)) {
 *     int ret = hgap_decoder_read(dec, &pkt, pkt_sz);
 *     if (ret == -HGAP_EOT) {
 *         break; // End of transfer
 *     } else if (ret < 0) {
 *         // Error
 *     } else if (ret > 0) {
 *         void *chunk_buf = malloc(ret);
 *         assert(hgap_decoder_emit(dec, chunk_buf, ret) == HGAP_SUCCESS);
 *         OUTPUT_CHUNK(chunk_buf, ret);
 *         free(chunk_buf);
 *     } // Else continue to read
 * }
 *
 * hgap_decoder_free(dec);
 */
struct hgap_decoder;

/**
 * Describes at which point in the protocol a hgap_decoder is.
 */
enum hgap_decoder_state {
    T_NEW = 0,
    T_STARTED = 1,
    T_DATA = 2,
    T_STOPPED = 3,
};


/**
 * Initializes the decoder (not much done yet).
 */
struct hgap_decoder *hgap_decoder_new();
void hgap_decoder_free(struct hgap_decoder *dec);

/**
 * Reads a raw hairgap packet and update its internal state.
 *
 * @return size of a chunk ready to be emitted, < 0 if an error occured, 0
 *     when expecting to read a new packet.
 */
ssize_t hgap_decoder_read(struct hgap_decoder *dec, void *raw_pkt, size_t len);

/**
 * @return HGAP_SUCCESS or an HGAP_ERR_* (auth or lost chunk)
 */
int hgap_decoder_emit(struct hgap_decoder *dec, void *out_buf, size_t len);

#endif // HGAP_ENCODING_H
