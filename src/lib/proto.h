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

#ifndef HGAP_PROTO_H
#define HGAP_PROTO_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <wirehair.h>

// Important note: the port looks like \m/(-_-)\m/
#define HGAP_PORT 11011

#define HGAP_NO_MORE_CHUNK  ((uint64_t) 0xffffffffffffffff)
#define HGAP_BEGIN_BEACON   ((uint64_t) 0xfffffffffffffffe)
#define HGAP_KEEP_ALIVE     ((uint64_t) 0xfffffffffffffffd)
#define HGAP_FIRST_RESERVED ((uint64_t) 0xfffffffffffffff0)

#define HGAP_HEADER_LEN sizeof(struct hgap_header)
#define HGAP_CONTROL_LEN 0
#define HGAP_SALVE_LEN 32
#define HGAP_LITTLE_CHUNK_RETRIES 128
#define HGAP_MIN_BUF (HGAP_HEADER_LEN + HGAP_CONTROL_LEN)

// A bit more than standard UDP MTU (FIXME should be adjusted)
#define HGAP_MAX_PKT_SIZE 1500
// A bit more than real max possible size
#define HGAP_MAX_CHUNK_SIZE (HGAP_MAX_PKT_SIZE * HGAP_MAX_N_PKT)
#define HGAP_MAX_DATA_SIZE (HGAP_MAX_PKT_SIZE - HGAP_HEADER_LEN)

// From wirehair doc
#define HGAP_MAX_N_PKT 64000

enum hgap_pkt_t {
    HGAP_PKT_UNKNOWN = 0,
    HGAP_PKT_BEGIN,
    HGAP_PKT_END,
    HGAP_PKT_KEEPALIVE,
    HGAP_PKT_DATA
};


/*
 * Hairgap packet format
 */
struct hgap_header {
    /// Chunk of encoded data this packet is part of
    uint64_t chunk_num;
    /// Size of the current chunk
    uint64_t chunk_size;
    /// Id of the data in this packet
    uint32_t data_id;
    /// size of payload
    uint32_t data_size;
};

struct hgap_pkt {
    struct hgap_header hdr;
    /// Ptr to data in _raw
    char *data;
};

/**
 * Parse raw data to create an hgap_pkt. The raw_pkt buffer shall be at least
 * size bytes long and live longer than pkt, as pkt will keep a reference on
 * it.
 *
 * @return HGAP_SUCCESS on success, HGAP_ERR_BAD_PKT if the packet is
 *     incoherent or too small
 */
int hgap_pkt_parse(struct hgap_pkt *pkt, const void *raw_pkt, size_t size);

/**
 * Returns the type of this packet.
 */
enum hgap_pkt_t hgap_pkt_type(void *pkt, size_t len);

/**
 * Write the header into buf. buf must be at least HGAP_HEADER_LEN long. The
 * payload can be written after HGAP_HEADER_LEN bytes.
 */
void hgap_write_header(const struct hgap_header *hdr, void *buf);

/**
 * Special header types
 */
// Announce begining of transfer
void hgap_header_begin(struct hgap_header *hdr);

// Announce end of transfer
void hgap_header_end(struct hgap_header *hdr);

// Keepalive
void hgap_header_keepalive(struct hgap_header *hdr);

void hgap_dump(struct hgap_pkt *pkt);
#ifdef DEBUG
#define HGAP_DUMP(pkt) hgap_dump(pkt);
#else
#define HGAP_DUMP(pkt)
#endif

#endif // HGAP_PROTO_H
