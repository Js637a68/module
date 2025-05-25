#include <iostream>
#include "conn_pool.h"
int opg = 2;
int twice = 250;
void done(Connection *conn, const char *name, int age, const char *sex, int op = opg)
{
    char sql[1024] = {};
    if (op & 1)
    {
        sprintf(sql, "insert into user(name,age,sex) values('%s','%d','%s')",
                name, age, sex);
        if (conn->update(sql))
            ;
        // cout << "update success" << endl;
    }
    if (op & 2)
    {
        sprintf(sql, "select * from user");
        MYSQL_RES *res;
        if (res = conn->query(sql))
            ;
        // cout << "query success" << endl;
        mysql_free_result(res);
    }
}

void test()
{
    clock_t start = clock();
    thread t1([]()
              {
        for (int i = 0; i < twice; ++i)
        {
            Connection conn;
            if (!conn.connect("127.0.0.1", 3306, "git", "git", "chat"))
            {
                cout << "connect error" << endl;
                return;
            }
			done(&conn, "zhang san", 20, "male");
		} });

    thread t2([]()
              {
        for (int i = 0; i < twice; ++i)
        {
            Connection conn;
            if (!conn.connect("127.0.0.1", 3306, "git", "git", "chat"))
            {
                cout << "connect error" << endl;
                return;
            }
            done(&conn, "zhang san", 20, "male");
            
        } });

    thread t3([]()
              {
        for (int i = 0; i < twice; ++i)
        {
            Connection conn;
            if (!conn.connect("127.0.0.1", 3306, "git", "git", "chat"))
            {
                cout << "connect error" << endl;
                return;
            }
            (&conn, "zhang san", 20, "male");
            
        } });
    thread t4([]()
              {
        for (int i = 0; i < twice; ++i)
        {
            Connection conn;
            if (!conn.connect("127.0.0.1", 3306, "git", "git", "chat"))
            {
                cout << "connect error" << endl;
                return;
            }
            done(&conn, "zhang san", 20, "male");
            
        } });

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    clock_t total = clock() - start;
    printf("twice op: %d per thread cost %ld ms\n", opg, total / 1000);
}

void test_pool()
{
    clock_t start = clock();
    thread t1([]()
              {
        ConnectionPool *cp = ConnectionPool::getConnectionPool();
        for (int i = 0; i < twice; ++i)
        {
            shared_ptr<Connection> sp = cp->getConnection();
            done(sp.get(), "zhang san", 20, "male");
		} });
    thread t2([]()
              {
        ConnectionPool *cp = ConnectionPool::getConnectionPool();
        for (int i = 0; i < twice; ++i)
        {
            shared_ptr<Connection> sp = cp->getConnection();
            done(sp.get(), "zhang san", 20, "male");
        } });

    thread t3([]()
              {
        ConnectionPool *cp = ConnectionPool::getConnectionPool();
        for (int i = 0; i < twice; ++i)
        {
            shared_ptr<Connection> sp = cp->getConnection();
            done(sp.get(), "zhang san", 20, "male");
            
        } });
    thread t4([]()
              {
        ConnectionPool *cp = ConnectionPool::getConnectionPool();
        for (int i = 0; i < twice; ++i)
        {
            shared_ptr<Connection> sp = cp->getConnection();
            done(sp.get(), "zhang san", 20, "male");
        } });

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    clock_t total = clock() - start;
    printf("%d twice op: %d per thread on 4 threads cost %ld ms\n", twice, opg, total / 1000);
}

int main()
{
    opg = 3;
    twice = 2500;
    test();
    test_pool();
}