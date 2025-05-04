#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <libgen.h>
#define BUFFER_SIZE 512
char *ip;
int port;
int sendbuf, recvbuf;

void *server();
void client();

int main(int argc, char *argv[])
{
    if (argc <= 4)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
    }
    ip = argv[1];
    port = atoi(argv[2]);
    recvbuf = atoi(argv[3]);
    sendbuf = atoi(argv[4]);
    pthread_t tid;
    pthread_create(&tid, NULL, server, NULL);
    sleep(1);
    client();
    pthread_join(tid, NULL);
    return 0;
}

void *server()
{
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    int len = sizeof(recvbuf);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof(recvbuf));
    getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recvbuf, (socklen_t *)&len);
    printf("the tcp receive buffer size after setting is %d\n", recvbuf);
    int ret = bind(sockfd, (struct sockaddr *)&server_address, sizeof(server_address));
    if (ret == -1)
    {
        printf("bind error: %s\n", strerror(errno));
        assert(ret != -1);
    }
    ret = listen(sockfd, 5);
    assert(ret != -1);

    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int connfd = accept(sockfd, (struct sockaddr *)&client_address, &client_addrlength);
    if (connfd < 0)
    {
        printf("errno is: %d\n", errno);
    }
    else
    {
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE - 1);
        while (recv(connfd, buffer, BUFFER_SIZE - 1, 0) > 0)
        {
        }
        close(connfd);
    }
    close(sockfd);
}

void client()
{
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    int len = sizeof(sendbuf);
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));
    getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, (socklen_t *)&len);
    printf("the tcp send buffer size after setting is %d\n", sendbuf);
    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) != -1)
    {
        char buffer[BUFFER_SIZE];
        memset(buffer, 'a', BUFFER_SIZE - 1);
        send(sockfd, buffer, BUFFER_SIZE, 0);
        close(sockfd);
    }
    close(sockfd);
}