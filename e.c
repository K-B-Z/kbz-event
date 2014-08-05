
#include "m.h"

// all = {}
// all[chan_id] = chan
// chan.procs[(pid,tid)] = proc
// proc.stat = NONE/GETTING/PUSHING/WAITING_ACK

static int isem_new() {
	for (;;) {
		int k = 0x999888 + (rand()%0xfffff);
		char name[128];

		sprintf(name, "event_v2:sem:%d", k);
		sem_t *s = sem_open(name, O_CREAT|O_EXCL, 0600, 1);
		if (s != SEM_FAILED) {
			sem_close(s);
			return k;
		}
	}
}

static key_t shm_new_from_buf(void *buf, int len) {
}

static void shm_del(key_t k) {
}

// buffer is avaliable until next push/post/get

static void event_v2_get(int chan, void **out, int *out_len) {
}

static void event_v2_post(int chan, void *in, int in_len) {
}

static int event_v2_push(int chan, void *in, int in_len, void **out, int *out_len, int timeout) {
}

static void event_v2_ack(void *in, void *out, int out_len) {
}

int main(int argc, char *argv[]) {
	return 0;
}

