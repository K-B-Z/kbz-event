#pragma once

#define EXPORT __attribute__ ((visibility ("default")))

EXPORT int kbz_event_get(int chan_id, int timeout, void **out, int *out_len);
EXPORT int kbz_event_post(int chan_id, void *in, int in_len);
EXPORT int kbz_event_ack(void *in, void *out, int out_len);

