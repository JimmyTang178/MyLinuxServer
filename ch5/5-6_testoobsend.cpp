//本程序发送，testoobrecv.cpp接收
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char* argv[]){
    if (argc <= 2){
        printf("Usage: %s ip port\n", argv[0]);
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0){
        printf("Connection failed\n");
    }
    else{
        const char* data = "abc";
        const char* normal_data = "123";
        send(sock, normal_data, strlen(normal_data), 0);
        send(sock, data, strlen(data), MSG_OOB); //发送或接收紧急数据
        send(sock, normal_data, strlen(normal_data), 0);
    }
    close(sock);
    return 0;
}