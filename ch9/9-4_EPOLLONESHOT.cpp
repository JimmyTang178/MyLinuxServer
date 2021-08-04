#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 1024

struct fds
{
    int epollfd;
    int sockfd;
};

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/*将fd上的EPOLLIN和EPOLLET事件注册到epollfd指向的epoll内核事件表中，
参数oneshot指定是否注册fd上的EPOLLONESHOT事件*/
void addfd(int epollfd, int fd, bool oneshot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;  //注册这两个事件
    if (oneshot)
    {
        event.events |= EPOLLONESHOT;  //注册EPOLLONESHOT事件
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/*重置fd上的事件，这样操作之后，尽管fd上的EPOLLONESHOT事件被注册，
  但操作系统仍然会触发fd上的EPOLLIN事件，且只触发一次*/
void reset_oneshot(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//工作线程
void* worker(void* arg)
{
    int sockfd = ((fds*)arg)->sockfd;
    int epollfd = ((fds*)arg)->epollfd;
    printf("start new thread to receive data on fd: %d \n", sockfd);
    char buf[BUFFER_SIZE];
    memset(buf, '\0', BUFFER_SIZE);
    /*循环读取sockfd上的数据，直到遇到EAGAIN错误*/
    while(1)
    {
        int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
        if (ret == 0)
        {
            close(sockfd);
            printf("foreigner closed connection\n");
            break;
        }
        else if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        }
        else
        {
            printf("get content: %s\n", buf);
            sleep(5);
        }
    }
    printf("end thread receiving data on fd: %d\n", sockfd);
}
//编译 g++ -Wall -o EPOLLONESHOT 9-4_EPOLLONESHOT.cpp  -lpthread

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s ip port.\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    /*注意，监听socket listenfd是不能注册EPOLLONESHOT事件的，否则应用程序只能处理一个客户连接，因为后续的客户连接将不再触发listenfd上的EPOLLIN事件*/
    addfd(epollfd, listenfd, false);

    while(1)
    {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1); //返回ret是就绪事件的个数
        if (ret < 0)
        {
            printf("epoll failure. \n");
            break;
        }
        for (int i = 0; i < ret; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd)
            {
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int connfd = accept(listenfd, (struct sockaddr*)&client, &client_len);
                //对于每个非监听文件描述符都注册EPOLLONESHOT事件
                addfd(epollfd, connfd, true);
            }
            else if (events[i].events & EPOLLIN)
            {
                pthread_t thread;
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = sockfd;
                //启动一个新工作进程为sockfd服务
                int p_ret = pthread_create(&thread, NULL, worker, (void*)&fds_for_new_worker);
                assert(p_ret == 0);
            }
            else
            {
                printf("Something else happened. \n");
            }
        }
    }
    close(listenfd);
    return 0;
}