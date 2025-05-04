#ifndef HTTPCONNTION_H
#define HTTPCONNTION_H

#include <unistd.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include "../IPC/locker.h"
class http_conn
{
public:
    // 文件名最大长度
    static const int FILENAME_LEN = 200;
    // 读缓冲区大小
    static const int READ_BUFFER_SIZE = 2048;
    // 写缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;
    // 客户请求方法，仅实现GET
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };
    // 解析客户请求时，主状态机所处的状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    // 服务器处理HTTP请求的可能结果，报文解析的结果
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    // 行的读取状态
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}
    void init(int sockfd, const sockaddr_in &addr);
    void close_conn(bool real_close = true);
    void process();
    bool read();
    bool write();

private:
    void init();
    // 解析HTTP请求行，获得请求方法，目标URL及HTTP版本号
    HTTP_CODE process_read();
    // 填充HTTP应答
    bool process_write(HTTP_CODE ret_code);
    //
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();
    //
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_length(int value);
    bool add_linger();
    bool add_blank_line();

public:
    // 所有socket上的事件都被注册到同一个epoll内核事件中，所以设置成静态的
    static int m_epollfd;
    //
    static int m_user_count;

private:
    // client的socket和地址
    int m_sockfd;
    sockaddr_in m_address;

    // 读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;

    // 写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;

    // 主状态机当前所处的状态
    CHECK_STATE m_check_state;
    // 请求行
    METHOD m_method;
    /*客户请求的目标文件的完整路径，其内容等于doc_root+m_url，doc_root是网站
    根目录*/
    char m_real_file[FILENAME_LEN];
    // 客户请求的目标文件的文件名
    char *m_url;
    // HTTP版本号
    char *m_version;
    // 主机名
    char *m_host;
    // http请求消息长度
    int m_content_length;
    // 请求是否长连接
    bool m_linger;
    // 客户请求的资源文档必须完整路径，该文档的内存映射区域
    char *m_file_address;
    // 目标文件的状态。通过stat函数获取，成功为0，失败为-1
    struct stat m_file_stat;
    /*我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示
    被写内存块的数量*/
    struct iovec m_iv[2];
    int m_iv_count;
};
#endif