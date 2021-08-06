#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
static int pipefd[2];

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void sig_handler(int sig)
{
    //保留原来的errno，在函数最后恢复，保证函数的可重入性
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0); //将信号写入管道，通知主循环
    errno = save_errno;
}

//给信号设置处理函数
void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART; //将信号设置为重启，使得被信号中断的系统调用能够自动重启，例如recv，send等
    sigfillset(&sa.sa_mask); //在信号集sa.sa_mask中设置所有信号
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char* argv[])
{
    if (argc <= 2)
    {
        printf("Usage: %s ip port\n", argv[0]);
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
    int epoll_fd = epoll_create(5);
    assert(epoll_fd >= 0);

    addfd(epoll_fd, listenfd);

    //使用sockpair创建管道，注册pipefd[0]上的可读事件
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);  //int socketpair(int domain, int type, int protocol, int sv[2]) domain 只能为AF_LOCAL或者AF_UNIX， protocol只能为0，只能用于有亲缘关系的进程或者线程间通信，所创建的套接字对均能够读写
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epoll_fd, pipefd[0]);

    //设置一些信号的处理函数
    addsig(SIGHUP);
    addsig(SIGCHLD);
    addsig(SIGTERM);
    addsig(SIGINT);
    bool stop_server = false;

    while(!stop_server)
    {
        int number = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1); //-1代表timeout参数，表示epoll将会阻塞直到某个事件发生
        if (number < 0 && errno != EINTR)
        {
            printf("epoll failure.\n");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            //如果是监听socket，则处理新的连接请求
            if (sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_len = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_len);
                addfd(epoll_fd, connfd); //将新建的socket文件描述符添加到epoll内核事件表中
            }
            else if (sockfd == pipefd[0] && events[i].events & EPOLLIN)
            {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1)
                {
                    continue;
                }
                else if(ret == 0)
                {
                    continue;
                }
                //因为每个信号占1个字节，所以按字节逐个接受信号，以SIGTERM为例，说明如何安全地终止服务器主循环
                else 
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch (signals[i])
                        {
                            case SIGCHLD:
                            case SIGHUP:
                            {
                                continue;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            else
            {

            }
        }
    }
    printf("close fds\n");
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    return 0;
}

