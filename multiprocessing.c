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

/*
*   多进程服务器
*/

#define BUFSIZE 4096

void waitchild(int signum){
    while(waitpid(-1, NULL, WNOHANG) > 0);
    return;
}

int main(int argc, char* argv[]){
    int clientFd, serverFd;
    int ret;
    int client_ip[1024];
    pid_t pid;

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    //socket
    //bind
    //listen
    //accept
    
    //设置连接属性
    struct sockaddr_in server;
    server.sin_family = AF_INET;    //IPv4
    server.sin_port = htons(port);  //主机序列转网络序列
    //inet_pton(AF_INET, ip, &server.sin_addr.s_addr);    //主机序列转网络序列
    server.sin_addr.s_addr = htonl(INADDR_ANY);   //设置监听的ip，INADDR_ANY为当前可用的任意ip

    serverFd = socket(AF_INET, SOCK_STREAM, 0); //获取socket套接字
    assert(serverFd >= 0);

    ret = bind(serverFd, (struct sockaddr*)&server, sizeof(server));
    assert(ret != -1);
    if(ret == -1){
        printf("%d", errno);
    }
    
    ret = listen(serverFd, 1024);   //设置监听上线为1024
    assert(ret != -1);

    struct sockaddr_in client;  //accept的传出参数，传出来的是连接的客户端的信息
    socklen_t client_length = sizeof(client);
    while(1){
        clientFd = accept(serverFd, (struct sockaddr*)&client, &client_length);
        //由于accept是慢系统调用，当阻塞于某个慢系统调用的一个进程捕获某个信号且相应处理函数返回时，
        //该系统调用可能返回一个EINTR错误。所以，我们必须对慢系统调用返回的EINTR有所准备。
        if(errno == EINTR) continue;    
        if(clientFd < 0){
            printf("errno is %s, pid is %d\n", strerror(errno), getpid() );
            exit(1);
        }
        
        printf("client's ip = %s, port is %d\n", 
            inet_ntop(AF_INET, &client.sin_addr.s_addr, client_ip, sizeof(client_ip)),
            client.sin_port);   //网络序列转换为主机序列，网络ip->主机ip，大端->小端，网络是大端，主机是小端
        pid = fork();
        if(pid < 0){
            perror("fork error!\n");
        }
        else if(pid == 0){      // 子进程
            close(serverFd);    // 关闭监听通道
            break;
        }
        else{   //父进程
            close(clientFd);
            struct sigaction act;
            act.sa_handler = waitchild; //设置捕捉函数
            ret = sigemptyset(&act.sa_mask);    //设置信号阻塞掩码，进而设置信号阻塞序列，来影响未决序列
            if(ret == -1){
                perror("sigemptyset error!\n");
                exit(1);
            }
            act.sa_flags = 0;   //默认
            ret = sigaction(SIGCHLD, &act, NULL); //注册信号
            if(ret < 0){
                perror("sigaction error!\n");
                exit(1);
            } 
        }
    }

    if(pid == 0){
        char buf[BUFSIZE];
        while(1){
            memset(buf, 0, BUFSIZE);
            //ret = recv(clientFd, buf, sizeof(buf) - 1, 0); //sizeof(buf)是错的，大小为4字节，为一个指针
            ret = recv(clientFd, buf, BUFSIZE - 1, 0);
            //printf("%s\n", buf);
            if(ret < 0){
                perror("recv");
                exit(1);
            }
            for(int i = 0; i != BUFSIZE; i++){
                buf[i] = toupper(buf[i]);
            }
            ////sizeof(buf)是错的，大小为4字节，为一个指针
            send(clientFd, buf, ret, 0);
        }
        close(clientFd);
    }

    return 0;
}