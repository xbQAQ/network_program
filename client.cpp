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

#define BUF_SIZE 1024
int main(int argc, char* argv[])
{
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    int count = 6;

    char* ip = argv[1];
    int port = atoi(argv[2]);
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    // server.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_pton(AF_INET, "172.28.62.55", &server.sin_addr.s_addr);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    //服务器的ip和地址
    int ret = connect(fd, (sockaddr*)&server, sizeof(server));
    assert(ret == 0);
    
    while(count--){
        memcpy(buf, "hello", 6);
        send(fd, buf, BUF_SIZE - 1, 0);
        recv(fd, buf, BUF_SIZE - 1, 0);
        printf("%s\n", buf);
        sleep(1);
    }
    close(fd);

    return 0;
}