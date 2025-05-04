#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string>

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