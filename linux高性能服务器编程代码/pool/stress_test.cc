#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static const char *request = "GET http://localhost/index.html HTTP/1.1\r\nConnection:kepp-alive\r\n\r\nxxxxxxxxxxx";
int setnoblocking(int fd)
{
    int old = fcntl(fd, F_GETFL);
    int newop = old | SOCK_NONBLOCK;
    fcntl(fd, F_SETFL, newop);
    return old;
}
void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnoblocking(fd);
}
bool write_nbytes(int sockfd, const char *buff, int len)
{
    int bytes_write = 0;
    printf("write out %d byte to socket%d\n", len, sockfd);
    while (1)
    {
        bytes_write = send(sockfd, buff, len, 0);
        if (bytes_write <= 0)
            return false;
        len -= bytes_write;
        buff += bytes_write;
        if (len <= 0)
            return true;
    }
}
bool read_once(int sockfd, char *buff, int len)
{
    int byte_read = 0;
    byte_read = recv(sockfd, buff, len, 0);
    if (byte_read <= 0)
        return false;
    printf("read in%d bytes from socket%d withcontent:%s\n", byte_read, sockfd, buff);
    return true;
}

void start_conn(int epoll_fd, int num, const char *ip, int port)
{
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    for (int i = 0; i < num; i++)
    {
        sleep(1);
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        printf("create 1 sock\n");
        if (sockfd < 0)
            continue;
        if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == 0)
        {
            printf("build connection%d\n", i);
            addfd(epoll_fd, sockfd);
        }
    }
}

void close_conn(int epoll_fd, int sockfd)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);
}

int main(int argc, char *argv[])
{
    assert(argc == 4);
    int epoll_fd = epoll_create(100);
    int num = atoi(argv[3]);
    char *ip = argv[1];
    int port = atoi(argv[2]);
    start_conn(epoll_fd, num, ip, port);
    epoll_event events[10000];
    char buff[2048];
    while (1)
    {
        int fds = epoll_wait(epoll_fd, events, 10000, 2000);
        for (int i = 0; i < fds; i++)
        {
            int sockfd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {
                if (!read_once(sockfd, buff, 2048))
                    close(sockfd);
                struct epoll_event event;
                event.data.fd = sockfd;
                event.events = EPOLLOUT | EPOLLERR | EPOLLET;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event);
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!write_nbytes(sockfd, request, strlen(request)))
                    close_conn(epoll_fd, sockfd);
                struct epoll_event event;
                event.data.fd = sockfd;
                event.events = EPOLLIN | EPOLLERR | EPOLLET;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event);
            }
            else if (events[i].events & EPOLLERR)
                close_conn(epoll_fd, sockfd);
        }
    }
}