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

#ifndef HGAP_SENDER_H
#define HGAP_SENDER_H

#include <stddef.h>
#include <stdint.h>

#include <sys/types.h>

/**
 * The purpose of this structure is to encapsulate rate limiting + keepalive.
 * Apart from the encoder handwave and the encoder teardown, it takes pre-built
 * packets to send.
 */
struct hgap_sender;

/**
 * Creates an hgap_sender that will send on socket at maximum rate byterate.
 * Creation starts the keepalive. If keepalive is 0, no keepalive is started.
 */
struct hgap_sender *hgap_sender_new(char *host, short port, uint64_t byterate,
                                    uint32_t keepalive);

/**
 * Free any memory associated with this hgap_sender
 */
void hgap_sender_free(struct hgap_sender *hs);

/**
 * Send a single packet.
 */
ssize_t hgap_sender_send(struct hgap_sender *hs, void *pkt, size_t size);

/**
 * Send a control salve of a given packet, see encoding.h for control packet
 * generation (e.g. hgap_encoder_handwave and hgap_encoder_teardown).
 *
 * Being unidirectional and redunded, hairgap protocol control packets receive
 * a special treatment: they are sent multiple times in the hope of one reaching
 * the destination.
 *
 * @return 0 on success, return value of sendto(2) otherwise.
 */
ssize_t hgap_sender_control(struct hgap_sender *hs, void *pkt, size_t size);

#endif // HGAP_SENDER_H
