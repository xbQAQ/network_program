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
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un serveraddr;
    serveraddr.sun_family = AF_UNIX;
    strcpy(serveraddr.sun_path, "serversock");
    int len = offsetof(struct sockaddr_un, sun_path) + strlen(serveraddr.sun_path);
    
    unlink("serversock");
    bind(fd, (struct sockaddr *)&serveraddr, (socklen_t)len);

    listen(fd, 10);
    struct sockaddr_un client;
    socklen_t client_length = sizeof(client);
    int cfd = accept(fd, (struct sockaddr *)&client, (socklen_t *)&client_length);
    client_length -= offsetof(struct sockaddr_un, sun_path);
    client.sun_path[client_length] = '\0';
    printf("client bind filename: %s\n", client.sun_path);

    char buf[1024];
    while(1){
        int ret = read(cfd, buf, sizeof(buf));
        printf("ret = %d\n", ret);
        if(ret < 0){
            perror("read error\n");
            close(cfd);
        }
        else if(ret == 0){
            close(cfd);
            exit(1);
        }
        else{
            for(int i = 0; i != ret; i++){
                buf[i] = toupper(buf[i]);
            }
            write(cfd, buf, ret);
            write(STDOUT_FILENO, buf, ret);
        }
    }
    close(cfd);
    close(fd);
    return 0;
}