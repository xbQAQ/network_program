#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <event2/event.h>

void eventread(evutil_socket_t fd, short what, void* arg){
    char buf[1024];
    int ret = read(fd, buf, sizeof(buf));
    printf("what: %s\t", what & EV_READ ? "YES" : "NO");
    write(STDOUT_FILENO, buf, ret);
    printf("\n");
    return ;
}

int main(){
    unlink("myfifo");
    mkfifo("myfifo", 0644);
    int fd = open("myfifo", O_RDONLY | O_NONBLOCK);
    if(fd < 0){
        perror("open error");
        exit(1);
    }
    struct event_base* base = event_base_new();
    
    struct event* event = event_new(base, fd, EV_READ | EV_PERSIST, eventread, NULL);
    event_add(event, NULL);
    
    //循环
    event_base_dispatch(base);

    event_base_free(base);
    event_free(event);
    close(fd);;
    return 0;
}
