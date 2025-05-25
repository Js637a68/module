#include "db.h"
#include <muduo/base/Logging.h>

// 数据库操作类
MySQL::MySQL()
{
  loadConfigFile();
  _conn = mysql_init(nullptr);
}
// 释放数据库连接资源
MySQL::~MySQL()
{
  if (_conn != nullptr)
    mysql_close(_conn);
}

// 连接数据库
bool MySQL::connect()
{
  MYSQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(),
                                password.c_str(), dbname.c_str(), 3306, nullptr, 0);
  if (p != nullptr)
  {
    mysql_query(_conn, "set names gbk");
    LOG_INFO << "MySQL connect success!";
    return true;
  }
  else
  {
    string err = mysql_error(_conn);
    LOG_ERROR << "MySQL connect failed! : " + err;
    return false;
  }
}

bool MySQL::update(string sql)
{
  if (mysql_query(_conn, sql.c_str()) != 0)
  {
    LOG_ERROR << "MySQL update failed! : " + string(mysql_error(_conn));
    return false;
  }
  // mysql_free_result(mysql_use_result(_conn));
  return true;
}

MYSQL_RES *MySQL::query(string sql)
{
  if (mysql_query(_conn, sql.c_str()) != 0)
  {
    LOG_ERROR << "MySQL query failed! : " + string(mysql_error(_conn));
    return nullptr;
  }
  return mysql_use_result(_conn);
}

MYSQL *MySQL::getConnection()
{
  return _conn;
}

bool MySQL::loadConfigFile()
{
  FILE *pf = fopen("mysql.cnf", "r");
  if (pf == nullptr)
  {
    LOG_ERROR << "mysql.cnf file not found!";
    return false;
  }
  while (!feof(pf))
  {
    char line[1024] = {0};
    fgets(line, 1024, pf);
    string str = line;
    int idx = str.find("=", 0);
    if (idx == -1)
      continue;
    int endidx = str.find("\n", idx);
    string key = str.substr(0, idx);
    string value = str.substr(idx + 1, endidx - idx - 1);
    if (key == "ip")
      server = value;
    else if (key == "user")
      user = value;
    else if (key == "password")
      password = value;
    else if (key == "dbname")
      dbname = value;
    else if (key == "port")
      port = atoi(value.c_str());
  }
  return true;
}