#include <stdio.h>
#include <dlfcn.h> //动态链接
#include <error.h> //错误处理
#include <unistd.h> //
#include <stdlib.h> //malloc

int main() {
    // 动态链接示例
    /* void *p = dlopen("./dynamiclink/libdynamiclink.so", RTLD_LAZY);
    if (!p) {
        fprintf(stderr, "Error loading library: %s\n", dlerror());
        return 1;
    }
    printf("Library loaded successfully.\n");
    void (*func)() = dlsym(p, "print");
    if (!func) {
        fprintf(stderr, "Error finding symbol: %s\n", dlerror());
    }
    func();
    int (*add) (int,int) = (int (*)(int,int)) dlsym(p, "add");
    if (!add) {
        fprintf(stderr, "Error finding symbol: %s\n", dlerror());
    } else {
        printf("Addition result: %d\n", add(3, 5));
    }
    dlclose(p); */

    //错误处理示例
    printf("pagesize:%d\n", getpagesize());
    printf("pid:%d\n", getpid());
    void *p1 = malloc(4096 * 33);
    if( p1 == NULL) {
        perror("Memory allocation failed");
        return 1;
    }
    printf("p1:%p\n", p1);
    void *p2 = malloc(4096 * 34);
    printf("p2:%p\n", p2);
    // FILE *file = fopen("nonexistent.txt", "r");
    // if (file == NULL) {
    //     perror("Error opening file");
    //     return 1;
    // }
    // fclose(file);
    getchar(); //暂停程序以查看输出
    // free(p);
    return 0;
}