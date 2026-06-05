#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int fd;
  fd = open(argv[1], O_RDWR);
  if (fd < 0) {
    perror("open failed");
    return -1;
  }
  void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED) {
    perror("mmap failed");
    return -1;
  }
  *((int *)p) = 42; // Example operation: write an integer to the mapped memory

  printf("File operation module loaded.\n");
  close(fd);
  munmap(p, 4096);
  return 0;
}