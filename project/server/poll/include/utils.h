#pragma once

#include <sys/types.h>
#include <unistd.h>

bool setFdnonBlocking(int fd);
ssize_t shortWrite(int fd, const void *buf, size_t n);