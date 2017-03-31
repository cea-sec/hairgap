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

#include <sys/time.h>

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "channel.h"
#include "common.h"

uint32_t send_amount = 0;
struct timeval t1, t2;

void chan_producer(struct channel *chan) {
    uint32_t i = 0;
    void *data = NULL;
    assert(channel_elt_size(chan) >= sizeof(uint32_t));

    gettimeofday(&t1, NULL);
    for (i = 0; i < send_amount; i++) {
        // Reserve data
        data = channel_reserve(chan);
        assert(data != NULL);
        // Fill with counter
        *(uint32_t *)data = i;
        // Send data
        assert(channel_send_reserved(chan, data) == 1);
    }
}

void chan_consumer(struct channel *chan) {
    uint32_t cur = 0;
    uint32_t next = 0;
    void *data = NULL;

    assert(channel_elt_size(chan) >= sizeof(uint32_t));
    while (1) {
        data = channel_peek(chan);
        if (data == NULL) {
            break;
        }
        cur = *(uint32_t *)data;
        assert(cur == next);
        if (channel_ack(chan, data) == 0) {
            break;
        }
        next = cur + 1;

        if (next == send_amount) {
            break;
        }
    }
    gettimeofday(&t2, NULL);
    assert(next == send_amount);
}

void test_simple_concurrent(size_t elt_size, size_t capacity) {
    struct channel *chan = channel_new(elt_size, capacity);
    pthread_t send_thread;
    CHK_PERROR(pthread_create(&send_thread, NULL,
                       (void*(*)(void*)) chan_producer, chan) == 0);
    chan_consumer(chan);
    pthread_join(send_thread, NULL);
    channel_free(chan);

    double t1d = ((double) t1.tv_sec) + ((double) t1.tv_usec) / 1000000;
    double t2d = ((double) t2.tv_sec) + ((double) t2.tv_usec) / 1000000;
    double tdiff = t2d - t1d;
    double throughput = ((double) send_amount) / tdiff;
    INFO("Throughput: %lf elt/s\n", throughput);
}

int
main() {
    INFO("Test 1\n");
    send_amount = 1 * 1024 * 1024;
    test_simple_concurrent(sizeof(uint32_t), 1024);
    INFO("Test 2\n");
    test_simple_concurrent(1500, 1024);
    INFO("Test 3\n");
    test_simple_concurrent(sizeof(uint32_t), 32);
    INFO("Test 4\n");
    send_amount = 1 * 128 * 1024;
    test_simple_concurrent(sizeof(uint32_t), 2);
    INFO("Test 5\n");
    test_simple_concurrent(1500, 2);
    //test_slow_send_recv();
    return EXIT_SUCCESS;
}
