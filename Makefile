
all:
	$(CC) -shared -fvisibility=hidden -fPIC -o kbz-event.so kbz-event.c test.c util.c -pthread -lrt 

clean:
	rm -rf *.o *.so

