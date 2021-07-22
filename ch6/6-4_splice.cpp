#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

//splice函数实现将客户端发来的数据反射回客户端

int main(int argc, char* argv[]){
    if (argc <= 2){
        printf("Usage: %s ip port \n", basename(argv[1]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock > 0);

    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int confd = accept(sock, (struct sockaddr*)&client, &client_len);
    
    if (confd < 0) {
        printf("errno is %d \n", errno);
    }
    else{
        int pipefd[2];
        assert(ret != -1);
        ret = pipe(pipefd); //创建管道， 即pipefd[0]是读管道，只能往外读数据，pipefd[1]是写管道，只能往里写数据
        //将confd上流入的客户数据定向到管道中
        //splice函数用于两个文件描述符之间的移动数据，也是零拷贝操作，即直接在内核中拷贝移动数据，其中两个文件描述符中至少有一个是管道文件描述符
        ret = splice(confd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        assert(ret != -1);
        //将管道的输出定向到confd客户端文件描述符中，写回客户端
        ret = splice(pipefd[0], NULL, confd, NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
        assert(ret != -1);
        close(confd);
    }
    close(sock);
    return 0;
}