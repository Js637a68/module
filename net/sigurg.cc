#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#define BUFFER_SIZE 1024
static int connfd;

void sig_urg(int sig)
{
    int save_errno = errno;
    char buffer[BUFFER_SIZE];
    memset(buffer, '\0', BUFFER_SIZE);
    int ret = recv(connfd, buffer, BUFFER_SIZE - 1, MSG_OOB);
    printf("got %d bytes of oob data: %s\n", ret, buffer);
    errno = save_errno;
}
void addsig(int sig, void (*sig_handler)(int))
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char *argv[])
{
    if (argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip);
    address.sin_port = htons(port);
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    int ret = bind(sockfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(sockfd, 5);
    assert(ret != -1);
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    connfd = accept(sockfd, (struct sockaddr *)&client_address, &client_addrlength);
    if (connfd < 0)
    {
        printf("errno is: %d\n", errno);
    }
    else
    {
        addsig(SIGURG, sig_urg);
        //  使用SIGURG前，需要设置socket的宿主进程
        fcntl(connfd, F_SETOWN, getpid());
        char buf[BUFFER_SIZE];
        while (1)
        {
            memset(buf, '\0', BUFFER_SIZE);
            ret = recv(connfd, buf, sizeof(buf) - 1, 0);
            if (ret == -1)
            {
                printf("recv error\n");
                break;
            }
            if (ret == 0)
            {
                break;
            }
            printf("got %d bytes of normal data: %s\n", ret, buf);
        }
        close(connfd);
    }
    close(sockfd);
    return 0;
}
