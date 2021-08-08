#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define FD_LIMIT 1024
#define MAX_EVENT_LIMIT 1024
#define PROCESS_LIMIT 65536

//编译命令 g++ 13-4_shm_communication.cpp -o shm_communication -lrt  因为用到了POSIX共享内存函数shm_unlink和shm_open

//处理一个客户连接的必要数据
struct client_data
{
    sockaddr_in address;
    int connfd;
    pid_t pid;    //处理这个连接的子进程的PID
    int pipefd[2];   //和父进程通信用的管道
};

static const char* shm_name = "/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd;
char* share_mem = 0;
//客户连接数组，进程用客户链接的编号来索引这个数组即可取得相关的客户连接数据
client_data* users = 0;
//子进程和客户连接的映射关系表，用进程的PID来索引这个数组，即可获得该子进程所处理的客户端的编号
int* sub_process = 0;

int user_count = 0;
bool stop_child = false;

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

void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig, void(*handler)(int), bool restart=true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
    {
        sa.sa_flags = SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void del_resource()
{
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    shm_unlink(shm_name);
    delete [] users;
    delete [] sub_process;
}

void child_term_handler(int sig)
{
    stop_child = true;
}

//子进程运行的函数，参数idx指出该子进程处理的客户链接编号，users是保存所有客户链接数据的数组，参数share_mem指出共享内存的起始地址
int run_child(int idx, client_data* users, char* share_mem)
{
    epoll_event events[MAX_EVENT_LIMIT];
    //子进程使用IO复用技术同时监听两个文件描述符，客户连接socket和与父进程通信的管道文件描述符
    int child_epollfd = epoll_create(5);
    assert(child_epollfd != -1);
    int connfd = users[idx].connfd;
    addfd(child_epollfd, connfd);
    int pipefd = users[idx].pipefd[1];
    addfd(child_epollfd, pipefd);
    int ret;
    //子进程需要设置自己的信号处理函数
    addsig(SIGTERM, child_term_handler, false);

    while(!stop_child)
    {
        int number = epoll_wait(child_epollfd, events, MAX_EVENT_LIMIT, -1);
        if (number < 0 && errno != EINTR)
        {
            printf("epoll failure.\n");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            //子进程负责的客户连接有新的数据到达
            if (sockfd == connfd && events[i].events & EPOLLIN)
            {
                memset(share_mem + idx * BUFFER_SIZE, '\0', BUFFER_SIZE);
                //将客户数据读取到对应的读缓存中，该读缓存是共享内存的一段，开始于idx*BUFFER_SIZE处，长度为BUFFER_SIZE字节，因此各个客户的读缓存是共享的
                ret = recv(connfd, share_mem + idx*BUFFER_SIZE, BUFFER_SIZE-1, 0);
                if (ret < 0)
                {
                    if (errno != EAGAIN)
                    {
                        stop_child = true;
                    }
                }
                else if (ret == 0)
                {
                    stop_child = true;
                }
                else
                {
                    //成功读取客户数据后就通过管道通知主进程来处理
                    send(pipefd, (char*)&idx, sizeof(idx), 0);
                }
            }
            else if (sockfd == pipefd && events[i].events & EPOLLIN)
            {
                int client = 0;
                //接收主进程发送来的数据，即有客户数据到达的连接的编号
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if (ret < 0)
                {
                    if (errno != EAGAIN)
                    {
                        stop_child = true;
                    }
                }
                else if (ret == 0)
                {
                    stop_child = true;
                }
                else 
                {
                    send(connfd, share_mem + client * BUFFER_SIZE, BUFFER_SIZE, 0);
                }
            }
            else
            {
                continue;
            }
        }
    }
    close(connfd);
    close(pipefd);
    close(child_epollfd);
    return 0;
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
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    user_count = 0;
    users = new client_data[USER_LIMIT+1];
    sub_process = new int[PROCESS_LIMIT];
    for (int i = 0; i < PROCESS_LIMIT; i++)
    {
        sub_process[i] = -1;
    }
    epoll_event events[MAX_EVENT_LIMIT];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);
    //int socketpair(int d, int type, int protocol, int sv[2]),创建一对相互连接的套接字，可以往sv[0]写，从sv[1]中读，反之也行，这对管道是全双工通信，但不同进程之间通信时，需要关闭写端的读管道，读端需要关闭写管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);
    addfd(epollfd, sig_pipefd[0]); //为什么将sig_pipefd[0]注册到内核时间表中呢？？因为子进程sig_handler会将信号写入到sig_pipefd[0]中，所以主线程中要监听sig_pipefd[0]上的事件

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
    bool stop_server = false;
    bool terminate = false;

    //创建共享内存，作为所有客户socket的读缓存
    shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    assert(shmfd != -1);
    ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE);
    assert(ret != -1);

    share_mem = (char*)mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    assert(share_mem != MAP_FAILED);
    close(shmfd);

    printf("Start running server...\n");
    while(!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_LIMIT, -1);
        if (number < 0 && errno != EINTR)
        {
            printf("epoll failure. \n");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            //新的客户连接到来
            if (sockfd == listenfd && events[i].events & EPOLLIN)
            {
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int connfd = accept(listenfd, (struct sockaddr*)&client, &client_len);
                if (connfd < 0)
                {
                    printf("errno is : %d\n", errno);
                    continue;
                }
                if (user_count >= USER_LIMIT)
                {
                    const char* info = "too many users.\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);

                    close(connfd);
                    continue;
                }
                //正常情况，保存第user_count个客户连接的相关数据
                users[user_count].address = client;
                users[user_count].connfd = connfd;
                //在主进程与子进程之间建立管道全双工pipefd，以传递必要数据
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                assert(ret != -1);
                pid_t pid = fork();
                if (pid < 0)
                {
                    close(connfd);
                    continue;
                }
                else if(pid == 0)
                {
                    close(epollfd);
                    close(listenfd); //子进程关掉监听socket
                    close(users[user_count].pipefd[0]); //因为子进程要往pipefd[1]写数据跟主进程通信，所以要关掉子进程中的pipefd[0]
                    close(sig_pipefd[0]); //这两个为啥关掉？？
                    close(sig_pipefd[1]);
                    run_child(user_count, users, share_mem);
                    munmap((void*)share_mem, USER_LIMIT * BUFFER_SIZE);
                    exit(0);
                }
                else //主进程
                {
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    addfd(epollfd, users[user_count].pipefd[0]);
                    users[user_count].pid = pid;
                    //记录新的客户连接在数组users中的索引，建立进程pid和该索引值之间的映射关系
                    sub_process[pid] = user_count;
                    user_count++;
                }
            }
            //处理信号事件
            else if (sockfd == sig_pipefd[0] && events[i].events & EPOLLIN)
            {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch(signals[i])
                        {
                            //子进程退出，表示某个客户端关闭了连接
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                                {
                                    //用子进程的pid取得被关闭的客户连接的编号
                                    int del_user = sub_process[pid];
                                    sub_process[pid] = -1;
                                    if (del_user < 0 || del_user > USER_LIMIT)
                                    {
                                        continue;
                                    }
                                    epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].pipefd[0], 0);
                                    close(users[del_user].pipefd[0]);
                                    users[del_user] = users[--user_count];
                                    sub_process[users[del_user].pid] = del_user;
                                }
                                if (terminate && user_count == 0)
                                {
                                    stop_server = true;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                //结束服务器程序
                                printf("kill all child now.\n");
                                if (user_count == 0)
                                {
                                    stop_server = true;
                                    break;
                                }
                                for (int i = 0; i < user_count; i++)
                                {
                                    int pid = users[i].pid;
                                    kill(pid, SIGTERM);
                                }
                                terminate = true;
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                }
            }
            //某个子进程向父进程写入了数据
            else if (events[i].events & EPOLLIN)
            {
                int child = 0;
                //读取管道数据，child变量记录了是哪个客户连接有数据到达
                ret = recv(sockfd, (char*)&child, sizeof(child), 0);
                printf("read data from child accross pipe.\n");
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else
                {
                    //向除了负责第child个客户连接的子进程之外的子进程发送消息，通知他们有客户数据要写
                    for (int j = 0; j < user_count; j++)
                    {
                        if (users[j].pipefd[0] != sockfd)
                        {
                            printf("send data to child accross pipe.\n");
                            send(users[j].pipefd[0], (char*)&child, sizeof(child), 0);
                        }
                    }
                }
            }
        }
    }
    del_resource();
    return 0;
}