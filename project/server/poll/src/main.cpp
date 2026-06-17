#include "poller.h"
#include "server.h"

#include <iostream>

/*
 * 主线程负责：
 *      1. 创建服务器
 *      2. 创建线程池
 *      3. accept() 客户端连接
 *      4. 把客户端连接提交给线程池
 */
int main() {
  const unsigned short port = 8080;

  TcpServer server(port);

  if (!server.start()) {
    std::cerr << "服务器启动失败" << std::endl;
    return 1;
  }

  Poller poller(server);

  poller.loop();

  return 0;
}