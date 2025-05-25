#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

void pv(int semid, int op)
{
    struct sembuf sem;
    sem.sem_num = 0;
    sem.sem_op = op;
    sem.sem_flg = SEM_UNDO; // 进程结束后，自动释放信号量
    semop(semid, &sem, 1);
}

int main()
{
    int semid = semget(IPC_PRIVATE, 1, 0666);
    union semun sem_un;
    sem_un.val = 1;
    semctl(semid, 0, SETVAL, sem_un);
    pid_t pid = fork();
    if (pid < 0)
    {
        return 1;
    }
    else if (pid == 0)
    {
        printf("child process\n");
        pv(semid, -1);
        exit(0);
    }
    else
    {
        printf("parent process\n");
        pv(semid, 1);
        printf("parent get the sem, would release it after 5 seconds\n");
        sleep(5);
        pv(semid, 1);
    }
    waitpid(pid, NULL, 0);
    semctl(semid, 0, IPC_RMID, sem_un); // 立即删除信号量
    return 0;
}
