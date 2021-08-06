//聊天室程序-服务端
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

//客户数据：客户端socket地址、待写到客户端数据的地址、从客户端读入的数据
struct client_data
{
    sockaddr_in address;
    char* write_buf;
    char buf[BUFFER_SIZE];
};

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
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

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    int ret = bind(sockfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sockfd, 9);
    assert(ret != -1);

    //创建users数组，分配FD_LIMIT个client_data对象，每个的socket连接都能获得这样一个对象，并且socket的值可以直接用来索引socket连接对应的client_data对象，这是将socket和客户数据相关联的高效方法
    client_data* users = new client_data[FD_LIMIT];
    //尽管分配了足够多的client_data对象，但为了提高poll性能，仍然要限制用户数量
    pollfd fds[USER_LIMIT+1];
    int user_count = 0;
    for (int i = 1; i <= USER_LIMIT; i++)
    {
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    fds[0].fd = sockfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    while(1)
    {
        ret = poll(fds, user_count+1, -1);
        if (ret < 0)
        {
            printf("poll error.\n");
            break;
        }
        for (int i = 0; i < user_count+1; i++)
        {
            if ((fds[i].fd == sockfd) && (fds[i].revents & POLLIN))
            {
                struct sockaddr_in client_address;
                socklen_t client_len = sizeof(client_address);
                int connfd = accept(sockfd, (struct sockaddr*)&client_address, &client_len);
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
                //对于新的连接，同时修改fds和users数组，users[connfd]对应于新连接文件描述符connfd的客户数据
                user_count++;
                users[connfd].address = client_address;
                setnonblocking(connfd);
                fds[user_count].fd = connfd;
                fds[user_count].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_count].revents = 0;
                printf("comes a new user, now have %d users\n", user_count);
        }
        else if (fds[i].revents & POLLERR)
        {
            printf("get an error from %d\n", fds[i].fd);
            char errors[100];
            memset(errors, '\0', 100);
            socklen_t length = sizeof(errors);
            if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0)
            {
                printf("get socket option failed\n");
            }
            continue;
        }
        else if (fds[i].revents & POLLRDHUP)
        {
            //如果客户端关闭连接，则服务端也关闭对应的连接，并将客户数减一
            users[fds[i].fd] = users[fds[user_count].fd];
            close(fds[i].fd);
            fds[i] = fds[user_count];
            i--;
            user_count--;
            printf("a client left \n");
        }
        else if (fds[i].revents & POLLIN)
        {
            int connfd = fds[i].fd;
            memset(users[connfd].buf, '\0', BUFFER_SIZE);
            ret = recv(connfd, users[connfd].buf, BUFFER_SIZE-1, 0);
            printf("get %d bytes of client data %s from %d \n", ret, users[connfd].buf, connfd);

                if (ret < 0)
                {
                    //如果读出错误，则关闭连接
                    if (errno != EAGAIN)
                    {
                        close(connfd);
                        users[fds[i].fd] = users[fds[user_count].fd];
                        fds[i] = fds[user_count];
                        i--;
                        user_count--;
                    }
                }
                else if (ret == 0)
                {

                }
                else
                {
                    //如果收到客户数据，则通知其他socket准备写数据
                    for (int j = 1; j <= user_count; j++)
                    {
                        if (fds[j].fd == connfd)
                        {
                            continue;
                        }
                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT; //给这个socket注册数据可写事件
                        users[fds[j].fd].write_buf = users[connfd].buf;
                    }
                }
        }
        else if (fds[i].revents & POLLOUT)
        {
            int connfd = fds[i].fd;
            if (!users[connfd].write_buf)
            {
                continue;
            }
            ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
            assert(ret >= 0);
            users[connfd].write_buf = NULL;
            //写完数据后需要重新注册fds[i]上的可读事件
            fds[i].events |= ~POLLOUT; //取消注册数据可写事件 167行的逆操作
            fds[i].events |= POLLIN;  //注册数据可读事件
        }
        }
    }
    
    delete [] users;
    close(sockfd);
    return 0;
}