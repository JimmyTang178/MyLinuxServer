#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h> //file control

//实现将标准输入的数据分别定向到文件和标准输出中

int main(int argc, char* argv[]){
    if (argc != 2){
        printf(" Usage: %s <file> \n", argv[0]);
        return 1;
    }

    int filefd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
    assert(filefd > 0);
    int pipefd_stdout[2];
    int ret = pipe(pipefd_stdout);
    assert(ret != -1);

    int pipefd_file[2];
    ret = pipe(pipefd_file);
    assert(ret != -1);

    //将标准输入内容重定向到输入管道
    ret = splice(STDIN_FILENO, NULL, pipefd_stdout[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret != -1);

    /*用tee函数将pipefd_stdout 的输出 复制到 管道pipefd_file的输入到*/
    ret = tee(pipefd_stdout[0], pipefd_file[1], 32768, SPLICE_F_NONBLOCK);
    assert(ret != -1);
    /*将管道pipefd_file的输出定向到文件描述符filefd上，从而将标准输入写入文件*/
    ret = splice(pipefd_file[0], NULL, filefd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret != -1);

    /*将管道pipefd_stdout输出定向到标准输出，其内容和写入文件的内容完全一致*/
    ret = splice(pipefd_stdout[0], NULL, STDOUT_FILENO, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret != -1);

    close(filefd);
    close(pipefd_stdout[0]);
    close(pipefd_stdout[1]);
    close(pipefd_file[0]);
    close(pipefd_file[1]); //管道两端需要分别关闭
    return 0;
}