
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

static void usage() {
	fprintf(stderr, 
		"  -i           reinit everything\n"
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

	switch (n) {
		case 0: {
			log("create shm shm.b");
			int fd = shm_open("shm.b", O_CREAT|O_RDWR, 0777);
			log("  fd=%d", fd);
			break;
		}

		case 1: {
			for (;;) {
				log("try open shm shm.a");
				int fd = shm_open("shm.a", O_RDWR, 0777);
				log("  fd=%d", fd);
				if (fd > 0)
					break;
				usleep(1e6*0.5);
			}
			break;
		}

		case 2: {
			log("test ctrl_get: put str");
			void *a = ctrl_get();
			sprintf(a, "hello: %d", (int)time(NULL));
			break;
		}

		case 3: {
			log("test ctrl_get: get str");
			void *a = ctrl_get();
			printf("%s\n", (char *)a);
			break;
		}

		case 4: {
			log("test mutex: try lock and sleep");
			ctrl_t *c = ctrl_get();
			sleep(1e6);
			break;
		}

		case 5: {
			log("test mutex: lock and exit");
			ctrl_t *c = ctrl_get();
			log("locked");
			break;
		}

		case 6: {
			log("test sem");
			sem_t *s = sem_open("kbz.123123", O_CREAT|O_EXCL, 0777, 0);
			log("sem: %p", s);
			break;
		}
	}
}

int main(int argc, char *argv[]) {
	if (argc == 1)
		usage();
	
	int i;

	for (i = 1; i < argc; i++) {

		if (!strcmp(argv[i], "-v")) {
			log_set_level(LOG_DEBUG);
		}

		if (!strcmp(argv[i], "-i")) {
			shm_unlink("shm.ctrl");
			ctrl_t *c = ctrl_get();
			ctrl_put(c);
			return;
		}

		if (!strcmp(argv[i], "-d")) {
			ctrl_t *c = ctrl_get();
			ctrl_dump(c);
			ctrl_put(c);
			return;
		}

		if (!strcmp(argv[i], "-g")) {
			if (i+1 >= argc)
				usage();
			int chan_id = 0;
			void *buf;
			int len;
			sscanf(argv[i+1], "%d", &chan_id);

			for (;;) {
				event_v2_get(chan_id, 0, &buf, &len);
				info("[got] buf=%s len=%d", buf, len);
				event_v2_ack(buf, "hello", 6);
			}
		}

		if (!strcmp(argv[i], "-p")) {
			if (i+2 >= argc)
				usage();
			int chan_id = 0;
			void *buf;
			int len;
			sscanf(argv[i+1], "%d", &chan_id);
			char *s = argv[i+2];
			event_v2_post(chan_id, s, strlen(s)+1);
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
			info("[got] buf=%s len=%d", buf, len);
			i += 2;
		}

		if (!strcmp(argv[i], "-t")) {
			if (i+1 >= argc)
				usage();
			int n = 0;
			sscanf(argv[i+1], "%d", &n);
			test(n);
		}

	}

	return 0;
}

