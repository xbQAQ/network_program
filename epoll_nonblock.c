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

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, int enable_et){
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if(enable_et){
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void lt(struct epoll_event* events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for(int i = 0; i < number; i++){
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd){
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            addfd(epollfd, connfd, 0);
        }
        else if(events[i].events & EPOLLIN){
            printf("event trigger once\n");
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE -1, 0);
            if(ret <= 0){
                close(sockfd);
                continue;
            }
            printf("get %d bytes of content: %s\n", ret, buf);
        }
        else{
            printf("something else happened\n");
        }
    }
}

void et(struct epoll_event* events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for(int i = 0; i < number; i++){
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd){
            struct sockaddr_in client_address;
            socklen_t client_addrlen = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlen);
            addfd(epollfd, connfd, 1);
        }
        else if(events[i].events == EPOLLIN){
            printf("event trigger once\n");
            while(1){
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
                if(ret < 0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        printf("read later\n");
                        break;
                    }
                    close(sockfd);
                    break;
                }
                else if(ret == 0){
                    close(sockfd);
                }
                else{
                    printf("get %d bytes of content: %s\n", ret, buf);
                }
            }
        }
        else{
            printf("something else happened\n");
        }
    }
}
int main(){
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

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenFd, 1);

    while(1){
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(ret < 0){
            printf("event failed\n");
            break;
        }
        lt(events, ret, epollfd, listenFd);
        // et(events, ret, epollfd, listenFd);
    }
    close(listenFd);
    return 0;
}