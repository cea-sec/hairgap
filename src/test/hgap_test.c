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

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "common.h"
#include "hairgap.h"
#include "proto.h"
#include "string.h"

void
test_check_config_sender() {
    struct hgap_config config;
    hgap_defaults(&config);
    hgap_config_dump(&config, stdout);

    assert(hgap_check_config_sender(&config) == HGAP_ERR_INVALID_ADDR);
    config.addr = "impossibru !";
    assert(hgap_check_config_sender(&config) == HGAP_ERR_INVALID_ADDR);
    config.addr = "localhost";
    assert(hgap_check_config_sender(&config) == HGAP_SUCCESS);
    config.addr = "127.0.0.1";
    assert(hgap_check_config_sender(&config) == HGAP_SUCCESS);

    config.pkt_size = 1;
    assert(hgap_check_config_sender(&config) == HGAP_ERR_MTU_TOO_SMALL);
    config.pkt_size = HGAP_MAX_PKT_SIZE + 1;
    assert(hgap_check_config_sender(&config) == HGAP_ERR_MTU_TOO_BIG);
    config.pkt_size = HGAP_DEF_PKT_SIZE;

    config.in = NULL;
    assert(hgap_check_config_sender(&config) == HGAP_ERR_BAD_IN_FD);
    config.in = HGAP_DEF_IN_FILE;

    config.n_pkt = 0;
    assert(hgap_check_config_sender(&config) == HGAP_ERR_BAD_N_PKT);
    config.n_pkt = 0;
    assert(hgap_check_config_sender(&config) == HGAP_ERR_BAD_N_PKT);
    config.n_pkt = HGAP_DEF_N_PKT;

    config.redund = 0.5;
    assert(hgap_check_config_sender(&config) == HGAP_ERR_BAD_REDUND);
    config.redund = HGAP_DEF_REDUND;
}

void
test_check_config_receiver() {
    // Code duplication is intentional for now
    struct hgap_config config;
    hgap_defaults(&config);

    assert(hgap_check_config_receiver(&config) == HGAP_ERR_INVALID_ADDR);
    config.addr = "impossibru !";
    assert(hgap_check_config_receiver(&config) == HGAP_ERR_INVALID_ADDR);
    config.addr = "localhost";
    assert(hgap_check_config_receiver(&config) == HGAP_SUCCESS);
    config.addr = "127.0.0.1";
    assert(hgap_check_config_receiver(&config) == HGAP_SUCCESS);

    config.out = NULL;
    assert(hgap_check_config_receiver(&config) == HGAP_ERR_BAD_OUT_FD);
    config.out = HGAP_DEF_OUT_FILE;
}

void
test_check_send_receive(struct hgap_config *config, size_t tr_size) {
    INFO("Send/Receive test\n");

    struct timeval t1, t2;
    gettimeofday(&t1, NULL);

    size_t in_buf_sz = tr_size;
    char *in_buf = xmalloc(in_buf_sz);

    size_t out_buf_sz = 0;
    char *out_buf = NULL;

    memset(in_buf, 0xc, in_buf_sz);
    for (size_t i = 0, j = 0; i < in_buf_sz;
            i += (config->pkt_size - HGAP_HEADER_LEN) * config->n_pkt, j++) {
        in_buf[i] = (char) j;
    }

    config->in = fmemopen(in_buf, in_buf_sz, "r");
    assert(config->in);
    config->out = open_memstream(&out_buf, &out_buf_sz);
    assert(config->out);

    int send_result = HGAP_ERR_INTERNAL;
    int receive_result = HGAP_ERR_INTERNAL;

    pthread_t receive_thread;
    CHK_PERROR(pthread_create(&receive_thread, NULL,
                       (void*(*)(void*)) hgap_receive, config) == 0);
    usleep(100);
    send_result = hgap_send(config);
    pthread_join(receive_thread, (void **)&receive_result);

    gettimeofday(&t2, NULL);
    double t1d = ((double) t1.tv_sec) + ((double) t1.tv_usec) / 1000000;
    double t2d = ((double) t2.tv_sec) + ((double) t2.tv_usec) / 1000000;
    double tdiff = t2d - t1d;
    double throughput = ((double) out_buf_sz) / (1024*1024*tdiff);
    INFO("Throughput: %lf MB/s\n", throughput);

    if (send_result != HGAP_SUCCESS) {
        HGAP_PERROR(send_result, "Hgap receive failed");
        exit(-1);
    }

    if (receive_result != HGAP_SUCCESS) {
        HGAP_PERROR(receive_result, "Hgap receive failed");
        exit(-1);
    }

    fclose(config->in);
    fclose(config->out);

    DBG("out_buf_sz(%zu), in_buf_sz(%zu)\n", out_buf_sz, in_buf_sz);
    if (in_buf_sz != out_buf_sz) {
        ERROR("out_buf_sz(%zu) != in_buf_sz(%zu)\n", out_buf_sz, in_buf_sz);
        exit(-1);
    }

    if (memcmp(in_buf, out_buf, in_buf_sz) != 0) {
        DBG("Input:\n");
        dbg_hexdump(in_buf, MIN(in_buf_sz, 0x100));
        DBG("Output:\n");
        dbg_hexdump(out_buf, MIN(in_buf_sz, 0x100));
        ERROR("in_buf != out_buf\n");
        exit(-1);
    }

    free(in_buf);
    free(out_buf);
    fprintf(stderr, "\n");
}

int
main() {
    test_check_config_sender();
    test_check_config_receiver();

    struct hgap_config config;
    hgap_defaults(&config);
    config.addr = "127.0.0.1";
    config.port = 12345;
    config.mem_limit = 1 * 1024 * 1024;
    //config.byterate = 10 * 1024 * 1024;

    fprintf(stderr, "\n");
    size_t tr_size;

    config.redund = 1.0;
    tr_size = 1*(config.pkt_size - HGAP_HEADER_LEN) - 500;
    test_check_send_receive(&config, tr_size);
    config.redund = 1.2;
    
    tr_size = 100;
    test_check_send_receive(&config, tr_size);

    tr_size = 1;
    test_check_send_receive(&config, tr_size);

    tr_size = 300L * 1024L * 1024L;
    test_check_send_receive(&config, tr_size);

    return EXIT_SUCCESS;
}
