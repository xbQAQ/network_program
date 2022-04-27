#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <memory.h>
#include <arpa/inet.h>

void read_cb(struct bufferevent *bev, void *ctx){
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    bufferevent_read(bev, buf, sizeof(buf));
    printf("say:%s\n", buf);
    //bufferevent_write(bev, buf, strlen(buf) +1);
    sleep(1);
}

void write_cb(struct bufferevent *bev, void *ctx){
    printf("已经写给服务器, 写缓冲区函数被回调\n");
}

void event_cb(struct bufferevent *bev, short what, void *ctx){
    if(what & BEV_EVENT_EOF){
        printf("connection closed\n");
    }
    else if(what & BEV_EVENT_ERROR){
        printf("some error occurred\n");
    }
    else if(what & BEV_EVENT_CONNECTED){        //当客户端连接到服务器，BEV_EVENT_CONNECTED会被设置
        printf("client connected\n");
        return;
    }
    bufferevent_free(bev);
    printf("bufferevent 资源已释放");
}

void read_terminal(evutil_socket_t fd, short what, void *arg){
    struct bufferevent* buffevent = (struct bufferevent*)arg;
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    int ret = read(fd, buf, sizeof(buf));
    if(ret < 0){
        perror("read error\n");
        exit(1);
    }
    else if(ret == 0){
        printf("read end\n");
    }
    else{
        bufferevent_write(buffevent, buf, ret + 1); 
    }
    
}

int main(int argc, char** argv){
    struct event_base* base = event_base_new();

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    inet_pton(AF_INET, "172.28.62.55", &addr.sin_addr.s_addr);
    struct bufferevent* buffevent = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_socket_connect(buffevent, (struct sockaddr *)&addr, sizeof(addr));
    bufferevent_setcb(buffevent, read_cb, write_cb, event_cb, NULL);
    bufferevent_enable(buffevent, EV_READ | EV_WRITE);

    struct event* event = event_new(base, STDIN_FILENO, EV_READ | EV_PERSIST, read_terminal, buffevent);
    event_add(event, NULL);

    event_base_dispatch(base);
    
    bufferevent_free(buffevent);
    event_base_free(base);
    return 0;
}