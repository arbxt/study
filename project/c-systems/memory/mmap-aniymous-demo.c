#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

int main() {
    void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }
    printf("success mmap at %p\n", p);
    strcpy(p, "Hello, mmap!");
    printf("Content: %s\n", (char *)p);
    munmap(p, 4096);
    return 0;
}