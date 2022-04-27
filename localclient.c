#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
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

int main(){
    int cfd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un clientaddr;
    bzero(&clientaddr, sizeof(clientaddr));
    clientaddr.sun_family = AF_UNIX;
    strcpy(clientaddr.sun_path, "clientsock");
    int len = offsetof(struct sockaddr_un, sun_path) + strlen(clientaddr.sun_path);

    unlink("clientsock");
    bind(cfd, (struct sockaddr *)&clientaddr, (socklen_t)len);

    struct sockaddr_un serveraddr;
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sun_family = AF_LOCAL;
    strcpy(serveraddr.sun_path, "serversock");
    len = offsetof(struct sockaddr_un, sun_path) + strlen(serveraddr.sun_path);
    
    connect(cfd, (struct sockaddr *)&serveraddr, (socklen_t)len);

    char buf[1024];
    while(1){
        int ret = read(STDIN_FILENO, buf, sizeof(buf));
        if(ret < 0){
            perror("read error!\n");
            close(cfd);
            exit(1);
        }
        else if (ret == 0){
            printf("read end\n");
            close(cfd);
        }
        ret = write(cfd, buf, ret);
        if(ret < 0){
            perror("write cfd failed");
        }
        ret = read(cfd, buf, sizeof(buf));
        if(ret < 0){
            perror("read client error");
            close(cfd);
            exit(1);
        }
        else if (ret == 0){
            printf("read end\n");
            close(cfd);
        }
        write(STDOUT_FILENO, buf, ret);
    }
    close(cfd);

    return 0;
}