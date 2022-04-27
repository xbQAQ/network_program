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

int main(){
    char buf[BUFSIZ];
    int port = 12345;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd < 0){
        perror("socket error!\n");
        exit(1);
    }
    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    int ret = bind(listenFd, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0){
        perror("bind error!\n");
        exit(1);
    }
    ret = listen(listenFd, 1024);
    if(ret < 0){
        perror("listen error!\n");
        exit(1);;
    }
    struct epoll_event event, events[4096];
    event.data.fd = listenFd;   //监听文件描述符为listenFd
    event.events = EPOLLIN;     //监听时间为读事件
    int epfd = epoll_create(4096);  //创造一个epoll红黑树，树的节点个数为4096，受内核影响
    if(epfd < 0) {
        perror("epoll_create error!\n");
        exit(1);
    }
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, listenFd, &event);     //将listenFd加到epoll红黑树上
    if(ret < 0) {
        perror("epoll_ctl error!\n");
        exit(1);;
    }

    while(1) {
        int readynumber = epoll_wait(epfd, events, 4096, -1);       //阻塞监听，当有事件发生时，通过events数组传出来，readynumber为发生事件的个数
        if(readynumber < 0) {
            printf("epoll_wait error\n");
            exit(1);
        }
        struct sockaddr_in client;
        int clientFd;
        int client_length = sizeof(client);
        for(int i = 0; i != readynumber; i++) {
            if(events[i].events & EPOLLIN){ //如果监听事件为读事件
                if(events[i].data.fd == listenFd){  //如果有客户端连接
                    clientFd = accept(listenFd, (struct sockaddr *)&client, &client_length);
                    char address[1024];
                    int address_len = sizeof(address);
                    printf("client address: %s, port is %d\n", inet_ntop(AF_INET, &client.sin_addr.s_addr, address, address_len), ntohs(client.sin_port));
                    if(ret < 0) {
                        perror("accept error!\n");
                        exit(1);
                    }
                    event.events = EPOLLIN; //将连接的客户端的监听事件设为读事件
                    event.data.fd = clientFd;   //监听的文件描述符为连接的客户端的文件描述符clientFd
                    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, clientFd, &event); //将连接的clientFd加到epoll红黑树上
                    if(ret < 0){
                        perror("epoll_ctl error!\n");
                        exit(1);
                    }
                }
                else{   //不是连接事件，那就是有数据传输
                    int fd = events[i].data.fd;
                    int recvnum = recv(fd, buf, BUFSIZ, 0);
                    if(recvnum < 0){
                        if(errno == EINTR){
                            continue;
                        }
                        else{
                            perror("recv error\n");
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                            close(fd);
                        }
                    }
                    else if(recvnum == 0){
                        printf("recv end!\n");
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                    }
                    else{
                        for(int j = 0; j != recvnum; j++){
                            buf[j] = toupper(buf[j]);
                        }
                        send(fd, buf, recvnum, 0);
                    }
                }
            }
        }
        
    }
    
    return 0;
}