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

#ifndef HAIRGAP_H
#define HAIRGAP_H

/**
 * Main hairgap API. This is the high level API, for better control, see:
 *     - encoding.h: core primitives of hairgap, good starting point.
 *     - proto.h: some details on the protocol.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "proto.h"

#define HGAP_DEF_IN_FILE stdin
#define HGAP_DEF_OUT_FILE stdout
#define HGAP_DEF_N_PKT 1000
// FIXME: MAX_PDU - header len
#define HGAP_DEF_PKT_SIZE 1400
#define HGAP_DEF_REDUND 1.2
#define HGAP_DEF_ADDR NULL
#define HGAP_DEF_PORT 11011
#define HGAP_DEF_BYTERATE 0
#define HGAP_DEF_KEEPALIVE 500
#define HGAP_DEF_TIMEOUT 1 * 1000 * 1000
#define HGAP_DEF_MEM_LIMIT 100 * 1024 * 1024

/**
 * in: a file object to read from when sending.
 * out: a file object to write to when receiving.
 * n_pkt: the number of packets in an error correction chunk
 * pkt_size size of a packet, hairgap protocol headers included (should
 *     typically fit in an UDP MTU).
 * redund: the desired amount of redundancy (1.2 produces 200 redundant packets
 *     for a 1000 packet long chunk).
 * addr: a string representing the dotted notation of the destination IP (e.g.:
 *     "10.0.0.2") or a hostname; represents the binding address on the
 *     receiver side, and the destination address on the sender side.
 * port: destination port (binding port on the receiver side, destination port
 *     on the sender side).
 * byterate: the max amount for bytes/second to send
 * keepalive: the keepalive period, in ms. Send a keepalive every keepalive ms.
 *     0 disables it. Sender side only.
 * timeout: the timeout (in us) after which to consider a transfer interrupted
 *     if no packets are received. 0 disables it (not recommended). Receiver
 *     side only.
 *  mem_limit: the approximate maximum amount of memory to use to buffer
 *      incoming packets and chunk (_very_ approximate).
 **/
struct hgap_config {
    FILE *in;
    FILE *out;
    uint32_t n_pkt;
    size_t pkt_size;
    double redund;
    char *addr;
    short port;
    double byterate;
    uint64_t keepalive;
    uint64_t timeout;
    size_t mem_limit;

    // FIXME: sockaddr* rather than addr?
};

// --------------------------------- Main API ----------------------------------

/**
 * Sets hgap_config to safe defaults. The addr field is NULL by default and
 * should be set by the user.
 *
 * @return 0 on success, != on failure (if hgap_config* is NULL)
 **/
int hgap_defaults(struct hgap_config *config);

/**
 * Send data as specified by config (from in to addr:port). This will start
 * 3 additional pthreads (or 2 if you disabled keepalives). Returns once the
 * transfer is complete.
 *
 * @return HGAP_SUCCESS on success, HGAP_ERR_* on failure
 **/
int hgap_send(const struct hgap_config *config);

/**
 * Similar to hgap_send but receives data from config->addr, config->port and
 * writes it to config->out.
 *
 * @return HGAP_SUCCESS on success, HGAP_ERR_* on failure
 */
int hgap_receive(const struct hgap_config *config);


// ------------------------------- Config functions ----------------------------

/**
 * Print a debug string of hgap_config to out.
 **/
void hgap_config_dump(const struct hgap_config *config, FILE *out);

/**
 * Returns HGAP_SUCCESS if the config is valid for sending data, HGAP_ERR_*
 * otherwise.
 */
int hgap_check_config_sender(const struct hgap_config *config);

/**
 * Returns HGAP_SUCCESS if the config is valid for receiving data, HGAP_ERR_*
 * otherwise.
 */
int hgap_check_config_receiver(const struct hgap_config *config);

// ---------------------------- Error functions --------------------------------

enum {
    HGAP_SUCCESS = 0,
    HGAP_EOT,
    HGAP_ERR_NO_CONFIG,
    HGAP_ERR_MTU_TOO_SMALL,
    HGAP_ERR_MTU_TOO_BIG,
    HGAP_ERR_INVALID_ADDR,
    HGAP_ERR_BAD_FD,
    HGAP_ERR_BAD_IN_FD,
    HGAP_ERR_BAD_OUT_FD,
    HGAP_ERR_FILE_READ,
    HGAP_ERR_BAD_N_PKT,
    HGAP_ERR_BAD_REDUND,
    HGAP_ERR_WIREHAIR_ERROR,
    HGAP_ERR_BUFFER_TOO_SMALL,
    HGAP_ERR_INCOMPLETE_CHUNK,
    HGAP_ERR_BAD_CHUNK,
    HGAP_ERR_BAD_PKT,
    HGAP_ERR_TIMEOUT,
    HGAP_ERR_NETWORK,
    HGAP_ERR_IPC,
    HGAP_ERR_INTERNAL,
};

/**
 * Returns a static string describing an HGAP_ERR_*.
 */
const char *hgap_err_str(int err);

#endif // HAIRGAP_H
