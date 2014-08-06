
#include <unistd.h>  
#include <fcntl.h>  
#include <stdlib.h>  
#include <stdio.h>  
#include <string.h>  
#include <semaphore.h>  
#include <sys/shm.h>  
#include <sys/mman.h>  
#include <sys/stat.h>  
#include <sys/ipc.h>  
#include <sys/sem.h>  
#include <sys/types.h>  

#include "utils.h"

off_t malloc_page(ctrl_t **_c) {
	ctrl_t *c = *_c;
	int n = c->pages_n;
	int i;

	for (i = 0; i < n; i++) {
		off_t _p = sizeof(ctrl_t) + i*PAGESIZE;
		page_t *p = page(_c, _p);
		if (p->stat == UNUSED || p->stat == FREED) {
			p->stat = USING;
			return _p;
		}
	}

	ctrl_realloc(_c, n*2);
	page(_c, n)->stat = USING;
	return sizeof(ctrl_t) + n*PAGESIZE;
}

iter_t slab_iter(ctrl_t **_c, off_t _sb) {
	iter_t it = {};
	it.page = slab(_c, _sb)->page;
	return it;
}

void slab_del(ctrl_t **_c, off_t _sb, int k) {
	item_t *h;
	if (slab_get(_c, _sb, k, NULL, &h))
		h->stat = FREED;
}

void slab_free(ctrl_t **_c, off_t _sb) {
	slab_t *sb = slab(_c, _sb);
	off_t _p = sb->page;
	while (_p) {
		page_t *p = page(_c, _p);
		memset(p->buf, 0, PAGESIZE-sizeof(page_t));
		p->stat = FREED;
		_p = p->next;
	}
	sb->page = 0;
}

slab_t slab_new(ctrl_t **_c, int size) {
	slab_t sb = {};
	sb.size = size + sizeof(item_t);
	sb.page = malloc_page(_c);
	return sb;
}

int iter_next(ctrl_t **_c, iter_t *it) {
	for (;;) {
		if (!it->page)
			return 0;
		if (it->i == slab_item_nr(_c, it->sb)) {
			int next = page(_c, it->page)->next;
			if (!next)
				return 0;
			it->page = next;
		}
		it->h = slab_item(_c, it->sb, it->page, it->i);
		it->k = item(_c, it->h)->k;
		it->v = it->h + sizeof(item_t);
		switch (item(_c, it->h)->stat) {
		case FREED:
			it->h_empty = it->h;
			break;
		case UNSED:
			return 0;
		case USING:
			return 1;
		}
		it->i++;
	}
}

int slab_get(ctrl_t **_c, off_t _sb, int k, off_t *_v, off_t *_h) {
	iter_t it = slab_iter(_c, _sb);

	while (iter_next(_c, &it)) {
		if (ik == it->k) {
			if (_v)
				*_v = it->v;
			if (_h)
				*_h = it->h;
			return 1;
		}
	}

	return 0;
}

int slab_get_or_new(ctrl_t **_c, off_t _sb, int k, off_t *_v, off_t *_h) {
	iter_t it = slab_iter(_c, _sb);

	while (iter_next(_c, &it)) {
		if (ik == it->k) {
			if (_v)
				*_v = it->v;
			if (_h)
				*_h = it->h;
			return 1;
		}
	}

	if (!it.h_empty) {
		it.page = malloc_page(_c);
		it.h_empty = it.page + sizeof(page_t);
	}

	item_t *h = item(_c, it.h_empty);
	h->stat = USING;
	h->k = k;

	if (_v)
		*_v = it.h_empty + sizeof(item_t);
	if (_h)
		*_h = it.h_empty;

	return 0;
}

off_t dict_new(ctrl_t **_c) {
	return slab_new(_c, sizeof(dict_t));
}

int dict_get(ctrl_t **_c, off_t d, int k, off_t *_v) {
	return slab_get(_c, dict(_c, d)->sb, k, _v, NULL);
}

int dict_get_or_new(ctrl_t **_c, off_t d, int k, off_t *_v) {
	return slab_get_or_new(_c, dict(_c, d)->sb, k, _v, NULL);
}

void dict_del(ctrl_t **_c, off_t d, int k) {
	slab_del(_c, dict(_c, d)->sb, k);
}

iter_t dict_iter(ctrl_t **_c, off_t d) {
	return slab_iter(_c, dict(_c, d)->sb);
}

int dict_len(ctrl_t **_c, off_t d) {
	iter_t it = dict_iter(_c, d);
	int n = 0;
	while (iter_next(_c, &it))
		n++;
	return n;
}

#define assert(x) _assert(__file__, __line__, #x, x)

static void _assert(char *file, int line, char *s_cond, int cond) {
	if (!cond) {
		fprintf(stderr, "assert failed at: %s:%d %s\n", file, line, s_cond);
		exit(-1);
	}
}

static int ctrl_size(int pages_n) {
	return sizeof(ctrl_t)+pages_n*PAGESIZE;
}

void ctrl_malloc(ctrl_t **_c, int pages_n) {
	int size = ctrl_size(pages_n);
	int id = shmget(key_magic, size, 0777|IPC_CREAT);

	ctrl_t *c = (ctrl_t *)shmat(id, NULL, 0);
	memset(c, 0, size);
	c->pages_n = pages_n;
	c->shm = id;

	*_c = c;
}

void ctrl_init(ctrl_t **_c) {
	ctrl(_c)->chans = dict_new();
}

void ctrl_realloc(ctrl_t **_c, int pages_n) {
	ctrl_t *c = *_c;

	int size = ctrl_size(c->pages_n);
	void *tmp = malloc(size);
	memcpy(tmp, c, size);

	int id = c->shm;
	shmdt(c);
	shmctl(id, 0, IPC_RMID, 0);

	ctrl_malloc(_c, pages_n);
	c = *_c;
	memcpy(c, tmp, size);
	free(tmp);

	c->pages_n = pages_n;
}

static const char *lock_name = "event_v2:lock";

void ctrl_get(ctrl_t **_c) {
	sem_t *lock;
	
	lock = sem_open(lock_name, 0);
	if (lock == SEM_FAILED)
		lock = sem_open(lock_name, O_CREAT, 0777, 1);
	sem_wait(lock);

	int id = shmget(k, size, 0777);
	if (id < 0) {
		ctrl_malloc(_c, 64);
		ctrl_init(_c);
		return;
	}
	
	*_c = (ctrl_t *)shmat(id, NULL, 0);
}

void ctrl_put(ctrl_t **_c) {
	ctrl_t *c = *_c;

	sem_t *lock = sem_open(lock_name, O_CREAT, 0777, 1);
	sem_post(lock);

	shmdt(c);
}

int isem_new() {
	for (;;) {
		int k = 0x999888 + (rand()%0xfffff);
		char name[128];

		sprintf(name, "event_v2:sem:%d", k);
		sem_t *s = sem_open(name, O_CREAT|O_EXCL, 0777, 1);
		if (s != SEM_FAILED) {
			sem_close(s);
			return k;
		}
	}
}

void ishm_del(int id) {
	semctl(id, 0, IPC_RMID, 0);
}

int ishm_new(int size) {
	int id = shmget(k, size, 0777);
}

int ishm_size(int id) {
	struct shmid_ds ds;
	shmctl(id, IPC_STAT, &ds);
	return ds.shm_segsz;
}

void *ishm_map(int id) {
	return shmat(id, NULL, 0);
}

void ishm_unmap(void *addr) {
	shmdt(addr);
}

int ishm_clone(int id) {
	int size = ishm_size(id);
	void *addr = ishm_map(id);
	int id2 = ishm_new(size);
	void *addr2 = ishm_map(id2);

	memcpy(addr2, addr, size);
	ishm_unmap(addr);
	ishm_unmap(addr2);

	return id2;
}

void ishm_del(int id) {
	shmctl(id, 0, IPC_RMID, 0);
}

int ishm_new_from_buf(void *buf, int size) {
	int id = ishm_new(size);
	void *addr = ishm_map(id);
	memcpy(addr, buf, size);
	ishm_unmap(id);
	return id;
}

void ishm_get_bufsize(int id, void **buf, int *size) {
}

