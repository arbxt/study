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

void handle_business(int client_fd, const std::string &data) {
  {
    std::string msg = "工作线程启动，thread id = " +
                      std::to_string(std::hash<std::thread::id>{}(
                          std::this_thread::get_id())) +
                      ", client_fd = " + std::to_string(client_fd);

    log_info(msg);
  }

  std::string msg = "线程收到数据，client_fd = " + std::to_string(client_fd) +
                    ", data = " + std::string(data);

  log_info(msg);

  /*
   * Echo：
   * 把收到的数据原样写回客户端。
   */
  if (!write_all(client_fd, data.c_str(), data.size())) {
    log_error("write 失败，client_fd = " + std::to_string(client_fd) +
              ", error = " + std::strerror(errno));
  }

  log_info("工作线程处理结束，client_fd = " + std::to_string(client_fd));
}