kbz-event
=========

Send and recv event between processes/threads in very few lines of codes.

  * Unlimited buffer size 
  * WITHOUT any initialization or daemon process.
  * Fast and simple

Usage:

server.c

    #include <kbz-event.h>
    
    void main() {
      
      for (;;) {
        char *buf;
        int len;
        
        // wait event from channel 123. timeout 1s.
        if (kbz_event_get(123, &buf, &len, 1000) == 0) {
         // if not timeout
         // buffer is available until next kbz_event_get call.
         printf("got: %s\n", buf);
        }
      }
      
    }

client.c

    #include <kbz-event.h>
    
    void main() {
      // post event to channel 123.
      kbz_event_post(123, "hello", 6);
    }

Usage2:

server.c

    #include <kbz-event.h>
    
    void main() {
      
      for (;;) {
        char *buf;
        int len;
        
        // get event from channel 123. wait forever(timeout=0).
        kbz_event_get(123, &buf, &len, 0);
        
        char ans[128];
        sprintf(ans, "time now is %d", time(NULL));
        kbz_ack_event(123, buf, ans, strlen(ans)+1);
      }
      
    }
  
client.c

    #include <kbz-event.h>
    
    void main() {
      char *ans;
      int ans_len;
      
      // post an empty event to channel 123 and wait for answer.
      kbz_event_push(123, "", 0, &ans, &ans_len, 0);
      
      printf("server says: %s\n", ans);
    }


Build Requirements:

    gcc version > 4.6
    Linux
    

