#pragma once

#include <stdint.h>

enum {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR,
};

#define log(fmt, args...) _log(LOG_DEBUG, __func__, __FILE__, __LINE__, fmt, ##args) 
#define info(fmt, args...) _log(LOG_INFO, __func__, __FILE__, __LINE__, fmt, ##args) 
#define warn(fmt, args...) _log(LOG_WARN, __func__, __FILE__, __LINE__, fmt, ##args) 
#define error(fmt, args...) _log(LOG_ERROR, __func__, __FILE__, __LINE__, fmt, ##args) 

void _log(int level, const char *, const char *, int, char *, ...);
void log_ban(const char *, const char *);
void log_set_level(int level);
void log_init();

float now();

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

#define SHMKEY_MAX (POSTS_NR*CHANS_NR*128)

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
	pthread_mutex_t lock;
} ctrl_t;

ctrl_t *ctrl_get();
void ctrl_put(ctrl_t *c);

