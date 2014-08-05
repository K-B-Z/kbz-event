
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

#include "m.h"

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
	itemhdr_t *h;
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
	sb.size = size + sizeof(itemhdr_t);
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
		it->k = itemhdr(_c, it->h)->k;
		it->v = it->h + sizeof(itemhdr_t);
		switch (itemhdr(_c, it->h)->stat) {
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

	itemhdr_t *h = item(_c, it.h_empty);
	h->stat = USING;
	h->k = k;

	if (_v)
		*_v = it.h_empty + sizeof(itemhdr_t);
	if (_h)
		*_h = it.h_empty;

	return 0;
}

off_t dict_new(ctrl_t **_c) {
	return slab_new(_c, sizeof(dict_t));
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

static void ctrl_malloc(ctrl_t **_c, int pages_n) {
	int size = ctrl_size(pages_n);
	int id = shmget(key_magic, size, 0600|IPC_CREAT);

	ctrl_t *c = (ctrl_t *)shmat(id, NULL, 0);
	memset(c, 0, size);
	c->pages_n = pages_n;
	c->shm = id;

	*_c = c;
}

static void ctrl_realloc(ctrl_t **_c, int pages_n) {
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

const char *lock_name = "event_v2:lock";

static void ctrl_get(ctrl_t **_c) {
	sem_t *lock;
	
	lock = sem_open(lock_name, 0);
	if (lock == SEM_FAILED)
		lock = sem_open(lock_name, O_CREAT, 0600, 1);
	sem_wait(lock);

	int id = shmget(k, size, 0600);
	if (id < 0) {
		ctrl_malloc(_c, 64);
		return;
	}
	
	*_c = (ctrl_t *)shmat(id, NULL, 0);
}

static void ctrl_put(ctrl_t **_c) {
	ctrl_t *c = *_c;

	sem_t *lock = sem_open(lock_name, O_CREAT, 0600, 1);
	sem_post(lock);

	shmdt(c);
}

