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

void read_cb(struct bufferevent *bev, void *ctx){
    char msg[4096];
 
    size_t len = bufferevent_read(bev, msg, sizeof(msg)-1 );
 
    msg[len] = '\0';
    printf("server read the data %s\n", msg);
 
    char reply[] = "I has read your data\n";
    bufferevent_write(bev, reply, strlen(reply) );

    sleep(1);
}

void write_cb(struct bufferevent *bev, void *ctx){
    printf("已经写给客户端, 写缓冲区函数被回调\n");
}

void event_cb(struct bufferevent *bev, short what, void *ctx){
    if(what & BEV_EVENT_EOF){
        printf("connection closed\n");
    }
    else if(what & BEV_EVENT_ERROR){
        printf("some error occurred\n");
    }
    bufferevent_free(bev);
    printf("bufferevent 资源已释放");
}

void listener_cb(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* addr, int socklen, void* arg){
    printf("client connected\n");
    struct event_base* base = (struct event_base*)arg;
    //添加bufferevent事件，并把监听到之后得到的fd（也就是connfd）绑定在事件上
    struct bufferevent* bfevent = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bfevent) {
        fprintf(stderr, "Error constructing bufferevent!");
        event_base_loopbreak(base);
        return;
    }
    bufferevent_setcb(bfevent, read_cb, write_cb, event_cb, NULL);
    bufferevent_enable(bfevent, EV_READ | EV_WRITE | EV_PERSIST);       //默认读缓存时关闭的
}

int main(){
    struct event_base* base = event_base_new();
    if (!base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}

    struct sockaddr_in addr;
    addr.sin_port = htons(12345);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int socklen = sizeof(addr);

    struct evconnlistener* listener = 
        evconnlistener_new_bind(base, listener_cb, (void*)base, LEV_OPT_THREADSAFE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, 20, (struct sockaddr*)&addr, socklen);
    
    if (!listener) {
        fprintf(stderr, "Could not create a listener!\n");
        return 1;
    }

    event_base_dispatch(base);
    
    evconnlistener_free(listener);
    event_base_free(base);
    return 0;
}
