#include "handle.h"
#include "selector.h"
#include "server.h"
#include "threadpool.h"

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

  /*
   * 线程池大小。
   *
   * std::thread::hardware_concurrency()
   * 可以获取硬件支持的并发线程数量。
   *
   * 对于阻塞式 I/O 服务器，
   * 线程数可以适当大于 CPU 核心数。
   *
   * 这里设置为 4。
   */
  const size_t thread_count = 4;

  TcpServer server(port);

  if (!server.start()) {
    std::cerr << "服务器启动失败" << std::endl;
    return 1;
  }

  ThreadPool pool(thread_count);

  std::cout << "线程池启动成功，工作线程数量 = " << thread_count << std::endl;

  Selector selector(server, pool);

  selector.loop();

  return 0;
}