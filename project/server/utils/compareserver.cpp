/*
 * multi_thread_server.cpp
 *
 * Linux / Ubuntu 环境下的多线程阻塞式 TCP 服务器。
 *
 * 功能：
 * 1. 主线程监听 8080 端口
 * 2. 主线程 accept() 阻塞等待客户端连接
 * 3. 每来一个客户端，创建一个 std::thread
 * 4. 工作线程负责 read() / write()
 * 5. 客户端断开后，工作线程关闭 client_fd 并结束
 *
 * 编译：
 *      g++ -Wall -Wextra -std=c++11 multi_thread_server.cpp -o multi_thread_server -pthread
 *
 * 运行：
 *      ./multi_thread_server
 *
 * 测试：
 *      nc 127.0.0.1 8080
 *
 * 可以打开多个终端同时测试。
 */

#include <iostream>      // std::cout, std::cerr
#include <thread>        // std::thread
#include <cstring>       // std::memset, std::strerror

#include <unistd.h>      // close(), read(), write()
#include <sys/types.h>   // ssize_t
#include <sys/socket.h>  // socket(), bind(), listen(), accept(), setsockopt()
#include <netinet/in.h>  // sockaddr_in
#include <arpa/inet.h>   // htons(), htonl(), inet_ntoa()
#include <errno.h>       // errno

/*
 * 处理客户端连接的线程函数。
 *
 * 参数：
 *      client_fd：已经 accept() 成功的客户端 socket。
 *
 * 这个函数会在工作线程中运行。
 */
void handle_client(int client_fd)
{
    /*
     * std::this_thread::get_id() 可以获取当前线程 ID。
     * 这不是 Linux 内核线程 ID，而是 C++ 标准库中的线程标识。
     */
    std::cout << "工作线程启动，thread id = "
              << std::this_thread::get_id()
              << std::endl;

    char buffer[1024];

    while (true) {
        /*
         * 每次读取前清空缓冲区。
         */
        std::memset(buffer, 0, sizeof(buffer));

        /*
         * read() 是阻塞函数。
         *
         * 如果客户端连接后一直不发送数据，
         * 当前工作线程会阻塞在这里。
         *
         * 但是主线程不会受影响，主线程仍然可以继续 accept()
         * 其他客户端连接。
         */
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0) {
            /*
             * 成功读取客户端数据。
             *
             * bytes_read 表示实际读取到的字节数。
             */
            std::cout << "线程 "
                      << std::this_thread::get_id()
                      << " 收到数据: "
                      << buffer
                      << std::endl;

            /*
             * Echo Server：
             * 客户端发什么，服务器原样返回什么。
             */
            ssize_t bytes_written = write(client_fd, buffer, bytes_read);

            if (bytes_written == -1) {
                std::cerr << "write 失败: "
                          << std::strerror(errno)
                          << std::endl;
                break;
            }

        } else if (bytes_read == 0) {
            /*
             * read() 返回 0：
             * 表示客户端正常关闭连接。
             */
            std::cout << "客户端关闭连接，线程 "
                      << std::this_thread::get_id()
                      << " 即将结束"
                      << std::endl;
            break;

        } else {
            /*
             * read() 返回 -1：
             * 表示读取失败。
             */
            std::cerr << "read 失败: "
                      << std::strerror(errno)
                      << std::endl;
            break;
        }
    }

    /*
     * 工作线程负责关闭 client_fd。
     *
     * 因为主线程把 client_fd 交给工作线程后，
     * 就不再使用这个 client_fd。
     */
    close(client_fd);

    std::cout << "工作线程结束，thread id = "
              << std::this_thread::get_id()
              << std::endl;
}

int main()
{
    /*
     * 第 1 步：创建监听 socket。
     *
     * socket() 返回一个文件描述符。
     *
     * AF_INET：
     *      IPv4
     *
     * SOCK_STREAM：
     *      TCP 字节流
     *
     * 0：
     *      让系统自动选择协议
     */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd == -1) {
        std::cerr << "socket 创建失败: "
                  << std::strerror(errno)
                  << std::endl;
        return 1;
    }

    /*
     * 第 2 步：设置端口复用。
     *
     * 如果服务器刚关闭，8080 端口可能暂时处于 TIME_WAIT 状态。
     * SO_REUSEADDR 可以让服务器更容易重新绑定该端口。
     */
    int opt = 1;

    if (setsockopt(
            listen_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) == -1) {
        std::cerr << "setsockopt 失败: "
                  << std::strerror(errno)
                  << std::endl;
        close(listen_fd);
        return 1;
    }

    /*
     * 第 3 步：准备服务器地址结构。
     *
     * sockaddr_in 是 IPv4 专用地址结构。
     */
    sockaddr_in server_addr;

    /*
     * 清空结构体，避免未初始化数据。
     */
    std::memset(&server_addr, 0, sizeof(server_addr));

    /*
     * 指定地址族为 IPv4。
     */
    server_addr.sin_family = AF_INET;

    /*
     * INADDR_ANY 表示监听本机所有网卡。
     *
     * htonl()：
     *      host to network long
     *      把主机字节序转换成网络字节序。
     */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /*
     * 监听 8080 端口。
     *
     * htons()：
     *      host to network short
     *      把主机字节序转换成网络字节序。
     */
    server_addr.sin_port = htons(8080);

    /*
     * 第 4 步：绑定 IP 和端口。
     */
    if (bind(
            listen_fd,
            reinterpret_cast<sockaddr*>(&server_addr),
            sizeof(server_addr)
        ) == -1) {
        std::cerr << "bind 失败: "
                  << std::strerror(errno)
                  << std::endl;
        close(listen_fd);
        return 1;
    }

    /*
     * 第 5 步：进入监听状态。
     *
     * 128 表示连接等待队列大小。
     */
    if (listen(listen_fd, 128) == -1) {
        std::cerr << "listen 失败: "
                  << std::strerror(errno)
                  << std::endl;
        close(listen_fd);
        return 1;
    }

    std::cout << "多线程阻塞服务器启动成功，监听端口 8080..."
              << std::endl;

    /*
     * 第 6 步：主线程循环 accept()。
     *
     * 主线程只负责接收新客户端连接。
     * 每接收到一个客户端，就创建一个工作线程。
     */
    while (true) {
        sockaddr_in client_addr;
        std::memset(&client_addr, 0, sizeof(client_addr));

        socklen_t client_addr_len = sizeof(client_addr);

        /*
         * accept() 是阻塞函数。
         *
         * 如果没有客户端连接，主线程会阻塞在这里。
         */
        int client_fd = accept(
            listen_fd,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_addr_len
        );

        if (client_fd == -1) {
            /*
             * 如果 accept() 出错，本次连接失败。
             * 服务器继续等待下一个客户端。
             */
            std::cerr << "accept 失败: "
                      << std::strerror(errno)
                      << std::endl;
            continue;
        }

        std::cout << "客户端连接成功: "
                  << inet_ntoa(client_addr.sin_addr)
                  << ":"
                  << ntohs(client_addr.sin_port)
                  << std::endl;

        /*
         * 第 7 步：创建工作线程。
         *
         * std::thread worker(handle_client, client_fd);
         *
         * 含义：
         *      创建一个新线程
         *      新线程执行 handle_client(client_fd)
         */
        std::thread worker(handle_client, client_fd);

        /*
         * 第 8 步：线程分离。
         *
         * detach() 表示让线程在后台独立运行。
         *
         * 分离后：
         *      主线程不需要 join() 等待它
         *      工作线程结束后，系统会自动回收线程资源
         *
         * 为什么这里要 detach()？
         *
         * 因为主线程要立刻回到 accept()，
         * 继续接收其他客户端。
         *
         * 如果这里调用 worker.join()，
         * 主线程会等待当前客户端处理完毕，
         * 那就又退化成单客户端阻塞服务器了。
         */
        worker.detach();

        /*
         * 注意：
         *      主线程这里不要 close(client_fd)。
         *
         * 多进程版本中，父进程 fork() 后需要 close(client_fd)，
         * 因为父子进程有各自的文件描述符表。
         *
         * 但多线程共享同一个进程的文件描述符表。
         *
         * 如果主线程在这里 close(client_fd)，
         * 工作线程中的 client_fd 也会失效。
         *
         * 所以本例中 client_fd 由工作线程负责关闭。
         */
    }

    /*
     * 理论上不会执行到这里。
     */
    close(listen_fd);

    return 0;
}