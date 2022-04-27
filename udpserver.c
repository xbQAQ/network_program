#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

int main(){
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    char buf[1024];
    char peer_addr[1024];
    struct sockaddr_in peer;
    while(1){
        socklen_t peer_length = sizeof(peer);
        memset(buf, 0, 1024);
        int ret = recvfrom(fd, buf, 1024, 0, (struct sockaddr*)&peer, &peer_length);
        printf("received from %s: %d\n", inet_ntop(AF_INET, &peer.sin_addr, peer_addr, sizeof(peer_addr)), ntohs(peer.sin_port));

        if(ret < 0){
            printf("recvfrom failed\n");
            close(fd);
        }
        else if(ret == 0){
            printf("peer closed\n");
            close(fd);
        }
        else{
            write(STDOUT_FILENO, buf, strlen(buf));
            for(int i = 0; i != ret; i++){
                buf[i] = toupper(buf[i]);
            }
            ret = sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&peer, peer_length);
            if(ret < 0){
                perror("send error");
            }
        }
    }
    close(fd);

    return 0;
}