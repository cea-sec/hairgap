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

#include "limiter.h"

#include <string.h>
#include <sys/time.h>

#include "common.h"
#include "proto.h"

#define HLIM_CHK_PERIOD 1000
#define HLIM_SLEEP_PERIOD 100                // in microseconds

struct hgap_limiter {
    double byterate;
    size_t n_pkt_sent;
    size_t n_bytes_sent;
    struct timeval since;

    size_t total_data_sent;
};

static double
time_diff(struct timeval *before , struct timeval *after)
{
    double before_us = before->tv_sec + before->tv_usec / 1000000.;
    double after_us = after->tv_sec + after->tv_usec / 1000000.;

    return after_us - before_us;
}

static void
hgap_limiter_reset_rate(struct hgap_limiter *hlim)
{
    gettimeofday(&hlim->since , NULL);
    hlim->n_pkt_sent = 0;
    hlim->n_bytes_sent = 0;
}

static double
hgap_limiter_get_current_rate(struct hgap_limiter *hlim)
{
    struct timeval now;
    gettimeofday(&now , NULL);
    double diff = time_diff(&hlim->since, &now);
    double rate;
    if (diff == 0) {
        rate = 0;
    } else {
        rate = hlim->n_bytes_sent / diff;
    }
    return rate;
}

struct hgap_limiter *
hgap_limiter_new(double byterate)
{
    struct hgap_limiter *hlim = xmalloc(sizeof(struct hgap_limiter));
    hlim->byterate = byterate;
    hgap_limiter_reset_rate(hlim);
    hlim->total_data_sent = 0;
    return hlim;
}

int
hgap_limiter_limit(struct hgap_limiter *hlim, size_t len)
{
    hlim->total_data_sent += len;
    hlim->n_pkt_sent++;
    hlim->n_bytes_sent += len;
    if (hlim->byterate && hlim->n_pkt_sent > HLIM_CHK_PERIOD) {
        while (hgap_limiter_get_current_rate(hlim) > hlim->byterate) {
            usleep(HLIM_SLEEP_PERIOD);
        }
        hgap_limiter_reset_rate(hlim);
    }

    return HGAP_SUCCESS;
}

void
hgap_limiter_free(struct hgap_limiter *hlim)
{
    DBG("Sent %zu bytes.\n", hlim->total_data_sent);
    free(hlim);
}

// TODO Token bucket filter
