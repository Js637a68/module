#include "log.h"
#include "conn.h"
#include <iostream>

Connection::Connection()
{
    _conn = mysql_init(nullptr);
}

Connection::~Connection()
{
    if (_conn != nullptr)
        mysql_close(_conn);
}

bool Connection::connect(string ip, unsigned short port,
                         string user, string password, string dbname)
{
    MYSQL *p = mysql_real_connect(_conn, ip.c_str(), user.c_str(),
                                  password.c_str(), dbname.c_str(), port, nullptr, 0);
    if (!p)
        cout << mysql_error(_conn) << endl;
    return p != nullptr;
}

bool Connection::update(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        string fail = mysql_error(_conn);
        LOG("更新失败：" + sql + fail);
        return false;
    }
    mysql_free_result(mysql_use_result(_conn));
    return true;
}

MYSQL_RES *Connection::query(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        string fail = mysql_error(_conn);
        LOG("查询失败：" + sql + fail);
        return nullptr;
    }
    return mysql_use_result(_conn);
}