#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool stop = false;

//SIGTERM 信号的处理函数，触发时结束主函数中的循环
static void handle_term(int sig){
    stop = true;
}

int main(int argc, char* argv[]){
    signal(SIGTERM, handle_term);
    if (argc <= 3){
        printf("Usage: %s ip_address port_number backlog\n", argv[0]);
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    int backlog = atoi(argv[3]);

    int sock = socket(PF_INET, SOCK_STREAM, 0);

    assert(sock >= 0);
    
    //创建一个ipv4地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr); //inet_pton(int af, const char* src, void* dst)将字符串表示的IP地址转换成网络字节序整数表示的IP地址，并把地址存储到dst指向的内存中
    address.sin_port = htons(port);

    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, backlog);
    assert(ret != -1);

    //循环等待连接，直到有SIGTERM信号中断
    while(!stop){
        sleep(1);
    }
    //关闭socket
    close(sock);
    return 0;
}