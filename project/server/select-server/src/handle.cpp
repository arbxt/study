#include "handle.h"

#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>

#include <errno.h>  // errno
#include <unistd.h> // read(), write(), close()

namespace {
std::mutex cout_mutex;
}

static void log_info(const std::string &message) {
  std::lock_guard<std::mutex> lock(cout_mutex);
  std::cout << message << std::endl;
}

static void log_error(const std::string &message) {
  std::lock_guard<std::mutex> lock(cout_mutex);
  std::cerr << message << std::endl;
}

static bool write_all(int fd, const char *buffer, ssize_t length) {
  ssize_t total_written = 0;

  while (total_written < length) {
    ssize_t written = write(fd, buffer + total_written, length - total_written);

    if (written > 0) {
      total_written += written;
    } else if (written == -1) {
      if (errno == EINTR) {
        /*
         * 被信号中断，可以重试。
         */
        continue;
      }

      return false;
    }
  }

  return true;
}

bool handle(int client_fd) {
  {
    std::string msg = "工作线程启动，thread id = " +
                      std::to_string(std::hash<std::thread::id>{}(
                          std::this_thread::get_id())) +
                      ", client_fd = " + std::to_string(client_fd);

    log_info(msg);
  }

  char buffer[1024];

  while (true) {
    std::memset(buffer, 0, sizeof(buffer));

    /*
     * read() 是阻塞操作。
     *
     * 如果客户端不发送数据，
     * 当前工作线程会阻塞在这里。
     *
     * 但是不会影响：
     *      1. 主线程继续 accept()
     *      2. 其他工作线程处理其他客户端
     */
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
      {
        std::string msg =
            "线程收到数据，client_fd = " + std::to_string(client_fd) +
            ", data = " + std::string(buffer, bytes_read);

        log_info(msg);
      }

      /*
       * Echo：
       * 把收到的数据原样写回客户端。
       */
      if (!write_all(client_fd, buffer, bytes_read)) {
        log_error("write 失败，client_fd = " + std::to_string(client_fd) +
                  ", error = " + std::strerror(errno));
        return false;
      }

    } else if (bytes_read == 0) {
      /*
       * read() 返回 0，表示客户端正常关闭连接。
       */
      log_info("客户端关闭连接，client_fd = " + std::to_string(client_fd));
      return false;

    } else {
      /*
       * read() 返回 -1，表示读取失败。
       */
      if (errno == EINTR) {
        /*
         * 被信号中断，可以继续读。
         */
        continue;
      }

      log_error("read 失败，client_fd = " + std::to_string(client_fd) +
                ", error = " + std::strerror(errno));
      return false;
    }
  }

  /*
   * client_fd 的所有权属于当前任务。
   *
   * 主线程 accept() 后把 client_fd 交给线程池任务，
   * 主线程不能 close(client_fd)。
   *
   * 当前任务结束时，由这里统一关闭。
   */
  close(client_fd);

  log_info("工作线程处理结束，client_fd = " + std::to_string(client_fd));
}