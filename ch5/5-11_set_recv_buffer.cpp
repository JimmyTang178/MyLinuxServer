#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#define BUF_SIZE 1024

int main(int argc, char* argv[]){

    if (argc <= 3){
        printf("usage: %s ip port recv_buffer_size\n", argv[0]);
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    int recvbuf = atoi(argv[3]);
    int len = sizeof(recvbuf);
    /*先设置TCP接收缓冲区大小，然后立即读取*/
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof(recvbuf));
    getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recvbuf, (socklen_t*)&len);
    printf("the TCP receive buffer size after setting is %d \n", recvbuf);

    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int confd = accept(sock, (struct sockaddr*)&client, &client_len);
    if (confd < 0){
        printf("errno is %d \n", errno);
    }
    else{
        char buffer[BUF_SIZE];
        memset(buffer, '\0', BUF_SIZE);
        while(recv(confd, buffer, BUF_SIZE-1, 0) > 0){ }
        close(confd);
    }
    close(sock);
    return 0;
}