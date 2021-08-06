#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <string.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

//将文件描述符设置成非阻塞的
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL); //fcntl函数针对文件描述符fd提供控制，参数F_GETFL获取文件打开的标志，标置含义与open一致
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;  //为啥返回old_option???
}

//将文件描述符fd上的EPOLLIN注册到epollfd指向的epoll内核事件表中，参数enable_et指定是否对fd启用ET模式
void addfd(int epollfd, int fd, bool enable_et)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (enable_et)
    {
        event.events |= EPOLLET;
    }
    //int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)用来操作epoll的内核事件表
    //epfd是指向epoll内核事件表的文件描述符，fd是要操作的文件描述符， event指定事件
    //成功时返回0，失败是返回-1并设置errno
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//LT 模式的工作流程
void lt(epoll_event* events, int number, int epollfd, int listenfd)
{
    char buf[BUFFER_SIZE];
    for (int i = 0; i < number; i++)
    {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd) //这个是怎么意思？
        {
            struct sockaddr_in client_address;
            socklen_t client_len = sizeof(client_address);
            //int connfd = accept(listenfd, (struct sockaddr*)&client_address, &sizeof(client_address)); //sizeof()返回左值，不能用&符号
            int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_len);
            addfd(epollfd, connfd, false); //对connfd禁用ET模式
        }
        else if (events[i].events & EPOLLIN)
        {
            //只要socket读缓存中还有未读出的数据，这段代码就会被反复触发
            printf("LT event trigger once.\n");
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
            if (ret <= 0)
            {
                close(sockfd);
                continue;
            }
            printf("get %d bytes of content: %s \n", ret, buf);
        }
        else
        {
            printf("Something wrong happen.\n");
        }
    }
}

//ET 模式的工作流程, 每个使用ET模式的文件描述符都应该是非阻塞的，因为如果文件描述符是阻塞的，那么写或者读会因为没有后续事件而处于阻塞状态
void et(epoll_event* events, int number, int epollfd, int listenfd)
{
    char buf[BUFFER_SIZE];
    for (int i = 0; i < number; i++)
    {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd)
        {
            struct sockaddr_in client_address;
            socklen_t client_len = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_len);
            addfd(epollfd, connfd, true); //connfd开启ET模式
        }
        else if (events[i].events & EPOLLIN)
        {
            //这段代码不会被重复触发，所以循环读取数据，以确保把socket读缓存中的数据全部读出
            printf("ET event trigger once.\n");
            while(1)
            {
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
                if (ret < 0)
                {
                    /*对于非阻塞IO， 下面的条件成立，说明数据已经全部读取完毕，此后epoll就能
                    再次触发sockfd上的EPOLLIN事件，驱动下一次读操作*/
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                    {
                        printf("read later.\n");
                        break;
                    }
                    close(sockfd);
                    break;
                }
                else if (ret == 0)
                {
                    close(sockfd);
                }
                else
                {
                    printf("Get %d bytes of content: %s .\n", ret, buf);
                }
            }
        }
        else
        {
            printf("Something else happend \n");
        }
    }
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

    int ret = 0;

    struct sockaddr_in address;
    bzero(&address, sizeof(address)); //在string.h
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    ret = bind(listenfd, (struct sockaddr*) &address, sizeof(address));
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    addfd(epollfd, listenfd, true);

    while(1)
    {
        //int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);
        //timeout为-1时，epoll调用将会阻塞，0时epoll调用立即返回，整数时表示等待超时时间，毫秒级
        //epfd指向内核事件表，epoll如果检测到事件，就将所有”就绪“事件从内核事件表中复制到events指向的数组中，这样就能提高应用程序检索文件描述符的效率
        //返回就绪的文件描述符的个数
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0)
        {
            printf("epoll failure \n");
            break;
        }
        lt(events, ret, epollfd, listenfd);
        //et(events, ret, epollfd, listenfd);
    }
    close(listenfd);
    return 0;
}
