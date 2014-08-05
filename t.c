#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include <sys/syscall.h> /*此头必须带上*/

pid_t gettid()
{
     return syscall(SYS_gettid);  /*这才是内涵*/
		 }


void *proc(void *_) {
	printf("thread %d:%d\n", getpid(), gettid());
}

int main() {

	int id;

	printf("main %d:%d\n", getpid(), gettid());
	
	pthread_t th;

	pthread_create(&th, NULL, proc, NULL);
	pthread_join(th, &id);

	pthread_create(&th, NULL, proc, NULL);
	pthread_join(th, &id);


	return 0;
}

