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

#ifndef HGAP_LIMITER_H
#define HGAP_LIMITER_H

#include <stddef.h>

struct hgap_limiter;

struct hgap_limiter *hgap_limiter_new(double byterate);
int hgap_limiter_limit(struct hgap_limiter* hlim, size_t len);
void hgap_limiter_free(struct hgap_limiter* hlim);

#endif // HGAP_LIMITER_H
