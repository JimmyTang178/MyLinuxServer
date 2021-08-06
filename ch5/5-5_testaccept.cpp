#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]){
    if (argc <= 2){
        printf("Usage: %s ip port\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(PF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, 5); //5 表示监听队列的最大长度，表示队列中处于完全连接状态的socket上限
    assert(ret != -1);

    /*暂停20秒以等待客户端退出或者掉线*/
    sleep(20);

    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int confd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if (confd < 0){
        printf("errno is : %d", errno);
    }
    else{
        /*连接成功则打印出客户端的IP地址和端口号*/
        char remote[INET_ADDRSTRLEN];
        printf("connected with ip: %s and port: %d\n", inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN), ntohs(client.sin_port));
        close(confd);
    }
    //在unistd.h中，严格来说并不是关闭一个链接，而是将连接的引用计数减一
    close(sock);
    return 0;
}