//接收testoobsend.cpp的数据
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

    if (argc <= 2){
        printf("usage: %s ip port\n", argv[0]);
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

    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int confd = accept(sock, (struct sockaddr*)&client, &client_len);
    if (confd < 0){
        printf("errno is %d\n", errno);
    }
    else{
        //接收数据
        char buffer[BUF_SIZE];
        memset(buffer, '\0', BUF_SIZE);
        ret = recv(confd, buffer, BUF_SIZE-1, 0); //从建立的socket连接confd中接收数据
        printf("got %d bytes of normal data %s \n", ret, buffer);

        memset(buffer, '\0', BUF_SIZE);
        ret = recv(confd, buffer, BUF_SIZE-1, MSG_OOB);//这里的MSG_OOB与发送程序中的相对应
        printf("got %d bytes of oob data %s \n", ret, buffer);

        memset(buffer, '\0', BUF_SIZE);
        ret = recv(confd, buffer, BUF_SIZE-1, 0);
        printf("got %d bytes of normal data %s \n", ret, buffer);
        close(confd);
    }
    close(sock);
    return 0;
}