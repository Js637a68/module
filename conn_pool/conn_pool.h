#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <iostream>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <memory>
#include <functional>
#include "conn.h"

class ConnectionPool
{
public:
    // 单例模式
    static ConnectionPool *getConnectionPool();
    shared_ptr<Connection> getConnection();
    ~ConnectionPool()
    {
        while (!_connQue.empty())
        {
            Connection *conn = _connQue.front();
            _connQue.pop();
            delete conn;
        }
        stop = true;
        _cv.notify_all();
    }

private:
    ConnectionPool();

    bool loadConfigFile();

    // 运行在独立的线程中，专门负责生产新连接
    void produceConnectionTask();

    // 扫描超过maxIdleTime时间的空闲连接，进行回收
    void scannerConnectionTask();

    string _ip;
    unsigned short _port;
    string _username;
    string _password;
    string _dbname;
    int _initSize;
    int _maxSize;
    int _maxIdleTime;
    int _connectionTimeout;

    bool stop = false;

    queue<Connection *> _connQue;
    mutex _queueMutex;
    atomic_int _connCnt;
    condition_variable _cv;
};