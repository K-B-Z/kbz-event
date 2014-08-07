kbz-event
=========

Send and recv buffers between processes in very few lines of codes.
WITHOUT any initialization or daemon process.

Usage:

server.c

    #include <kbz-event.h>
    
    void main() {
      
      for (;;) {
        char *buf;
        int len;
        
        // get event from channel 123. timeout 1s.
        kbz_event_get(123, 1000, &buf, &len);
        printf("got: %s\n", buf);
      }
      
    }

client.c

    #include <kbz-event.h>
    
    void main() {
      kbz_event_post(123, "hello", 6);
    }


Build Requirements:

    gcc version > 4.6
    

