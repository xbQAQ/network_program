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
    // if(argc <= 2){
	// 	printf("usage:%s ip_address port_number backlog\n", basename(argv[0]));
	// 	return 1;
	// }

    const char* ip = argv[1];
    int port = atoi(argv[2]);
    char client_ip[1024];

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    //设置sockaddr_in结构体
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    //addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_pton(AF_INET, ip, &addr.sin_addr.s_addr);
    
    int ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    if(ret == -1){
        printf("%d", errno);
    }
    
    ret = listen(fd, 1024);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t client_length = sizeof(addr);
    int serverfd = accept(fd, (sockaddr*)&client, &client_length);
    printf("client's ip = %s, port is %d\n", 
        inet_ntop(AF_INET, &client.sin_addr.s_addr, client_ip, sizeof(client_ip)),
        client.sin_port);

    sockaddr_in serv, guest;
    char serv_ip[20], guest_ip[20];
    socklen_t serv_len = sizeof(serv), guest_len = sizeof(guest);
    getsockname(serverfd, (sockaddr*)&serv, &serv_len);
    getpeername(serverfd, (sockaddr*)&guest, &guest_len);
    inet_ntop(AF_INET, &serv.sin_addr.s_addr, serv_ip, serv_len);
    inet_ntop(AF_INET, &guest.sin_addr.s_addr, guest_ip, guest_len);
    printf("service ip: %s, port: %d", serv_ip, ntohs(serv.sin_port));
    printf("guest ip: %s, port: %d", guest_ip, ntohs(guest.sin_port));

    if(serverfd < 0){
        printf("errno is %d\n", strerror(errno));
    }
    else{
        char buf[BUF_SIZE];
        memset(buf, 0, BUF_SIZE);
        while(1){
            recv(serverfd, &buf, BUF_SIZE - 1, 0);
            printf("%s\n", buf);
            for(auto& i : buf){
                i = toupper(i);
            }
            send(serverfd, &buf, BUF_SIZE - 1, 0);
        }
    }
    close(fd);
    close(serverfd);

    return 0;
}