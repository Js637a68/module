#include "processpool.h"
class cgi_conn
{
public:
    cgi_conn() {}
    ~cgi_conn() {}
    void init(int epollfd, int sockfd, const sockaddr_in &client_addr)
    {
        m_epollfd = epollfd;
        m_sockfd = sockfd;
        m_address = client_addr;
        memset(m_buf, '\0', BUFFER_SIZE);
        m_read_idx = 0;
    }
    void process()
    {
        int idx = 0;
        int ret = -1;
        while (true)
        {
            idx = m_read_idx;
            ret = recv(m_sockfd, m_buf + idx, BUFFER_SIZE - 1 - idx, 0);
            if (ret < 0)
            {
                if (errno != EAGAIN)
                {
                    removefd(m_epollfd, m_sockfd);
                }
                break;
            }
            else if (ret == 0)
            {
                removefd(m_epollfd, m_sockfd);
                break;
            }
            else
            {
                m_read_idx += ret;
                printf("user content is: %s\n", m_buf);
                // 解析http请求行，获取请求方法、目标url及http版本号
                for (; idx < m_read_idx; ++idx)
                {
                    if ((idx >= 1) && m_buf[idx - 1] == '\r' && m_buf[idx] == '\n')
                    {
                        break;
                    }
                }
                // 如果没有遇到\r\n，则需要继续读取
                if (idx == m_read_idx)
                {
                    continue;
                }
                m_buf[idx - 1] = '\0';
                char *file_name = m_buf;
                if (access(file_name, F_OK) == -1)
                {
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                ret = fork();
                if (ret == -1)
                {
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                else if (ret > 0)
                {
                    wait(NULL);
                    removefd(m_epollfd, m_sockfd);
                    continue;
                }
                else
                {
                    close(STDOUT_FILENO);
                    dup(m_sockfd);
                    printf("exec: %s\n", m_buf);
                    execlp(m_buf, m_buf, 0, NULL);
                    exit(0);
                }
            }
        }
    }

private:
    static const int BUFFER_SIZE = 1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[BUFFER_SIZE];
    int m_read_idx;
};

int cgi_conn::m_epollfd = -1;

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

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);

    processpool<cgi_conn> *pool = processpool<cgi_conn>::create(listenfd);
    if (pool)
    {
        pool->run();
        delete pool;
    }
    close(listenfd);
    return 0;
}