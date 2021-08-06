#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char* argv[]){
    if (argc <= 3){
        printf("Usage: %s ip port file", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    const char* file_name = argv[3];
    int filefd = open(file_name, O_RDONLY);
    assert(filefd > 0);
    struct stat stat_buf;
    fstat(filefd, &stat_buf);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = PF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int confd = accept(sock, (struct sockaddr*)&client, &client_len);

    if (confd < 0){
        printf("errno is %d\n", errno);
    }
    else {
        //ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count); 其中in_fd必须是真实文件的描述符，不能是socket连接和管道
        sendfile(confd, filefd, NULL, stat_buf.st_size); //没有读取filefd描述符的内容到用户空间，而是直接在内核中操作，效率很高，称为零拷贝
        close(confd);
    }
    close(confd);
    return 0;
}