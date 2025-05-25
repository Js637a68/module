#include "conn_pool.h"
#include "log.h"

ConnectionPool::ConnectionPool()
{
    if (!loadConfigFile())
        return;

    for (int i = 0; i < _initSize; i++)
    {
        Connection *p = new Connection();
        if (!p->connect(_ip, _port, _username, _password, _dbname))
            continue;

        p->resfreshAliveTime();
        _connQue.push(p);
        _connCnt++;
    }

    thread produce(std::bind(&ConnectionPool::produceConnectionTask, this));
    produce.detach();

    thread scanner(std::bind(&ConnectionPool::scannerConnectionTask, this));
    scanner.detach();
}

ConnectionPool *ConnectionPool::getConnectionPool()
{
    static ConnectionPool pool;
    return &pool;
}

shared_ptr<Connection> ConnectionPool::getConnection()
{
    unique_lock<mutex> lock(_queueMutex);
    while (_connQue.empty())
    {
        if (cv_status::timeout == _cv.wait_for(lock, chrono::milliseconds(_connectionTimeout)))
        {
            if (_connQue.empty())
            {
                LOG("获取空闲连接超时...获取连接失败!");
                // return nullptr;
            }
        }
    }

    shared_ptr<Connection> sp(_connQue.front(),
                              [&](Connection *pcon)
                              {
                                  unique_lock<mutex> lock(_queueMutex);
                                  pcon->resfreshAliveTime();
                                  _connQue.push(pcon);
                              });
    _connQue.pop();
    _cv.notify_all();
    return sp;
}

void ConnectionPool::produceConnectionTask()
{
    for (;;)
    {
        unique_lock<mutex> lock(_queueMutex);
        while (!_connQue.empty())
        {
            _cv.wait(lock);
            if (stop)
                return;
            /*
            原本主线程退出后，这里分离线程导致程序不能退出
            */
        }

        if (_connCnt < _maxSize)
        {
            Connection *p = new Connection();
            if (!p->connect(_ip, _port, _username, _password, _dbname))
                continue;
            p->resfreshAliveTime();
            _connQue.push(p);
            _connCnt++;
        }
        _cv.notify_all();
    }
}

void ConnectionPool::scannerConnectionTask()
{
    for (;;)
    {
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));
        unique_lock<mutex> lock(_queueMutex);
        while (_connCnt > _initSize)
        {
            Connection *p = _connQue.front();
            if (!p)
                return;
            if (p->getAliveeTime() >= _maxIdleTime * 1000)
            {
                _connQue.pop();
                _connCnt--;
                delete p;
            }
            else
            {
                break;
            }
        }
    }
}

bool ConnectionPool::loadConfigFile()
{
    FILE *pf = fopen("mysql.conf", "r");
    if (pf == nullptr)
    {
        LOG("mysql.conf file is not exist!");
        return false;
    }

    while (!feof(pf))
    {
        char line[1024] = {0};
        fgets(line, 1024, pf);
        string str = line;
        int idx = str.find('=', 0);
        if (idx == -1)
            continue;

        int endidx = str.find('\n', idx);
        string key = str.substr(0, idx);
        string value = str.substr(idx + 1, endidx - idx - 1);
        if (key == "ip")
        {
            _ip = value;
        }
        else if (key == "port")
        {
            _port = atoi(value.c_str());
        }
        else if (key == "username")
        {
            _username = value;
        }
        else if (key == "password")
        {
            _password = value;
        }
        else if (key == "dbname")
        {
            _dbname = value;
        }
        else if (key == "initSize")
        {
            _initSize = atoi(value.c_str());
        }
        else if (key == "maxSize")
        {
            _maxSize = atoi(value.c_str());
        }
        else if (key == "maxIdleTime")
        {
            _maxIdleTime = atoi(value.c_str());
        }
        else if (key == "connectionTimeOut")
        {
            _connectionTimeout = atoi(value.c_str());
        }
    }
    return true;
}