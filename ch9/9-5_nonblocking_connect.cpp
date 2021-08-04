#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#define BUFFER_SIZE 1023

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_GETFL, new_option);
    return old_option;
}
//超时连接函数，参数分别是服务器IP、端口号和超时时间。成功返回已经处于连接状态的socket，失败则返回-1
int nonblock_connect(const char* ip, int port, int time)
{
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    int fdopt = setnonblocking(sockfd); //fdopt保存的是sockfd原来的设置，也就是sockfd是阻塞的
    ret = connect(sockfd, (struct sockaddr*)&address, sizeof(address)); //connect成功时返回0，此时这个connect是非阻塞的
    if (ret == 0)
    {
        //如果连接成功，则恢复sockfd的属性，并立即返回
        printf("connect with server successfully.\n");
        fcntl(sockfd, F_SETFL, fdopt);  //sockfd恢复成阻塞的
        return sockfd;
    }
    else if (errno != EINPROGRESS)
    {
        //如果连接没有建立，那么只有当errno是EINPROGRESS时才表示连接还在进行中，否则返回出错
        printf("nonblock connect not support.\n");
        return -1;
    }
    fd_set readfds;   //fd_set结构体仅包含一个整型数组，该数组的每个元素的每一位标记一个文件描述符
    fd_set writefds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &writefds);  //FD_SET(int fd, fd_set* fdset)设置writefds的sockfd位

    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    
    ret = select(sockfd+1, NULL, &writefds, NULL, &timeout); //int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptionfds, struct timeval timeout)
    if (ret <= 0)                                          //readfds,writefds,exceptionfds分别指向可读可写和异常事件对应的文件描述符集合
    {
        //select超时或者出错，立即返回
        printf("connection time out\n");
        close(sockfd);
        return -1;
    }
    if (!FD_ISSET(sockfd, &writefds)) //判断sockfd是否有可写事件
    {
        printf("no events on sockfd found.\n");
        return -1;
    }
    int error = 0;
    socklen_t length = sizeof(error);
    //printf("length %d\n", length); //length 4
    //调用getsockopt来获取并清除sockfd上的错误,成功返回0，失败返回-1
    //int getsockopt(int socket, int level, int opt_name, void*restrict option_value, socklen_t *restrict option_len);
    //获取一个套接字的选项， level协议层次：SOL_SOCKET套接字层次， IPPROTO_IP IP层次， IPPROTO_TCP TCP层次
    //option_name:  SO_SNDBUF 获取发送缓冲区长度  SO_RCVBUF 获取接收缓冲区长度   SO_REUSEADDR 是否允许重复使用本地地址 等等
    //option_value:获取到的选项的值
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &length) < 0)
    {
        printf("get socket option failed.\n");
        close(sockfd);
        return -1;
    }
    //错误号不为0说明连接出错
    if (error != 0)
    {
        printf("connection failed after select with the error: %d \n", error);
        close(sockfd);
        return -1;
    }
    //连接成功
    printf("connection ready after select with the socket : %d \n", sockfd);
    fcntl(sockfd, F_SETFL, fdopt);
    return sockfd;
}

int main(int argc, char* argv[])
{
    if (argc <= 2)
    {
        printf("Usage: %s ip port \n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = nonblock_connect(ip, port, 10);

    if (sockfd < 0)
    {
        return 1;
    }
    printf("connection ended.\n");
    close(sockfd);
    return 0;
}