#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char* argv[]){
    if (argc <= 2){
        printf("Usage: %s ip port\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int confd = accept(listenfd, (struct sockaddr*)&client, &client_len);
    
    if (confd < 0){
        printf("errno is %d\n", errno);
        close(listenfd);
    }
    else{
        char buf[1024];
        fd_set read_fds;
        fd_set exception_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&exception_fds);

        while(1){
            memset(buf, '\0', sizeof(buf));
            //每次调用select前都要重新在read_fds和exception_fds中设置文件描述符confd，因为事件发生后，文件描述符集合将会被内核修改
            FD_SET(confd, &read_fds);
            FD_SET(confd, &exception_fds);
            ret = select(confd+1, &read_fds, NULL, &exception_fds, NULL);
            if (ret < 0){
                printf("selection ffailure.\n");
                break;
            }
            if (FD_ISSET(confd, &read_fds)){
                ret = recv(confd, buf, sizeof(buf)-1, 0); //recv返回成功读取的字节数
                if (ret <= 0){
                    break;
                }
                printf("get %d bytes of normal data: %s\n", ret, buf);
            }
            //对于异常事件，采用带MSG_OOB标志的recv函数读取带外数据。What's 带外数据？
            else if (FD_ISSET(confd, &exception_fds)){
                ret = recv(confd, buf, sizeof(buf)-1, MSG_OOB);
                if (ret <= 0){
                    printf("recv no data.\n");
                    break;
                }
                printf("get %d bytes of oob data: %s\n", ret, buf);
            }
        }

    }
    close(confd);
    close(listenfd);
    return 0;
}