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
#include <sys/select.h>

#define BUFSIZE 1024

int main(){
    int port = 12345;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenFd >= 0);
    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));   //设置io复用
    int ret = bind(listenFd, (struct sockaddr *)&addr, sizeof(addr));
    assert(ret != -1);
    ret = listen(listenFd, 1024);
    assert(ret != -1);

    int maxFd = listenFd;
    fd_set rset, allset;
    FD_ZERO(&allset);
    FD_SET(listenFd, &allset);

    char buf[BUFSIZE];
    int clientnum[FD_SETSIZE];
    int maxi = -1;
    for(int i = 0; i < FD_SETSIZE; i++){
        clientnum[i] = -1;
    }
    int connectFd;
    struct sockaddr_in client;
    int client_length = sizeof(client);
    while(1){
        rset = allset;
        int max = select(maxFd + 1, &rset, NULL, NULL, NULL);
        if(max < 0){
            printf("select failed: %s\n", strerror(errno));
            exit(1);
        }
        if(FD_ISSET(listenFd, &rset)){
            connectFd = accept(listenFd, (struct sockaddr *)&client, &client_length);
            if(connectFd < 0){
                printf("Failed to connect: %s\n", strerror(errno));
                exit(1);
            }
            FD_SET(connectFd, &allset);
            
            int i;
            for(i = 0; i != FD_SETSIZE; i++){
                if(clientnum[i] == -1){
                    clientnum[i] = connectFd;
                    break;
                }
            }

            if(i == FD_SETSIZE){ 
                fputs("too many sockets\n", stderr);
                exit(1);
            }

            if(i > maxi){
                maxi = i;
            }

            if(maxFd < connectFd){
                maxFd = connectFd;
            }

            if(--max == 0){
                continue;
            }
        }

        for(int i = 0; i != maxi + 1; i++){
            int sockfd;
            if((sockfd = clientnum[i]) == -1){
                continue;
            }
            if(FD_ISSET(sockfd, &rset)){
                int length = recv(sockfd, buf, BUFSIZE - 1, 0);
                if(length < 0){
                    if(errno == EINTR){
                        continue;
                    }
                    perror("recv error");
                }
                else if(length == 0){
                    close(sockfd);
                    clientnum[i] = -1;
                    FD_CLR(sockfd, &allset);
                }
                else{
                    for(int j = 0; j != length; j++){
                        buf[j] = toupper(buf[j]);
                    }
                    write(sockfd, buf, length);
                }
            }
            else{
                continue;
            }
            if(--max == 0){
                break;
            }
        }

    }
    
    return 0;
}