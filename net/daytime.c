#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>

int main(int argc, char *argv[])
{
    assert(argc == 2);

    const char *ip = argv[1];
    struct hostent *hostinfo = gethostbyname(ip);
    struct servent *servinfo = getservbyname("daytime", "tcp");
    if (!hostinfo || !servinfo)
    {
        printf("gethostbyname or getservbyname error\n");
        exit(1);
    }
    printf("daytime port is %d\n", ntohs(servinfo->s_port));

    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = servinfo->s_port;
    server_address.sin_addr = *(struct in_addr *)hostinfo->h_addr_list;
    printf("server address: %s\n", inet_ntoa(server_address.sin_addr));
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    int ret = connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address));
    assert(ret != -1);
    char buffer[128];
    ret = read(sockfd, buffer, sizeof(buffer) - 1);
    buffer[ret] = '\0';
    printf("the day time is %s\n", buffer);
    close(sockfd);
    return 0;
}