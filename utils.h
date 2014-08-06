#pragma once

// {}
// []

enum {
	UNUSED,
	FREED,
	USING,
};

#define PAGESIZE 2048

typedef struct {
	int stat;
	off_t next;
	char buf[0];
} page_t;

typedef struct {
	off_t page;
	int size;
} slab_t;

typedef struct {
	int stat;
	int k;
} item_t;

typedef struct {
	slab_t sb;
} dict_t;

typedef struct {
	off_t sb;

	off_t page;
	int i;

	int k;
	off_t v;

	off_t h;
	off_t h_empty;
} iter_t;

typedef struct {
	off_t chans;

	int shm;
	int pages_n;
	char buf[0];
} ctrl_t;

typedef struct {
	int shm;
} post_t;

typedef struct {
	off_t procs;
	off_t posts;
	int post_s, post_e;
} chan_t;

enum {
	NONE,
	GETTING,
	PUSHING,
	WAITING_ACK,
};

typedef struct {
	int pid, tid;
	int stat;
} proc_t;

static inline void *ctrl_ptr(ctrl_t **_c, off_t p) {
	return (void *)(*_c) + p;
}

static inline ctrl_t *ctrl(ctrl_t **_c) {
	return *c;
}

static inline slab_t *slab(ctrl_t **_c, off_t _sb) {
	return (slab_t *)ctrl_ptr(_c, _sb);
}

static inline dict_t *dict(ctrl_t **_c, off_t _d) {
	return (dict_t *)ctrl_ptr(_c, _sb);
}

static inline page_t *page(ctrl_t **_c, off_t _p) {
	return (page_t *)ctrl_ptr(_c, _p);
}

static inline itemhdr_t *item(ctrl_t **_c, off_t _h) {
	return (itemhdr_t *)ctrl_ptr(_c, _h);
}

static inline proc_t *proc(ctrl_t **_c, off_t _p) {
	return (proc_t *)ctrl_ptr(_c, _p);
}

static inline off_t slab_item(ctrl_t **_c, off_t _sb, off_t _p, int i) {
	return _p + sizeof(page_t) + i*slab(_c, _sb)->size;
}

static inline int slab_item_nr(ctrl_t **_c, off_t _sb) {
	return (PAGESIZE - sizeof(page_t)) / slab(_c, _sb)->size;
}

