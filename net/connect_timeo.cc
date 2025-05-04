#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

int timeout_connect(const char *ip, int port, int time)
{
    int ret = 0;
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(ip);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    struct timeval timeout;
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    socklen_t len = sizeof(timeout);
    ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
    assert(ret != -1);
    ret = connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address));
    if (ret == -1)
    {
        if (errno == EINPROGRESS)
        {
            printf("connect timeout, process timeout logic\n");
            return -1;
        }
        printf("error occur when connecting to server: %s\n", strerror(errno));
        return -1;
    }
    return sockfd;
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
    int sockfd = timeout_connect(ip, port, 1000);
    if (sockfd < 0)
    {
        return 1;
    }
    close(sockfd);
    return 0;
}