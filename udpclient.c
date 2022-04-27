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
    struct sockaddr_in peer;
    peer.sin_family = AF_INET;
    peer.sin_port = htons(12345);
    inet_pton(AF_INET, "172.28.62.55", &peer.sin_addr.s_addr);
    socklen_t peer_length = sizeof(peer);

    char buf[1024];

    while(fgets(buf, 1024, stdin) != NULL) {
        int n = sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&peer, peer_length);
        if(n < 0) {
            perror("sendto error!\n");
        }
        n = recvfrom(fd, buf, strlen(buf), 0, (struct sockaddr *)&peer, peer_length);
        if(n < 0) {
            perror("recvfrom error!\n");
        }
        write(STDOUT_FILENO, buf, strlen(buf));
    }
    close(fd);

    return 0;
}