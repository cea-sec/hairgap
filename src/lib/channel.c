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

#include "channel.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "common.h" // Only uses xmalloc

/**
 * r is the next slot to be read, w the next slot to be written.
 * _ _ r - - - w _ _ (size = 4)
 * - w _ _ _ _ _ r - (size = 3)
 * r w _ _ _ _ _ _ _ (size = 1)
 * _ rw_ _ _ _ _ _ _ (empty)
 * - w r - - - - - - (full)
 *
 * NOTE/FIXME: the actual implementation loses 1 entry of the channel, i.e when
 * the channel is full there is still 1 unused slot. This seems to allow less
 * lock contention but is a bit sad. Suggestions are welcome.
 * I guess this could be implemented lock-free, but perf is perfectly OK for the
 * current application.
 */
struct channel {
    size_t elt_size;
    size_t capacity;

    int poisoned;

    void *elts;
    size_t wr_idx;
    size_t rd_idx;

    pthread_mutex_t mutex;
    pthread_cond_t send_cond;
    pthread_cond_t recv_cond;
};

#define POISON_CHECK(chan, ret_val) \
    if ((chan)->poisoned) { return (ret_val); }

#define channel_next(chan, idx) \
    (((idx) + 1) % (chan)->capacity)

#define channel_get(chan, idx) \
    ((chan)->elts + (((idx) % (chan)->capacity) * (chan)->elt_size))

static int
channel_is_full(struct channel *chan)
{
    return (chan->rd_idx == channel_next(chan, chan->wr_idx) &&
            !chan->poisoned);
}

static int
channel_is_empty(struct channel *chan)
{
    return (chan->rd_idx == chan->wr_idx && !chan->poisoned);
}

static void
_channel_wait_lock(struct channel *chan, int (*test)(struct channel *),
                   pthread_cond_t *cond)
{
    // First test is without lock: there is only one producer, so if the channel
    // isn't full at the moment of this call, there is no race condition
    // possible.
    if (test(chan)) {
        // Take the mutex, test and wait
        pthread_mutex_lock(&chan->mutex);
        // Re-test, as it could no longer be full between previous test and the
        // lock acquisition
        while (test(chan)) {
            pthread_cond_wait(cond, &chan->mutex);
        }

        pthread_mutex_unlock(&chan->mutex);
    }
}

struct channel *
channel_new(size_t elt_size, size_t capacity)
{
    struct channel *chan = xmalloc(sizeof(struct channel));
    if (chan == NULL) {
        return NULL;
    }
    chan->elt_size = elt_size;
    // + 1 to include the empty slot
    chan->capacity = capacity + 1;

    chan->poisoned = 0;

    chan->elts = xmalloc(elt_size * chan->capacity);
    chan->rd_idx = 0;
    chan->wr_idx = 0;

    pthread_mutex_init(&chan->mutex, NULL);
    pthread_cond_init(&chan->send_cond, NULL);
    pthread_cond_init(&chan->recv_cond, NULL);

    return chan;
}

void
channel_free(struct channel *chan)
{
    channel_poison(chan);
    pthread_cond_destroy(&chan->send_cond);
    pthread_cond_destroy(&chan->recv_cond);
    pthread_mutex_destroy(&chan->mutex);
    free(chan->elts);
    free(chan);
}

void *
channel_reserve(struct channel *chan)
{
    POISON_CHECK(chan, NULL);

    _channel_wait_lock(chan, channel_is_full, &chan->send_cond);

    // May have been poisoned while waiting
    POISON_CHECK(chan, NULL);

    void *buf = channel_get(chan, chan->wr_idx);

    return buf;
}

int
channel_send_reserved(struct channel *chan, void *data)
{
    POISON_CHECK(chan, 0);

    // Check ptr validity (channel_get handles wrapping)
    if (data == NULL || data != channel_get(chan, chan->wr_idx)) {
        return 0;
    }

    pthread_mutex_lock(&chan->mutex);
    int was_empty = channel_is_empty(chan);
    chan->wr_idx = channel_next(chan, chan->wr_idx);
    if (was_empty) {
        // Notify any waiting receiver
        pthread_cond_signal(&chan->recv_cond);
    }
    pthread_mutex_unlock(&chan->mutex);

    return 1;
}

int
channel_send(struct channel *chan, void *data)
{
    void *dst = channel_reserve(chan);
    if (dst == NULL) {
        return 0;
    }
    memcpy(dst, data, chan->elt_size);
    return channel_send_reserved(chan, dst);
}

void *
channel_peek(struct channel *chan)
{
    POISON_CHECK(chan, NULL);

    _channel_wait_lock(chan, channel_is_empty, &chan->recv_cond);

    // May have been poisoned while waiting
    POISON_CHECK(chan, NULL);

    void *data = channel_get(chan, chan->rd_idx);
    return data;
}

int
channel_ack(struct channel *chan, void *data)
{
    POISON_CHECK(chan, 0);

    // Invalid data
    if (data == NULL || data != channel_get(chan, chan->rd_idx)) {
        return 0;
    }

    pthread_mutex_lock(&chan->mutex);
    int was_full = channel_is_full(chan);
    chan->rd_idx = channel_next(chan, chan->rd_idx);
    if (was_full) {
        // Notify waiting thread
        pthread_cond_signal(&chan->send_cond);
    }
    pthread_mutex_unlock(&chan->mutex);


    return 1;
}

int
channel_recv(struct channel *chan, void *data)
{
    void *src = channel_peek(chan);
    if (src == NULL) {
        return 0;
    }
    memcpy(data, src, chan->elt_size);
    return channel_ack(chan, src);
}

void
channel_poison(struct channel *chan)
{
    chan->poisoned = 1;
    pthread_cond_signal(&chan->send_cond);
    pthread_cond_signal(&chan->recv_cond);
}

size_t
channel_elt_size(struct channel *chan)
{
    return chan->elt_size;
}
