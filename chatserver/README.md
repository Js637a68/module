消息转发流程：
客户端发送消息 -> 服务端接收消息
  1. 检查连接是否存在 -> 直接转发
  2. 不存在则检查redis订阅通道 -> 发布订阅
  3. 说明对方离线 -> 存储消息
对方上线：
  服务端请求mysql，发送离线信息，服务端删除离线信息

TODO:
1. 实现拉黑功能：
  加入逻辑层对消息处理，转发 

2. 确保消息可靠传递：
  - 为每个用户的每个消息分配一个唯一的seq
  - 消息先写入存储层，同时记录着客户端确认的最大seq
  - 客户端收到消息更新最大seq，向服务端确认
  - 服务端收到确认更新客户端最大seq，并且删除存储层的消息
  - 多个设备登时，服务端以最新的seq为准


