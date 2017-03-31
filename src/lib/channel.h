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

#ifndef HGAP_CHANNEL_H
#define HGAP_CHANNEL_H

#include <stddef.h>

/**
 * An abstraction over a single producer single consumer channel of data.
 *
 * Implemented as a fixed size ring buffer. Should be lock-free except when
 * blocking (empty/full queue).
 *
 * Please note that all the data is allocated once.
 */
struct channel;

/**
 * Allocates the channel to transfer elements of size elt_size. channel_send
 * will block if capacity elements already are in the queue.
 *
 * Effectivley allocates elt_size * (capacity + 1) bytes.
 */
struct channel *channel_new(size_t elt_size, size_t capacity);

/**
 * Frees resources associated with this channel.
 */
void channel_free(struct channel *chan);

/**
 * Reserves an elt_size-long buffer on the channel and returns a pointer to it.
 * Multiple reservations will return the same pointer as long as
 * channel_send_reserved has not been called.
 *
 * If channel_send is called after a reservation, the data passed to it will
 * override the buffer returned by the reservation.
 */
void *channel_reserve(struct channel *chan);

/**
 * Send the previously reserved data on the channel.
 *
 * @return 1 on success, 0 on failure or if data is not a valid reserved
 *     buffer.
 */
int channel_send_reserved(struct channel *chan, void *data);

/**
 * Send the data pointed by data of size elt_size (see channel_init).
 *
 * This function copies data in the channel. See channel_reserve and
 * channel_send_reserved for a more efficient approach.
 *
 * @return 1 on success, 0 on failure
 */
int channel_send(struct channel *chan, void *data);

/**
 * Get the next element to be read on the channel. This is meant to retrieve
 * an element to use it before calling channel_ack that allows it to be written
 * again.
 */
void *channel_peek(struct channel *chan);

/**
 * Signal an element gotten by channel_peek as read and ready to be recycled.
 * Further uses of data will result in undefined behaviour.
 */
int channel_ack(struct channel *chan, void *data);

/**
 * Receive data of size elt_size (see channel_init) in the buffer pointed by
 * data.
 *
 * Note that this function copies an internal buffer to data. See channel_peek
 * and channel_ack for a more efficient approach.
 *
 * @return 1 on success, 0 on failure
 */
int channel_recv(struct channel *chan, void *data);

/**
 * Poisons the channel so that any further send/receive will fail.
 */
void channel_poison(struct channel *chan);

/**
 * Returns the size of the elements transfered on this channel
 */
size_t channel_elt_size(struct channel *chan);

#endif // HGAP_CHANNEL_H
