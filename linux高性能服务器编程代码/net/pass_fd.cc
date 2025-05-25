#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
static const int CONTROL_LEN = CMSG_LEN(sizeof(int));

void send_fd(int fd, int fd_to_send)
{
    struct iovec iov[1];
    struct msghdr msg;
    char buf[0];
    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    cmsghdr cm;
    cm.cmsg_len = CONTROL_LEN;
    cm.cmsg_level = SOL_SOCKET;
    cm.cmsg_type = SCM_RIGHTS;
    *(int *)CMSG_DATA(&cm) = fd_to_send;
    msg.msg_control = &cm;
    msg.msg_controllen = CONTROL_LEN;
    sendmsg(fd, &msg, 0);
}

int recv_fd(int sock_fd)
{
    struct iovec iov[1];
    struct msghdr msg;
    char buf[0];
    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    cmsghdr cm;
    msg.msg_control = &cm;
    msg.msg_controllen = CONTROL_LEN;
    recvmsg(sock_fd, &msg, 0);
    int fd = *(int *)CMSG_DATA(&cm);
    return fd;
}

int main()
{
    int pipe_fd[2];
    int fd_to_pass;
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipe_fd);
    assert(ret != -1);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0)
    {
        close(pipe_fd[0]);
        fd_to_pass = open("test.txt", O_RDWR, 0666);
        send_fd(pipe_fd[1], (fd_to_pass >= 0) ? fd_to_pass : 0);
        close(fd_to_pass);
        exit(0);
    }
    else
    {
        close(pipe_fd[1]);
        fd_to_pass = recv_fd(pipe_fd[0]);
        char buf[1024];
        memset(buf, '\0', sizeof(buf));
        read(fd_to_pass, buf, sizeof(buf));
        printf("I got fd %d and data:%s\n", fd_to_pass, buf);
        close(fd_to_pass);
        waitpid(pid, NULL, 0);
    }
    return 0;
}