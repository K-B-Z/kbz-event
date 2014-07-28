/*
	 Copyright (C) 2014 Frank Xie

	 This program is free software: you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation, either version 3 of the License, or
	 (at your option) any later version.

	 This program is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.

	 You should have received a copy of the GNU General Public License
	 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#define EXPORT __attribute__ ((visibility ("default")))

EXPORT int kbz_event_get(int chan_id, int timeout, void **out, int *out_len);
EXPORT int kbz_event_post(int chan_id, void *in, int in_len);
EXPORT int kbz_event_push(int chan_id, void *in, int in_len, void **out, int *out_len, int timeout);
EXPORT int kbz_event_ack(void *in, void *out, int out_len);

