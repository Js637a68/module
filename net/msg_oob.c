#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#include <pthread.h>
#define BUFFER_SIZE 1024

char *ip;
int port;

void *oob_server();
void oob_client();
int main(int argc, char *argv[])
{
    if (argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    ip = argv[1];
    port = atoi(argv[2]);

    // pthread_t tid;
    // pthread_create(&tid, NULL, oob_server, NULL);
    // oob_client();
    // pthread_join(tid, NULL);
    oob_client();

    return 0;
}

void *oob_server()
{
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    int ret = bind(sockfd, (struct sockaddr *)&server_address, sizeof(server_address));
    assert(ret != -1);

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
        memset(buffer, '\0', BUFFER_SIZE);
        sleep(1);
        ret = recv(connfd, buffer, BUFFER_SIZE - 1, 0);
        printf("got %d bytes of normal data '%s'\n", ret, buffer);

        memset(buffer, '\0', BUFFER_SIZE);
        ret = recv(connfd, buffer, BUFFER_SIZE - 1, MSG_OOB);
        printf("got %d bytes of oob data '%s'\n", ret, buffer);

        memset(buffer, '\0', BUFFER_SIZE);
        sleep(1);
        ret = recv(connfd, buffer, BUFFER_SIZE - 1, 0);
        printf("got %d bytes of normal data '%s'\n", ret, buffer);

        close(connfd);
    }
    close(sockfd);
}

void oob_client()
{
    sleep(2);
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        printf("connection failed\n");
    }
    else
    {
        const char *oob_data = "abc";
        const char *normal_data = "123";
        send(sockfd, normal_data, strlen(normal_data), 0);
        send(sockfd, oob_data, strlen(oob_data), MSG_OOB);
        send(sockfd, normal_data, strlen(normal_data), 0);
        send(sockfd, oob_data, strlen(oob_data), 0);
        send(sockfd, normal_data, strlen(normal_data), 0);
    }
    close(sockfd);
}