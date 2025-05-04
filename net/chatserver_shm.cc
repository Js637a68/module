#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <libgen.h>
#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define PROCESS_LIMIT 65535
/*
虽然使用共享内存来存储数据，但是依然没有解决丢失消息的问题
本质还是只有一个缓存区，之前时不同客户端同时发送消息，导致消息丢失
现在时一个客户端发送两个消息时，可能因为网络延迟导致两数据包同时到达，第二个消息会覆盖第一个消息
*/
struct client_data
{
    sockaddr_in address;
    int connfd;
    pid_t pid;
    int pipefd[2];
};
static const char *shm_name = "/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd;
char *share_mem = NULL;
/*客户连接数组。进程用客户连接的编号来索引这个数组，即可取得相关的客户连接数
据*/
client_data *users = NULL;
/*子进程和客户连接的映射关系表。用进程的PID来索引这个数组，即可取得该进程所处
理的客户连接的编号*/
int *sub_process = NULL;
int user_counter = 0;
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
}
void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}
void addsig(int sig, void (*sig_handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
void del_source()
{
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    shm_unlink(shm_name);
    delete[] users;
    delete[] sub_process;
}
void child_term_handler(int sig)
{
    stop_child = true;
}
// 子进程运行函数,idx为子进程的编号,users为共享内存中保存的客户连接信息,share_mem为共享内存的地址
int run_child(int idx, client_data *users, char *share_mem)
{
    // 创建epoll事件表
    epoll_event events[MAX_EVENT_NUMBER];
    int child_epollfd = epoll_create(5);
    assert(child_epollfd != -1);
    // 将监听socket和管道读端口添加到epoll事件表中
    int connfd = users[idx].connfd;
    addfd(child_epollfd, connfd);
    int pipefd = users[idx].pipefd[1];
    addfd(child_epollfd, pipefd);
    int ret;
    // 子进程需要设置信号处理函数
    addsig(SIGTERM, child_term_handler, false);
    while (!stop_child)
    {
        int number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == connfd && events[i].events & EPOLLIN)
            {
                memset(share_mem + idx * BUFFER_SIZE, '\0', BUFFER_SIZE);
                // 将客户端读取到对应的读缓存，该缓存是共享内存的一部分，他开始于idx*BUFFER_SIZE，长度为BUFFER_SIZE，
                // 因此，各个客户端的读缓存是共享的
                ret = recv(connfd, share_mem + idx * BUFFER_SIZE, BUFFER_SIZE, 0);
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
                    // 成功读取数据后就通知主进程（通过管道的方式）
                    send(pipefd, (char *)&idx, sizeof(idx), 0);
                }
            } // 通知主进程将第client个客户端的数据发送到其他客户端
            else if (sockfd == pipefd && events[i].events & EPOLLIN)
            {
                int client;
                ret = recv(sockfd, (char *)&client, sizeof(client), 0);
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
                continue;
        }
    }
    close(connfd);
    close(pipefd);
    close(child_epollfd);
    return 0;
}
int main(int argc, char *argv[])
{
    if (argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip);
    address.sin_port = htons(port);
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);
    user_counter = 0;
    users = new client_data[USER_LIMIT + 1];
    sub_process = new int[PROCESS_LIMIT];
    for (int i = 0; i < PROCESS_LIMIT; i++)
    {
        sub_process[i] = -1;
    }
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);
    addfd(epollfd, sig_pipefd[0]);
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
    bool stop_server = false;
    bool terminate = false;
    // 创建共享内存
    shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    ret = ftruncate(shmfd, BUFFER_SIZE * USER_LIMIT);
    share_mem = (char *)mmap(NULL, sizeof(char) * BUFFER_SIZE * USER_LIMIT, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    assert(share_mem != MAP_FAILED);
    close(shmfd);
    while (!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                if (connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if (user_counter >= USER_LIMIT)
                {
                    const char *info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }
                users[user_counter].address = client_address;
                users[user_counter].connfd = connfd;
                int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_counter].pipefd);
                assert(ret != -1);
                pid_t pid = fork();
                if (pid < 0)
                {
                    close(connfd);
                    continue;
                }
                else if (pid == 0)
                {
                    close(epollfd);
                    close(listenfd);
                    close(users[user_counter].pipefd[0]);
                    close(sig_pipefd[1]);
                    close(sig_pipefd[0]);
                    run_child(user_counter, users, share_mem);
                    munmap(share_mem, sizeof(char) * BUFFER_SIZE * USER_LIMIT);
                    exit(0);
                }
                else
                {
                    close(connfd);
                    close(users[user_counter].pipefd[1]);
                    addfd(epollfd, users[user_counter].pipefd[0]);
                    users[user_counter].pid = pid;
                    sub_process[pid] = user_counter;
                    user_counter++;
                }
            }
            else if (sockfd == sig_pipefd[0] && events[i].events & EPOLLIN)
            {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0)
                    continue;
                else
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch (signals[i])
                        {
                        // 处理子进程退出信号
                        case SIGCHLD:
                        {
                            pid_t pid;
                            int stat;
                            while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                            {
                                int del_user = sub_process[pid];
                                sub_process[pid] = -1;
                                if (del_user < 0 || del_user >= USER_LIMIT)
                                    continue;
                                epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].connfd, 0);
                                close(users[del_user].pipefd[0]);
                                users[del_user] = users[--user_counter];
                                sub_process[users[user_counter].pid] = del_user;
                            }
                            if (terminate && user_counter == 0)
                            {
                                stop_server = true;
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:
                        {
                            printf("kill all child process\n");
                            if (user_counter == 0)
                            {
                                stop_server = true;
                                break;
                            }
                            for (int i = 0; i < user_counter; i++)
                            {
                                int pid = users[i].pid;
                                if (pid != -1)
                                {
                                    kill(pid, SIGTERM);
                                }
                            }
                            terminate = true;
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }
            // 处理子进程通过管道发来的消息
            else if (events[i].events & EPOLLIN)
            {
                int child = 0;
                ret = recv(sockfd, (char *)&child, sizeof(child), 0);
                printf("read data from child process pipe\n");
                if (ret <= 0)
                    continue;
                // 向除child进程外的所有子进程发送消息
                else
                {
                    for (int j = 0; j < user_counter; j++)
                    {
                        if (users[j].pipefd[0] != sockfd)
                        {
                            printf("send data to child process across pipe\n");
                            send(users[j].pipefd[0], (char *)&child, sizeof(child), 0);
                        }
                    }
                }
            }
        }
    }
    del_source();
    return 0;
}

/*
❑虽然我们使用了共享内存，但每个子进程都只会往自己所处理
的客户连接所对应的那一部分读缓存中写入数据，所以我们使用共享
内存的目的只是为了“共享读”。因此，每个子进程在使用共享内存的
时候都无须加锁。这样做符合“聊天室服务器”的应用场景，同时提高
了程序性能。
❑我们的服务器程序在启动的时候给数组users分配了足够多的空
间，使得它可以存储所有可能的客户连接的相关数据。同样，我们一
次性给数组sub_process分配的空间也足以存储所有可能的子进程的相
关数据。这是牺牲空间换取时间的又一例子。
*/