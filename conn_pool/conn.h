#pragma once
#include <mysql/mysql.h>
#include <string>
#include <ctime>
using namespace std;

class Connection
{
public:
    Connection();
    ~Connection();
    bool connect(string ip,
                 unsigned short port,
                 string user,
                 string password,
                 string dbname);

    bool update(string sql);
    MYSQL_RES *query(string sql);

    // 刷新一下连接的起始的空闲时间点
    void resfreshAliveTime() { _alivetime = clock(); }
    // 返回存活的时间
    clock_t getAliveeTime() const { return clock() - _alivetime; }

private:
    MYSQL *_conn;       // 表示和MySQL Server的一条连接
    clock_t _alivetime; // 记录进入空闲状态后的起始存活时间
};