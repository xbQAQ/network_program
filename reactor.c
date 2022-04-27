#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define MAX_EVENTS 1024
#define BUFSIZE 4096
#define SERV_PORT 12345

void recvdata(int fd, int events, void* arg);
void senddata(int fd, int events, void* arg);

/*描述就绪文件符相关信息*/

struct myevent_s{
    int fd;                                                 //要监听的文件描述符
    int events;                                             //对应的监听事件
    void* arg;                                              //泛型参数
    void (*callback)(int fd, int events, void* arg);        //回调函数
    int status;                                             //是否在监听:1->在红黑树上(监听), 0->不在(不监听)
    char buf[BUFSIZE];
    int length; 
    long last_active;                                       //记录每次加入红黑树 g_efd 的时间值
};

int g_efd;                                                  //全局变量, 保存epoll_create返回的文件描述符
struct myevent_s g_events[MAX_EVENTS + 1];                  //自定义结构体类型数组. +1-->listen fd

/*将结构体 myevent_s 成员变量 初始化*/
void eventset(struct myevent_s* ev, int fd, void(*callback)(int, int, void*), void* arg){
    ev->fd = fd;
    ev->events = 0;
    ev->arg = arg;
    ev->callback = callback;
    ev->status = 0;
    memset(ev->buf, 0, sizeof(ev->buf));
    ev->length = 0;
    ev->last_active = time(NULL);

    return;
}

/* 向 epoll监听的红黑树 添加一个 文件描述符 */
//eventadd(efd, EPOLLIN, &g_events[MAX_EVENTS]);
void eventadd(int epfd, int events, struct myevent_s* ev){
    struct epoll_event event = {0, {0}};
    int operation;
    event.data.ptr = ev;
    event.events = ev->events = events;             //EPOLLIN 或 EPOLLOUT
    
    if(ev->status == 0){                            //如果不在红黑树 g_efd 里
        ev->status = 1;                             //将其加入红黑树 g_efd, 并将status置1
        operation = EPOLL_CTL_ADD;                  
    }

    if(epoll_ctl(epfd, operation, ev->fd, &event) < 0){         //实际添加/修改
        printf("event add failed [fd = %d], events[%d]\n", ev->fd, events);
    }
    else{
        printf("event add OK [fd=%d], operation = %d, events[%0X]\n", ev->fd, operation, events);
    }
    
    return;
}

/*从epoll监听的红黑树中删除一个文件描述符*/
void eventdel(int epfd, struct myevent_s* ev){
    struct epoll_event event = {0, {0}};
    
    if(ev->status != 1)         //不在红黑树上
        return;
    
    event.data.ptr = NULL;
    ev->status = 0;             //修改状态
    epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &event);         //从红黑树 efd 上将 ev->fd 摘除
}

/*  当有文件描述符就绪, epoll返回, 调用该函数 与客户端建立链接 */
void acceptconn(int lfd, int events, void* arg) {
    struct sockaddr_in cin;
    socklen_t len = sizeof(cin);
    int cfd, i;

    if((cfd = accept(lfd, (struct sockaddr*)&cin, &len)) == -1) {
        if(errno != EAGAIN && errno != EINTR){
            /*暂时不处理*/
        }
        printf("%s: accept, %s\n", __func__, strerror(errno));
        return;
    }

    do{
        for(i = 0; i != MAX_EVENTS; i++)        //从全局数组g_events中找一个空闲元素
            if(g_events[i].status == 0)         //类似于select中找值为-1的元素
                break;                          //跳出 for

        if(i == MAX_EVENTS){                    //跳出do while(0) 不执行后续代码
            printf("%s: max connect limit[%d]\n", __func__, MAX_EVENTS);
            break;
        }

        int flag = 0;
        if((flag = fcntl(cfd, F_SETFL, O_NONBLOCK)) < 0){       //将cfd也设置为非阻塞
            printf("%s: fcntl nonblocking failed, %s\n", __func__, strerror(errno));
            break;
        }

        /*给cfd设置一个myevent_s 结构体，回调函数设置为recvdata*/
        eventset(&g_events[i], cfd, recvdata, &g_events[i]);
        //将cfd添加到红黑树g_efd中,监听读事件
        eventadd(g_efd, EPOLLIN, &g_events[i]);
        
    }while(0);

    printf("new connect [%s: %d][time: %ld], pos[%d]\n", 
        inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), g_events[i].last_active, i);

    return;
}

void recvdata(int fd, int events, void* arg){
    struct myevent_s* ev = (struct myevent_s*)arg;
    int len;

    len = recv(fd, ev->buf, sizeof(ev->buf), 0);    //读文件描述符，数据存入myevent_s成员buf中

    eventdel(g_efd, ev);    //将该节点从红黑树上删除

    if(len > 0){
        ev->length = len;
        ev->buf[len] = '\0';            //手动添加字符串结束标记
        printf("C[%d]: %s\n", fd, ev->buf);

        eventset(ev, fd, senddata, ev);     //设置该 fd 对应的回调函数为 senddata
        eventadd(g_efd, EPOLLOUT, ev);      //将fd加入红黑树g_efd中,监听其写事件
    }
    else if(len == 0){
        close(ev->fd);
        /*ev - g_events 地址相减得到偏移元素位置*/
        printf("[fd = %d] pos[%ld], closed\n", fd, ev-g_events);
    }
    else{
        close(ev->fd);
        printf("recv[fd = %d] error[%d]: %s\n", fd, errno, strerror(errno));
    }

    return;
}

void senddata(int fd, int events, void* arg){
    struct myevent_s* ev = (struct myevent_s*)arg;
    int len;

    len = send(fd, ev->buf, ev->length, 0);                         //直接将数据 回写给客户端。未作处理

    eventdel(g_efd, ev);                                            //从红黑树g_efd中移除

    if(len > 0){
        printf("send[fd = %d], [%d]%s\n", fd, len, ev->buf);
        eventset(ev, fd, recvdata, ev);                             //将该fd的回调函数改为recvdata
        eventadd(g_efd, EPOLLIN, ev);                               //从新添加到红黑树上，设为监听事件
    }
    else{
        close(ev->fd);                                              //关闭链接
        printf("send[fd = %d] error %s\n", fd, strerror(errno));
    }

    return;
}

/*创建socket, 初始化lfd*/
void initlistensocket(int epfd, short port){
    struct sockaddr_in sin;

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int flags = fcntl(lfd, F_GETFL);            //将socket设为非阻塞
    flags |= O_NONBLOCK;
    fcntl(lfd, F_SETFL, flags);
    int flag = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;

    bind(lfd, (struct sockaddr *)&sin, sizeof(sin));
    listen(lfd, 20);

    /* void eventset(struct myevent_s *ev, int fd, void (*call_back)(int, int, void *), void *arg);  */
    eventset(&g_events[MAX_EVENTS], lfd, acceptconn, &g_events[MAX_EVENTS]);
    /* void eventadd(int efd, int events, struct myevent_s *ev) */
    eventadd(epfd, EPOLLIN, &g_events[MAX_EVENTS]);
    
    return;
}

int main(int argc, char **argv){
    unsigned short port = SERV_PORT;
    
    if(argc == 2){
        port = atoi(argv[1]);           //使用用户指定端口.如未指定,用默认端口
    }

    g_efd = epoll_create(MAX_EVENTS + 1);       //创建红黑树,返回给全局 g_efd 
    if(g_efd <= 0){
        printf("create efd in %s err %s\n", __func__, strerror(errno));
    }

    initlistensocket(g_efd, port);          //初始化监听socket

    struct epoll_event events[MAX_EVENTS + 1];      //保存已经满足就绪事件的文件描述符数组
    printf("server running: port[%d]\n", port);

    int checkpos = 0, i;
    while(1){
        /*超时验证，每次测试100个链接，不测试listened。当客户端60秒没有和服务器通信，则关闭此客户端链接*/

        long now = time(NULL);
        for(i = 0; i != 100; i++){
            if(checkpos == MAX_EVENTS)      //一次循环检查测100个。使用checkpos控制检测对象
                checkpos = 0;
            if(g_events[checkpos].status != 1)      //不在红黑树上
                continue;
            
            long duration = now - g_events[checkpos].last_active;           //客户端不活跃的时间
            if(duration >= 60){
                close(g_events[checkpos].fd);                               //关闭与该客户端链接
                printf("[fd = %d] timeout\n", g_events[checkpos].fd);
                eventdel(g_efd, &g_events[checkpos]);                       //将该客户端 从红黑树 g_efd移除
            }
        }

        /*监听红黑树g_efd, 将满足事件的文件描述符加到events数组中, 1秒没有事件满足，返回 0*/
        int nfd = epoll_wait(g_efd, events, MAX_EVENTS + 1, 1000);
        if(nfd < 0){
            printf("epoll_wait error, exit\n");
            break;
        }

        for(i = 0; i < nfd; i++){
            /*使用自定义结构体myevent_s类型指针，接受联合体data的void* ptr成员*/
            struct myevent_s* ev = (struct myevent_s*)events[i].data.ptr;

            if((events[i].events & EPOLLIN) && (ev->events & EPOLLIN)){         //读就绪事件
                ev->callback(ev->fd, events[i].events, ev->arg);
            }
            if((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT)){       //写就绪事件
                ev->callback(ev->fd, events[i].events, ev->arg);
            }
        }
    }
    return 0;
}