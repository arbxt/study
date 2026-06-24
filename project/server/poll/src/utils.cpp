#include "utils.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>

bool setFdnonBlocking(int fd) {

  int flags = fcntl(fd, F_GETFL, 0);

  if (flags < 0) {
    std::cerr << "fcntl(F_GETFL) failed for fd " << fd << ": "
              << std::strerror(errno) << '\n';
    return false;
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    std::cerr << "fcntl(F_SETFL, O_NONBLOCK) failed for fd " << fd << ": "
              << std::strerror(errno) << '\n';
    return false;
  }

  return true;
}

ssize_t shortWrite(int fd, const void *buf, size_t n) {
  const char *ptr = static_cast<const char *>(buf);
  size_t total_written = 0;

  while (total_written < n) {
    ssize_t nwritten = write(fd, ptr + total_written, n - total_written);

    if (nwritten > 0) {
      total_written += static_cast<size_t>(nwritten);
      continue;
    }

    if (nwritten == 0) {
      return static_cast<ssize_t>(total_written);
    }

    // nwritten < 0
    if (errno == EINTR) {
      continue;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return static_cast<ssize_t>(total_written);
    }
    return -1;
  }

  return static_cast<ssize_t>(total_written);
}