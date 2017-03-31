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
#include "proto.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

const char *
hgap_err_str(int err)
{
    switch (err) {
        case HGAP_SUCCESS:
            return "Success";
        case HGAP_EOT:
            return "End of transfer";
        case HGAP_ERR_NO_CONFIG:
            return "No configuration passed (logic error)";
        case HGAP_ERR_MTU_TOO_SMALL:
            return "MTU too small (should be more than "STR(HGAP_HEADER_LEN)")";
        case HGAP_ERR_MTU_TOO_BIG:
            return "MTU too big (> "STR(HGAP_MAX_PKT_SIZE)")";
        case HGAP_ERR_INVALID_ADDR:
            return "Invalid address or host";
        case HGAP_ERR_BAD_FD:
            return "Bad file descriptor";
        case HGAP_ERR_BAD_IN_FD:
            return "Bad input file descriptor";
        case HGAP_ERR_BAD_OUT_FD:
            return "Bad output file descriptor";
        case HGAP_ERR_FILE_READ:
            return "Error while reading input file";
        case HGAP_ERR_BAD_N_PKT:
            return "Bad number of packets per chunk (should be < "
                   STR(HGAP_MAX_N_PKT)")";
        case HGAP_ERR_BAD_REDUND:
            return "Bad redundancy, should be >= 1.0";
        case HGAP_ERR_WIREHAIR_ERROR:
            return "Error correction engine (wirehair) error";
        case HGAP_ERR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        case HGAP_ERR_INCOMPLETE_CHUNK:
            return "Chunk could not be reassembled (probably too many lost "
                   "packets)";
        case HGAP_ERR_BAD_CHUNK:
            return "Invalid chunk (probably too big)";
        case HGAP_ERR_BAD_PKT:
            return "Invalid packet (probably too small)";
        case HGAP_ERR_NETWORK:
            return "Unspecified network error";
        case HGAP_ERR_TIMEOUT:
            return "Receive socket probably timed out";
        case HGAP_ERR_IPC:
            return "Internal (IPC) error";
        default:
            return "Unknown error";
    }
}
