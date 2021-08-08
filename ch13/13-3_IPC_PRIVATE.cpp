//使用IPC_PRIVATE信号量
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

//推荐的semctl的命令格式，用于第四个参数
union semun
{
    int val;    //用于SETVAL命令
    struct semid_ds* buf;   //用于IPC_STAT和IPC_SET命令
    unsigned short int * array; //用于GETALL和SETALL命令
    struct seminfo* __buf;   //用于IPC_INFO命令
};

//op为-1时执行P操作，op为1时执行V操作
void pv(int sem_id, int op)
{
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = op;
    sem_b.sem_flg = SEM_UNDO; //sem_flg的取值可以是IPC_NOWAIT和SEM_UNDO，IPC_NOWAIT的含义是无论信号量操作是否成功，semop调用都会立即返回，类似于非阻塞IO，SEM_UNDO的含义是当进程退出时取消正在进行的semop操作
    semop(sem_id, &sem_b, 1);
}

int main(int argc, char* argv[])
{
    int sem_id = semget(IPC_PRIVATE, 1, 0666);
    union semun sem_un;
    sem_un.val = 1;
    semctl(sem_id, 0, SETVAL, sem_un);

    pid_t id = fork();
    if (id < 0)
    {
        return 1;
    }
    else if (id == 0)
    {
        printf("child try getting binary sem.\n");
        //在父子进程间共享的IPC_PRIVATE信号量的关键在于二者都可以操作该信号量的标识符sem_id
        pv(sem_id, -1);
        printf("child get the sem and would release it after 5 seconds.\n");
        sleep(5);
        pv(sem_id, 1);
        exit(0);
    }
    else
    {
        printf("parent try getting binary sem.\n");
        pv(sem_id, -1);
        printf("parent get the sem and would release it after seconds.\n");
        sleep(5);
        pv(sem_id, 1);
    }
    waitpid(id, NULL, 0);
    semctl(sem_id, 0, IPC_PRIVATE, sem_un); //删除信号量
    return 0;
}