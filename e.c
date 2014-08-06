
#include "utils.h"

// chans= {}
// chans[chan_id] = chan
// chan.procs[(pid,tid)] = proc
// chan.posts = {.., 12:post12, 13:post13, ..}
// chan.post_s ~ chan.post_e = post id range
// proc.stat = NONE/GETTING/PUSHING/WAITING_ACK
// proc.post_i = last processed post id

// buffer is avaliable until next push/post/get

static iter_t chan_procs_iter(ctrl_t **_c, int chan_id) {
	off_t _chan;
	
	if (!dict_get_or_new(_c, ctrl(_c)->chans, chan_id, &_chan)) {
		chan(_chan)->procs = dict_new(_c);
	}

	return dict_iter(_c, chan(_chan)->procs);
}

static int pid_tid_exists(int pid, int tid) {
	char buf[128];
	sprintf(buf, "/proc/%d/task/%d", pid, tid);
	return !access(buf, F_OK);
}

static void chan_procs_check(ctrl_t **_c, int chan_id) {
	iter_t it = chan_procs_iter(_c, chan_id);

	while (iter_next(_c, &it)) {
		proc_t *p = proc(_c, it.v);
		if (!pid_tid_exists(p->pid, p->tid)) 
			item(_c, it->h)->stat = FREED;
	}
}

static void chan_find_proc_self(ctrl_t **_c, off_t _chan, off_t *_p) {
	int pid = getpid();
	int tid = gettid();
	int k = pid*33 + tid;

	off_t _proc;
	if (!dict_get_or_new(_c, chan(_chan)->procs, k, &_proc)) {
		proc(_c, _proc)->pid = pid;
		proc(_c, _proc)->tid = tid;
	}

	*_p = proc;
}

static void proc_prepare_shm(ctrl_t **_c, off_t o_chan, off_t o_proc, int post_i, int *_ishm) {
	off_t o_post;
	if (!dict_get(chan(_c, o_chan)->posts, post_i, &o_post))
		bug(0);
	if (proc(_c, o_proc)->shm)
		ishm_del(proc(_c, o_proc)->shm);
	proc(_c, o_proc)->shm = ishm_clone(post(_c, o_post)->shm);
	*_ishm = proc(_c, o_proc)->shm;
}

static void proc_prepare_sem(ctrl_t **_c, off_t _p, int *_isem) {
	if (proc(_c, _p)->sem)
		isem_del(proc(_c, _p)->sem);
	proc(_c, _p)->sem = isem_new();
	*_isem = proc(_c, _p)->sem;
}

static void proc_find_matched_post_else_wait(ctrl_t **_c, off_t _ch, off_t _p, int *_ishm, int *_isem) {
	if (proc(_c, _p)->post_i < chan(_c, _ch)->post_s) {
		proc_prepare_shm(_c, _ch, _p, chan(_c, _ch)->post_s, _ishm);
		proc(_c, _p)->post_i = chan(_c, _ch)->post_s;
	} else if (proc(_c, _p)->post_i < chan(_c, _ch)->post_e) {
		proc_prepare_shm(_c, _ch, _p, proc(_c, _p)->post_i);
		proc(_c, _p)->post_i += 1;
	} else {
		proc_prepare_sem(_c, _p, _isem);
	}
}

static void event_v2_get(int chan_id, void **out, int *out_len) {
	for (;;) {
		int ishm = 0;
		int isem = 0;
		int quit = 0;

		ctrl_t **_c = ctrl_get();
		off_t _ch, _p;
		chan_get(_c, chan_id, &_ch);
		chan_procs_check(_c, _ch);
		chan_find_proc_self(_c, _ch, &_p);
		if (proc(_p)->stat != NONE) {
			quit = 1;
		} else {
			proc_find_matched_post_else_wait(_c, _ch, _p, &ishm, &isem);
		}
		ctrl_put(_c);

		if (quit)
			break;

		if (ishm) {
			ishm_get_bufsize(ishm, out, out_len);
			break;
		}

		if (isem) 
			isem_down(isem);
	}
}

static void event_v2_post(int chan, void *in, int in_len) {
	ctrl_get();
	put_it_into_posts();
	notify_sleep_getting_proc();
	ctrl_put();
}

static int event_v2_push(int chan, void *in, int in_len, void **out, int *out_len, int timeout) {
	ctrl_get();
	put_it_into_posts();
	notify_sleep_getting_proc();
	waiting_for_ack();
	ctrl_put();
}

static void event_v2_ack(void *in, void *out, int out_len) {
	ctrl_get();
	ctrl_put();
}

int main(int argc, char *argv[]) {
	return 0;
}

