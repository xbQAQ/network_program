#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <event2/event.h>

void eventwrite(evutil_socket_t fd, short what, void* arg){
    char buf[1024];
    static int i = 0;
    sprintf(buf, "hello-%d", i++);
    int ret = write(fd, buf, strlen(buf));
    printf("what: %s\t", what & EV_WRITE ? "YES" : "NO");
    write(STDOUT_FILENO, buf, ret);
    printf("\n");
    sleep(1);
    return;
}

int main(){
    int fd = open("myfifo", O_WRONLY | O_NONBLOCK);
    if(fd < 0){
        perror("open error");
        exit(1);
    }
    struct event_base* base = event_base_new();
    
    struct event* event = event_new(base, fd, EV_WRITE | EV_PERSIST, eventwrite, NULL);
    event_add(event, NULL);
    
    //循环
    event_base_dispatch(base);

    event_base_free(base);
    event_free(event);
    close(fd);;
    return 0;
}
