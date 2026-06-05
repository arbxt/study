#include "handle/handle.h"

int main(int argc, char * argv[]) {
    wsx_config_t config = {0};
    int error_code = 0;
    if(-1 == init_config(&config)) { /* Read the configuration */
        exit(-1);
    }
    int sock_type = 0;
    int listenfd = open_listenfd(config.use_addr, config.listen_port, &sock_type);
    if(listenfd < 0) {
        fprintf(stderr, "listen fd is Error \n");
        return -1;
    }
    error_code = listen(listenfd, SOMAXCONN);
    if(-1 == error_code) {
        perror("Usage listen Queue: ");
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);
    handle_loop(listenfd, sock_type, &config);

    return 0;
}