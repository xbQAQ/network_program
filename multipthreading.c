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

/*
*   多线程服务器
*/

#define BUFSIZE 4096

void* thread_client(void* arg){
    char buf[BUFSIZE];
    int clientFd = (void*)arg;
    while(1){
        int ret = recv(clientFd, buf, BUFSIZE - 1, 0);
        if(ret < 0){
            printf("recv error: %s\n", strerror(errno));
            pthread_exit((void*)1);
        }
        if(ret == 0){
            break;
        }
        for(int i = 0; i != ret; i++){
            buf[i] = toupper(buf[i]);
        }
        ret = send(clientFd, buf, ret, 0);
        if(ret < 0){
            printf("recv error: %s\n", strerror(errno));
            pthread_exit((void*)1);
        }
    }
    close(clientFd);
    return NULL;
}

int main(int argc, char **argv){
    if(argc > 1){
        printf("Arguments greater than 2\n");
        exit(1);
    }
    int port = 12345;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverFd < 0) {
        printf("socket error!, error code: %s\n", strerror(errno));
        exit(1);
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt));

    int ret = bind(serverFd, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0){
        printf("bind error, error code: %s\n", strerror(errno));
        exit(1);
    }

    ret = listen(serverFd, 1024);
    if(ret < 0){
        printf("listen error, error code: %s\n", strerror(errno));
        exit(1);
    }    

    while(1){
        struct sockaddr_in client;
        int clientFd;
        int client_length = sizeof(client);
        
        clientFd = accept(serverFd, (struct sockaddr*)&client, &client_length);
        if(ret < 0){
            printf("accept error, error code: %s\n", strerror(errno));
            exit(1);
        }

        pthread_t client_tid;
        pthread_attr_t attr;
        ret = pthread_attr_init(&attr);     //初始化属性
        if(ret != 0) {
            printf("pthread_attr_init failed: %s\n", strerror(errno));
            pthread_exit((void*)1);
        }
        ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);  //设置分离状态
        if(ret != 0) {
            printf("pthread_attr_setdetach error: %s\n", strerror(errno));
            pthread_exit((void*)1);
        }
        ret = pthread_create(&client_tid, &attr, thread_client, (void*)clientFd);       //创建线程
        if(ret != 0) {
            printf("pthread_create failed: %s\n", strerror(errno));
            pthread_exit((void*)1);
        }
        char ip[1024];
        printf("pthread %ld: ip: %s, port: %d connected\n", client_tid, 
            inet_ntop(AF_INET, (struct sockaddr *)&client.sin_addr.s_addr, ip, sizeof(ip)),
            client.sin_port);

        ret = pthread_attr_destroy(&attr);      //摧毁属性
        if(ret != 0) {
            printf("pthread_attr_destroy failed: %s\n", strerror(errno));
            pthread_exit((void*)1);
        }
    }

    close(serverFd);
    pthread_exit((void*)0);
    return 0;
}