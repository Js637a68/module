// 半同步/半异步，进程池

#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H

#include <sys/types.h>
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
#include <sys/stat.h>

class process
{
public:
    process() : m_pid(-1) {}

public:
    pid_t m_pid;     // 子进程的ID
    int m_pipefd[2]; // 子进程和父进程之间的管道
};

template <typename T>
class processpool
{
private:
    processpool(int listenfd, int process_number = 0);

public:
    static processpool<T> *create(int listenfd, int process_number = 8)
    {
        if (!m_instance)
            m_instance = new processpool<T>(listenfd, process_number);
        return m_instance;
    }
    ~processpool()
    {
        delete[] m_sub_process;
    }
    void run();

private:
    void set_sig_pipe();
    void run_parent();
    void run_child();

private:
    static const int MAX_PROCESS_NUMBER = 16;  // 最大进程数
    static const int USER_PER_PROCESS = 65536; // 每个进程最多处理的用户连接数
    static const int MAX_EVENT_NUMBER = 10000; // 最大事件数
    int m_process_number;
    int m_idx; // 子进程在池中的序号
    int m_epollfd;
    int m_listenfd;
    int m_stop;
    process *m_sub_process; // 保存所有子进程的描述
    static processpool<T> *m_instance;
};

template <typename T>
processpool<T> *processpool<T>::m_instance = NULL;
static int sig_pipefd[2]; // 用于处理信号的管道，以实现统一事件源
static int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
static void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}
static void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}
static void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}
static void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
template <typename T>
processpool<T>::processpool(int listenfd, int process_number)
    : m_listenfd(listenfd), m_process_number(process_number), m_idx(-1), m_stop(false)
{
    assert((process_number > 0) && (process_number <= MAX_PROCESS_NUMBER));
    m_sub_process = new process[process_number];
    assert(m_sub_process);

    for (int i = 0; i < process_number; i++)
    {
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert(ret != -1);

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);
        if (m_sub_process[i].m_pid > 0)
        {
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else
        {
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}

template <typename T>
void processpool<T>::set_sig_pipe()
{
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd, sig_pipefd[0]);
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

template <typename T>
void processpool<T>::run()
{
    if (m_idx == -1)
    {
        run_parent();
    }
    else
    {
        run_child();
    }
}

template <typename T>
void processpool<T>::run_child()
{
    set_sig_pipe();
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    addfd(m_epollfd, pipefd);
    epoll_event events[MAX_EVENT_NUMBER];
    T *users = new T[USER_PER_PROCESS];
    assert(users);
    int number = 0;
    int ret = -1;
    while (!m_stop)
    {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if ((sockfd == pipefd) && (events[i].events & EPOLLIN))
            {
                int client = 0;
                ret = recv(sockfd, (char *)&client, sizeof(client), 0);
                if (((ret < 0) && (errno != EWOULDBLOCK)) || (ret == 0))
                {
                    continue;
                }
                else
                {
                    struct sockaddr_in client_address;
                    socklen_t client_addrlength = sizeof(client_address);
                    int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                    if (connfd < 0)
                    {
                        printf("errno is: %d\n", errno);
                    }
                    addfd(m_epollfd, connfd);
                    /*模板类T必须实现init方法，以初始化一个客户连接。我们直接使用connfd来索引
                    逻辑处理对象（T类型的对象），以提高程序效率*/
                    users[connfd].init(m_epollfd, connfd, client_address);
                }
            }
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0)
                {
                    continue;
                }
                else
                {
                    for (int j = 0; j < ret; j++)
                    {
                        switch (signals[j])
                        {
                        case SIGCHLD:
                        {
                            pid_t pid;
                            int stat;
                            while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                            {
                                continue;
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:
                        {
                            m_stop = true;
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
            else if (events[i].events & EPOLLIN)
            {
                users[sockfd].process();
            }
            else
            {
                continue;
            }
        }
    }
    delete[] users;
    users = NULL;
    close(pipefd);
    // close(m_listenfd);
    close(m_epollfd);
}

template <typename T>
void processpool<T>::run_parent()
{
    set_sig_pipe();
    addfd(m_epollfd, m_listenfd);
    epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;
    while (!m_stop)
    {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == m_listenfd)
            {
                // 如果有新连接到来，则轮询子进程来处理
                int i = sub_process_counter;
                do
                {
                    if (m_sub_process[i].m_pid != -1)
                        break;
                    i = (i + 1) % m_process_number;
                } while (i != sub_process_counter);
                if (m_sub_process[i].m_pid == -1)
                {
                    m_stop = true;
                    break;
                }
                sub_process_counter = (i + 1) % m_process_number;
                send(m_sub_process[i].m_pipefd[0], (char *)&new_conn, sizeof(new_conn), 0);
                printf("send request to child%d\n", i);
            }
            else if (sockfd == sig_pipefd[0] && events[i].events & EPOLLIN)
            {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0)
                {
                    continue;
                }
                else
                {
                    for (int j = 0; j < ret; j++)
                    {
                        switch (signals[j])
                        {
                        case SIGCHLD:
                        {
                            pid_t pid;
                            int stat;
                            while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) // WNOHANG: 如果子进程没有结束，则不阻塞，返回0
                            {
                                for (int k = 0; k < m_process_number; k++)
                                {
                                    // 如果第k个子进程退出，则关闭相应的管道，并重置m_pid
                                    if (m_sub_process[k].m_pid == pid)
                                    {
                                        printf("child %d join\n", k);
                                        close(m_sub_process[k].m_pipefd[0]);
                                        m_sub_process[k].m_pid = -1;
                                    }
                                }
                            }
                            m_stop = true;
                            for (int k = 0; k < m_process_number; k++)
                            {
                                if (m_sub_process[k].m_pid != -1)
                                {
                                    m_stop = false;
                                    break;
                                }
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:
                        {
                            /*如果父进程接收到终止信号，那么就杀死所有子进程，并等待它们全部结束。当然，
                            通知子进程结束更好的方法是向父、子进程之间的通信管道发送特殊数据，读者不妨自己实
                            现之*/
                            printf("kill all the child process\n");
                            for (int k = 0; k < m_process_number; k++)
                            {
                                int pid = m_sub_process[k].m_pid;
                                if (pid != -1)
                                {
                                    kill(pid, SIGTERM);
                                }
                            }
                        }
                        default:
                        {
                            break;
                        }
                        }
                    }
                }
            }
            else
            {
                continue;
            }
        }
    }
    close(m_epollfd);
    // close(m_listenfd);
}
#endif