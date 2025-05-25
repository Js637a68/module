#include "chatserver.h"
#include "json.hpp"
#include "chatservice.h"

#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
  // 注册链接回调
  _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
  // 注册消息回调
  _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));
  // 设置线程数量
  _server.setThreadNum(4);
}

void ChatServer::start()
{
  _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
  if (!conn->connected())
  {
    ChatService::instance()->clientCloseException(conn);
    conn->shutdown();
  }
}

void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{
  string buf = buffer->retrieveAllAsString();
  json js = json::parse(buf);
  // 完全解耦网络模块的代码和业务模块的代码
  // 通过js["msgid"]获取业务handler
  auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
  msgHandler(conn, js, time);
}