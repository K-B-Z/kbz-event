
#include <unistd.h>  
#include <errno.h>  
#include <stdint.h>  
#include <fcntl.h>  
#include <stdlib.h>  
#include <stdio.h>  
#include <string.h>  
#include <semaphore.h>  
#include <pthread.h>  
#include <sys/shm.h>  
#include <sys/mman.h>  
#include <sys/stat.h>  
#include <sys/ipc.h>  
#include <sys/sem.h>  
#include <sys/types.h>  
#include <sys/syscall.h>

#include "util.h"

// chans= {}
// chans[chan_id] = chan
// chan.procs[(pid,tid)] = proc
// chan.posts = [] # post queue
// chan.post_s ~ chan.post_e # post queue
// proc.stat = NONE/WAITING
// proc.post_i = last processed post id

// buffer is avaliable until next push/post/get

#define PROCS_NR 128
#define POSTS_NR 1024
#define CHANS_NR 5

enum {
	POST,
	PUSH,
	ACK,
};

typedef struct {
	int type;
	int shm;
	int proc;
	int chan_id;
	uint64_t id;
	uint64_t ack_id;
} post_t;

enum {
	NONE,
	WAITING,
};

typedef struct {
	int stat;
	int pid, tid;
	int sem;
	uint64_t post_i;
	uint64_t ack_i;
	void *buf;
} proc_t;

typedef struct {
	proc_t procs[PROCS_NR];
	int proc_nr;

	post_t posts[POSTS_NR];
	uint64_t post_s, post_e;
	int post_nr;
} chan_t;

typedef struct {
	chan_t chans[CHANS_NR];
} ctrl_t;

static const char *ctrl_lock_name = "event_v2:lock";
static const key_t ctrl_shm_key = 0xffeeffee;

ctrl_t *ctrl_get() {
	sem_t *lock = sem_open(ctrl_lock_name, 0);
	if (lock == SEM_FAILED)
		lock = sem_open(ctrl_lock_name, O_CREAT, 0777, 1);
	if (lock == SEM_FAILED)
		return NULL;
	log("wait");
	sem_wait(lock);
	log("wait done");

	int shm = shmget(ctrl_shm_key, sizeof(ctrl_t), 0777);
	if (shm < 0) 
		shm = shmget(ctrl_shm_key, sizeof(ctrl_t), 0777|IPC_CREAT);
	if (shm < 0) 
		return NULL;
	return (ctrl_t *)shmat(shm, NULL, 0);
}

void ctrl_put(ctrl_t *c) {
	sem_t *lock = sem_open(ctrl_lock_name, 0);
	sem_post(lock);

	shmdt(c);
}

static void ctrl_dump(ctrl_t *c) {
	int i;

	for (i = 0; i < CHANS_NR; i++) {
		chan_t *ch = &c->chans[i];
		fprintf(stderr, "chan #%d\n", i);
		fprintf(stderr, "  proc_nr %d\n", ch->proc_nr);
		fprintf(stderr, "  post_nr %d\n", ch->post_nr);
	}
}

int ishm_new(int len) {
	for (;;) {
		int shm = shmget(ctrl_shm_key + (rand()&0xfffff), len, 0777|IPC_CREAT|IPC_EXCL);
		if (shm > 0)
			return shm;
	}
}

int ishm_new_from_buf(void *buf, int buf_len, void *meta, int meta_len) {
	int k = ishm_new(buf_len + meta_len);
	void *p = shmat(k, NULL, 0);

	if (meta_len) {
		memcpy(p, meta, meta_len);
		memcpy(p + meta_len, buf, buf_len);
	} else {
		memcpy(p, buf, buf_len);
	}

	shmdt(p);
	return k;
}

void ishm_del(int i) {
	semctl(i, 0, IPC_RMID, 0);
}

int ishm_len(int i) {
	struct shmid_ds ds;
	shmctl(i, IPC_STAT, &ds);
	return ds.shm_segsz;
}

static const char *isem_fmt = "event_v2:sem:%d";

int isem_new(int n) {
	for (;;) {
		int k = ctrl_shm_key + (rand()%0xfffff);

		char name[128];
		sprintf(name, isem_fmt, k);

		sem_t *s = sem_open(name, O_CREAT|O_EXCL, 0777, n);
		if (s != SEM_FAILED) {
			sem_close(s);
			return k;
		}
	}
}

void isem_up(int k) {
	char name[128];
	sprintf(name, isem_fmt, k);

	sem_t *s = sem_open(name, 0);
	if (s == SEM_FAILED)
		return;
	sem_post(s);
}

void isem_down_timeout(int k, int timeout) {
	char name[128];
	sprintf(name, isem_fmt, k);

	sem_t *s = sem_open(name, 0);
	if (s == SEM_FAILED)
		return;

	struct timespec ts;
	ts.tv_sec = timeout/1000;
	ts.tv_nsec = (timeout%1000)*1000000;
	sem_timedwait(s, &ts);
}

int gettid() {
	return syscall(SYS_gettid);
}

static int pid_tid_exists(int pid, int tid) {
	char buf[128];
	sprintf(buf, "/proc/%d/task/%d", pid, tid);
	return !access(buf, F_OK);
}

static proc_t *chan_free_procs(chan_t *ch) {
	int i;

	for (i = 0; i < PROCS_NR; i++) {
		proc_t *p = &ch->procs[i];

		if (!pid_tid_exists(p->pid, p->tid)) {
			memset(p, 0, sizeof(proc_t));
			ch->proc_nr--;
			return p;
		}
	}

	return NULL;
}

static proc_t *chan_get_or_new_proc(chan_t *ch, int pid, int tid) {
	int i;
	proc_t *p_free = NULL;

	for (i = 0; i < PROCS_NR; i++) {
		proc_t *p = &ch->procs[i];
		if (p->pid == pid && p->tid == tid)
			return p;
		if (p->pid == 0 && p->tid == 0)
			p_free = p;
	}

	if (p_free == NULL)
		p_free = chan_free_procs(ch);
	if (p_free == NULL)
		return NULL;

	p_free->pid = pid;
	p_free->tid = tid;
	ch->proc_nr++;
	return p_free;
}

typedef int (*check_post_cb_t)(post_t *, void *);
typedef int (*check_proc_cb_t)(chan_t *, proc_t *, void *);

static post_t *proc_get_post(chan_t *ch, proc_t *p, check_post_cb_t cb, void *cb_p) {
	if (p->post_i < ch->post_s) 
		p->post_i = ch->post_s;
	while (p->post_i < ch->post_e) {
		post_t *po = &ch->posts[p->post_i % POSTS_NR];
		p->post_i++;

		if (cb(po, cb_p) == 0)
			return po;
	}
	return NULL;
}

static int wait_post(
	int chan_id, int timeout, void **out, int *out_len,
	check_proc_cb_t check_proc, check_post_cb_t check_post, void *cb_p
) {
	for (;;) {
		ctrl_t *c = ctrl_get();
		if (c == NULL)
			return -EINVAL;

		chan_t *ch = &c->chans[chan_id];

		proc_t *p = chan_get_or_new_proc(ch, getpid(), gettid());
		if (p == NULL) {
			ctrl_put(c);
			return -ENOMEM;
		}

		if (check_proc) {
			int r = check_proc(ch, p, cb_p);
			if (r) {
				ctrl_put(c);
				return r;
			}
		}

		post_t *po = proc_get_post(ch, p, check_post, cb_p);
		if (po == NULL) {
			int sem = isem_new(0);
			p->stat = WAITING;
			p->sem = sem;
			ctrl_put(c);

			// waiting 
			isem_down_timeout(sem, timeout);
			continue;
		}

		p->stat = NONE;
		if (p->buf) 
			shmdt(p->buf);
		p->buf = shmat(po->shm, NULL, 0);
		*out = p->buf;
		*out_len = ishm_len(po->shm);

		ctrl_put(c);
		return 0;
	}
}

static int enque_post(int chan_id, int type, uint64_t ack_id, void *in, int in_len, post_t *cb) {
	ctrl_t *c = ctrl_get();
	if (c == NULL)
		return -ENOENT;

	chan_t *ch = &c->chans[chan_id];
	post_t *po;

	if (ch->post_nr == POSTS_NR) {
		po = &ch->posts[ch->post_s % POSTS_NR];
		ishm_del(po->shm);
		po->shm = 0;
		ch->post_s++;
		ch->post_nr--;
	}

	po = &ch->posts[ch->post_e % POSTS_NR];
	po->type = type;
	po->id = ch->post_e;
	po->ack_id = ack_id;
	po->chan_id = chan_id;
	po->shm = ishm_new_from_buf(in, in_len, po, sizeof(post_t));
	if (cb)
		*cb = *po;
	ch->post_nr++;
	ch->post_e++;

	int i;
	for (i = 0; i < PROCS_NR; i++) {
		proc_t *p = &ch->procs[i];
		if (p->stat == WAITING) {
			isem_up(p->sem);
		}
	}

	ctrl_put(c);
	return 0;
}

static int post_is_normal(post_t *p, void *_) {
	return p->type == POST || p->type == PUSH;
}

static int event_v2_get(int chan_id, int timeout, void **out, int *out_len) {
	return wait_post(chan_id, timeout, out, out_len, NULL, post_is_normal, NULL);
}

static int event_v2_post(int chan_id, void *in, int in_len) {
	return enque_post(chan_id, POST, 0, in, in_len, NULL);
}

static int push_check_proc(chan_t *ch, proc_t *p, void *_) {
	post_t *po = (post_t *)_;
	if (po->id < ch->post_s)
		return -ENOENT;
	return 0;
}

static int push_check_post(post_t *po, void *_) {
	post_t *po_push = (post_t *)_;
	return !(po->type == ACK && po->ack_id == po_push->id);
}

static int event_v2_push(int chan_id, void *in, int in_len, void **out, int *out_len, int timeout) {
	log("chan=%d in_len=%d", chan_id, in_len);
	post_t po;
	enque_post(chan_id, PUSH, 0, in, in_len, &po);
	return wait_post(chan_id, timeout, out, out_len, push_check_proc, push_check_post, &po);
}

static int event_v2_ack(void *in, void *out, int out_len) {
	post_t *po = (post_t *)(in - sizeof(post_t));
	if (po->type != PUSH)
		return -EINVAL;
	return enque_post(po->chan_id, ACK, po->id, out, out_len, NULL);
}

static void usage() {
	fprintf(stderr, 
		"  -d           dump all info\n"
		"  -u 1 '{}'    push string '{}' to channel 1\n"
		"  -p 2 '{}'    post string to channel 2\n"
		"  -g 3         get string from channel 3\n"
		"  -v           verbose output\n"
		"  -t 0         run test 0\n"
	);
	exit(0);
}

static void test(int n) {
	log("test %d", n);

	pthread_mutexattr_t mutex_attr;
	pthread_mutexattr_init(&mutex_attr);
	pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

	int shm = shmget(ctrl_shm_key, sizeof(ctrl_t), 0777);
	if (shm < 0) {
		shm = shmget(ctrl_shm_key, sizeof(ctrl_t), 0777|IPC_CREAT);
	}
	char *p = (char *)shmat(shm, NULL, 0);

	switch (n) {
	case 0:
		break;
	case 1:
		break;
	}
}

int main(int argc, char *argv[]) {
	if (argc == 1)
		usage();
	
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d")) {
			log_set_level(LOG_DEBUG);
		}
		if (!strcmp(argv[i], "-u")) {
			if (i+2 >= argc) 
				usage();
			int chan_id = 0;
			void *s = argv[i+2];
			void *buf;
			int len;
			sscanf(argv[i+1], "%d", &chan_id);
			event_v2_push(chan_id, s, strlen(s)+1, &buf, &len, 1500);
			i += 2;
		}
		if (!strcmp(argv[i], "-t")) {
			int n = 0;
			sscanf(argv[i+1], "%d", &n);
			test(n);
		}
	}

	return 0;
}

