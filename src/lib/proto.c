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


#include "proto.h"

#include <endian.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <wirehair.h>

#include "common.h"

int
hgap_pkt_parse(struct hgap_pkt *pkt, const void *raw_pkt, size_t size)
{
    if (size < HGAP_HEADER_LEN) {
        return HGAP_ERR_BAD_PKT;
    }

    const struct hgap_header *net_hdr = raw_pkt;

    pkt->hdr.chunk_num = be64toh(net_hdr->chunk_num);
    pkt->hdr.chunk_size = be64toh(net_hdr->chunk_size);
    pkt->hdr.data_id = be32toh(net_hdr->data_id);
    pkt->hdr.data_size = be32toh(net_hdr->data_size);
    pkt->data = (char *) raw_pkt + HGAP_HEADER_LEN;

    if (size < pkt->hdr.data_size + HGAP_HEADER_LEN) {
        return HGAP_ERR_BAD_PKT;
    }

    return HGAP_SUCCESS;
}


enum hgap_pkt_t
hgap_pkt_type(void *pkt, size_t len)
{
    struct hgap_header *net_hdr = pkt;

    if (len < HGAP_HEADER_LEN) {
        return HGAP_PKT_UNKNOWN;
    }

    uint64_t type = be64toh(net_hdr->chunk_num);

    switch (type) {
    case HGAP_BEGIN_BEACON:
        return HGAP_PKT_BEGIN;
    case HGAP_NO_MORE_CHUNK:
        return HGAP_PKT_END;
    case HGAP_KEEP_ALIVE:
        return HGAP_PKT_KEEPALIVE;
    default:
        if (type >= HGAP_FIRST_RESERVED) {
            return HGAP_PKT_UNKNOWN;
        }

        return HGAP_PKT_DATA;
    }
}

void
hgap_write_header(const struct hgap_header *hdr, void *buf)
{
    struct hgap_header *net_hdr = buf;

    net_hdr->chunk_num = htobe64(hdr->chunk_num);
    net_hdr->chunk_size = htobe64(hdr->chunk_size);
    net_hdr->data_id = htobe32(hdr->data_id);
    net_hdr->data_size = htobe32(hdr->data_size);
}

void
hgap_header_begin(struct hgap_header *hdr)
{
    memset(hdr, 0, sizeof *hdr);
    hdr->chunk_num = HGAP_BEGIN_BEACON;
}

void
hgap_header_end(struct hgap_header *hdr)
{
    memset(hdr, 0, sizeof *hdr);
    hdr->chunk_num = HGAP_NO_MORE_CHUNK;
}

void
hgap_header_keepalive(struct hgap_header *hdr)
{
    memset(hdr, 0, sizeof *hdr);
    hdr->chunk_num = HGAP_KEEP_ALIVE;
}

void
hgap_dump(struct hgap_pkt *pkt)
{
    FAKE_USE(pkt);
    DBG("Header:\n"
        "    chunk_num:  %"PRIu64"\n"
        "    chunk_size: %"PRIu64"\n"
        "    data_id:    %"PRIu32"\n"
        "    data_size:  %"PRIu32"\n"
        "Meta:\n"
        "    total_size: %"PRIu64"\n",
        pkt->hdr.chunk_num,
        pkt->hdr.chunk_size,
        pkt->hdr.data_id,
        pkt->hdr.data_size,
        HGAP_HEADER_LEN + pkt->hdr.data_size);
}
