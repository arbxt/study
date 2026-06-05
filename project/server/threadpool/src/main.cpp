#include "handle.h"
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

  while (true) {
    /*
     * accept_client() 是阻塞函数。
     *
     * 如果没有客户端连接，主线程会停在这里。
     */
    int client_fd = server.accept_client();

    if (client_fd == -1) {
      /*
       * accept 出错时继续等待下一个连接。
       */
      continue;
    }

    /*
     * 把客户端连接封装成任务，提交给线程池。
     *
     * [client_fd]() {
     *     handle(client_fd);
     * }
     *
     * 这里使用值捕获 client_fd。
     *
     * 很重要：
     *      不能使用引用捕获 [&]。
     *
     * 因为 client_fd 是循环中的局部变量。
     * 如果引用捕获，多个任务可能引用同一个变量，
     * 会产生严重错误。
     */
    pool.submit([client_fd]() { handle(client_fd); });
  }

  return 0;
}