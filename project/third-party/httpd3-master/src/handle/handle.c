//
// Created by root on 3/17/16.
//

#include "handle.h"
#include <linux/tcp.h>
#include <pthread.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/mman.h>
char * cache_file = NULL;

enum SERVE_STATUS {
    CLOSE_SERVE = 1,
    RUNNING_SERVER= 0
};
static enum SERVE_STATUS terminal_server = RUNNING_SERVER;
char * website_root_path = NULL;
static int * epfd_group = NULL;  /* Workers' epfd set */
static int   epfd_group_size = 0; /* Workers' epfd set size */
static int   workers = 0;         /* Number of Workers */
/* TODO
 * Will be use in Multi-Process */
static int   listeners = MAX_LISTEN_EPFD_SIZE; /* Number of Listenner */
static conn_client * clients;   /* Client set */

/* Add new Socket to the epfd
 * */
static inline void add_event(int epfd, int fd, int event_flag) {
    struct epoll_event event = {0};
    event.data.fd = fd;
    event.events = event_flag | EPOLLET | EPOLLRDHUP;
    if (-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event)) {
        perror("epoll_ctl ADD: ");
        exit(-1);
    }
}
/*
 * Modify including Re-register Each sock while it woke up by epfd
 * */
static inline void mod_event(int epfd, int fd, int event_flag) {
    struct epoll_event event = {0};
    event.data.fd = fd;
    event.events |=  event_flag;
    if (-1 == epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event)) {
        perror("epoll_ctl MOD: ");
        exit(-1);
    }
}
/*
 * Prepare Some Resources For Workers
 * Worker epfd set and client set
 * */
static void prepare_workers(const wsx_config_t * config) {
    char file[256] = {'\0'};
    snprintf(file, 256, "%s%s", config->root_path, "index.html");
    int fd = open(file, O_RDONLY);
    assert(fd > 0);
    cache_file = mmap(NULL, 51, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(cache_file != NULL);
    epfd_group_size = workers;
    /* Prepare for workers' set */
    epfd_group = Malloc(epfd_group_size * (sizeof(int)));
    if (NULL == epfd_group) {
#if defined(WSX_DEBUG)
        fputs("Malloc Fail!", stderr);
#endif
        exit(-1);
    }
    for (int i = 0; i < epfd_group_size; ++i) {
        epfd_group[i] = epoll_create1(0);
        if (-1 == epfd_group[i]) {
#if defined(WSX_DEBUG)
            fprintf(stderr, "Error on epoll\n");
#endif
            exit(-1);
        }
        /* TODO
         * This Part will be replace with a Special Data Structure
         * SkipList, which is aims to Store/Maintain the each 'conn_client's.
         * */
        clients = Malloc(OPEN_FILE * sizeof(conn_client));
        if (NULL == clients) {
            exit(-1);
        } else{
            for (int j = 0; j < OPEN_FILE; ++j) {
                clients[j].r_buf = MAKE_STRING_S("");
                clients[j].w_buf = MAKE_STRING_S("");
                clients[j].conn_res.requ_res_path = MAKE_STRING_S("");
            }
        }

        website_root_path = Malloc(strlen(config->root_path) + 1);
        if (NULL == website_root_path) {
#if defined(WSX_DEBUG)
            fputs("Malloc Fail!", stderr);
#endif
            exit(-1);
        }
        else {
            strncpy(website_root_path, config->root_path, strlen(config->root_path));
        }
    }
}

static inline void destroy_resouce() {
    Free(epfd_group);
    Free(clients);
    Free(website_root_path);
}
static void clear_clients(conn_client * clear) {
    clear->file_dsp = -1;
    clear->conn_res.conn_linger = 0;
    //- clear->read_offset = 0;
    clear->r_buf_offset = 0;
    clear->w_buf_offset = 0;
    clear_string(clear->r_buf);
    clear_string(clear->w_buf);
    //clear->r_buf->use->clear(clear->r_buf);
    //clear->w_buf->use->clear(clear->w_buf);
    clear_string(clear->conn_res.requ_res_path);
    //clear->conn_res.requ_res_path->use->clear(clear->conn_res.requ_res_path);
}

/* Listener's Thread
 * @param arg will be a epoll instance
 * */
static void * listen_thread(void * arg) {
    int listen_epfd = *(int *)arg;
    struct epoll_event new_client = {0};
    /* Adding new Client Sock to the Workers' thread */
    int balance_index = 0;
    while (terminal_server != CLOSE_SERVE) {
        int is_work = epoll_wait(listen_epfd, &new_client, 1, 2000);
        int sock = 0;
        /* New Connect */
        while (is_work > 0) {
            sock = accept(new_client.data.fd, NULL, NULL);
            if (sock > 0) {
#if defined(WSX_BASE_DEBUG)
                fprintf(stderr, "There has a client(%d) Connected\n", sock);
#endif
                set_nonblock(sock);
                clear_clients(&clients[sock]);
                clients[sock].file_dsp = sock;
                add_event(epfd_group[balance_index], sock, EPOLLIN);
                balance_index = (balance_index+1) % workers;
            } else /* sock == -1 means nothing to accept */
                break;
        } /* new Connect */
    }/* main while */
    close(listen_epfd);

    pthread_exit(0);
}

/* Workers' Thread
 * @param arg will be a epoll instance
 * */
static void * workers_thread(void * arg) {
    int deal_epfd = *(int *)arg;
    struct epoll_event new_apply = {0};
    while(terminal_server != CLOSE_SERVE) {
        int is_apply = epoll_wait(deal_epfd, &new_apply, 1, 2000);
        if(is_apply > 0) { /* New Apply */
            int sock = new_apply.data.fd;
            conn_client * new_client = &clients[sock];
#if defined(WSX_BASE_DEBUG)
            fprintf(stderr, "The thread %d receive the client(%d)\n", pthread_self(), sock);
#endif
            if (new_apply.events & EPOLLIN) { /* Reading Work */
                int err_code = handle_read(new_client);
                if (err_code == MESSAGE_INCOMPLETE) {
                    /* Some Message may not arrive */
#if defined(WSX_DEBUG)
                    fprintf(stderr, "READ FROM NEW CLIENT But Do not complete\n");

#endif
                    mod_event(deal_epfd, sock, EPOLLIN);
                    continue;
                }
                else if (err_code != HANDLE_READ_SUCCESS) {
                    /* Read Bad Things */
#if defined(WSX_DEBUG)
                    fprintf(stderr, "READ FROM NEW CLIENT FAIL\n");
#endif
                    close(sock);
                    continue;
                }

                /* Try to Send the Message immediately */
                err_code = handle_write(new_client);
                if (HANDLE_WRITE_AGAIN == err_code) {
                    /* TCP Write Buffer is Full */
                    mod_event(deal_epfd, sock, EPOLLOUT);
                    continue;
                }
                else if (HANDLE_WRITE_FAILURE == err_code) {
                    /* Peer Close */
                    close(sock);
                    continue;
                }
                else {
                    /* Write Success */
                    if(1 == new_client->conn_res.conn_linger)
                        mod_event(deal_epfd, sock, EPOLLIN);
                    else{
                        close(sock);
                        continue;
                    }
                }
#if 0
                mod_event(deal_epfd, sock, EPOLLONESHOT | EPOLLOUT);
                continue;
#endif
#if defined(WSX_DEBUG)
                fprintf(stderr, "Client(%d)Read For Writing!!: \n\n%s",new_client->file_dsp, new_client->w_buf->str);
#endif
            } // Read Event
            else if (new_apply.events & EPOLLOUT) { /* Writing Work */
                int err_code = handle_write(new_client);
                if (HANDLE_WRITE_AGAIN == err_code) /* TCP's Write buffer is Busy */
                    mod_event(deal_epfd, sock, EPOLLOUT);
                else if (HANDLE_READ_FAILURE == err_code){ /* Peer Close */
                    close(sock);
                    continue;
                }
                /* if Keep-alive */
                if(1 == new_client->conn_res.conn_linger)
                    mod_event(deal_epfd, sock, EPOLLIN);
                else{
                    close(sock);
                    continue;
                }
            }
            else { /* EPOLLRDHUG EPOLLERR EPOLLHUG */
                close(sock);
            }
        } /* New Apply */
    } /* main while */
    return (void*)0;
}
static void shutdowns(int arg) {
    terminal_server = CLOSE_SERVE;
    return;
}
/* The Http server Main "Loop" */
void handle_loop(int file_dsption, int sock_type, const wsx_config_t * config) {
    signal(SIGINT, shutdowns);
    workers = config->core_num - listeners;

    int listen_epfd = epoll_create1(0);
    if(-1 == listen_epfd) {
        perror("epoll_creat1: ");
        exit(-1);
    }
    { /* Register listen fd to the listen_epfd */
        struct epoll_event event;
        event.data.fd = file_dsption;
        event.events = EPOLLET | EPOLLERR | EPOLLIN;
        epoll_ctl(listen_epfd, EPOLL_CTL_ADD, file_dsption, &event);
    }
    /* Prepare Workers Sources */
    prepare_workers(config);
    pthread_t listener_set[listeners];
    pthread_t worker_set[workers];
    for (int i = 0; i < listeners; ++i) {
        pthread_create(&listener_set[i], NULL, listen_thread, (void *)&listen_epfd);
        //pthread_detach(listener_set[i]);
    }
    for (int j = 0; j < workers; ++j) {
        pthread_create(&worker_set[j], NULL, workers_thread, (void *)&(epfd_group[j]));
        pthread_detach(worker_set[j]);
    }
    for (int k = 0; k < listeners; ++k) {
        pthread_join(listener_set[k], NULL);
    }
    destroy_resouce();
}

int set_nonblock(int file_dsption){
    int old_flg;
    old_flg = fcntl(file_dsption, F_GETFL);
    fcntl(file_dsption, F_SETFL, old_flg | O_NONBLOCK);
    return old_flg;
}

void optimizes(int file_dsption) {
    const int on = 1;
    if(0 != setsockopt(file_dsption, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) ) {
        perror("REUSEADDR: ");
    }
    if(setsockopt(file_dsption, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)))
        perror("TCP_NODELAY: ");

}

/* open_listenfd is aim to make a prepare for listen socket.
 *
 * */
int open_listenfd(const char * restrict host_addr, const char * restrict port, int * restrict sock_type){
    int listenfd = 0; /* listen the Port, To accept the new Connection */
    struct addrinfo info_of_host;
    struct addrinfo * result;
    struct addrinfo * p;

    memset(&info_of_host, 0, sizeof(info_of_host));
    info_of_host.ai_family = AF_UNSPEC; /* Unknown Socket Type */
    info_of_host.ai_flags = AI_PASSIVE; /* Let the Program to help us fill the Message we need */
    info_of_host.ai_socktype = SOCK_STREAM; /* TCP */

    int error_code;
    if(0 != (error_code = getaddrinfo(host_addr, port, &info_of_host, &result))){
        fputs(gai_strerror(error_code), stderr);
        return ERR_GETADDRINFO; /* -2 */
    }

    for(p = result; p != NULL; p = p->ai_next) {
        listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if(-1 == listenfd)
            continue; /* Try the Next Possibility */
        optimizes(listenfd);
        if(-1 == bind(listenfd, p->ai_addr, p->ai_addrlen)){
            close(listenfd);
            continue; /* Same Reason */
        }
        break; /* If we get here, it means that we have succeed to do all the Work */
    }
    freeaddrinfo(result);
    if (NULL == p) {
        fprintf(stderr, "In %s, Line: %d\nError Occur while Open/Binding the listen fd\n",__FILE__, __LINE__);
        return ERR_BINDIND;
    }
    fprintf(stderr, "DEBUG MESG: Now We(%d) are in : %s , listen the %s port Success\n", listenfd,
            inet_ntoa(((struct sockaddr_in *)p->ai_addr)->sin_addr), port);
    *sock_type = p->ai_family;
    set_nonblock(listenfd);
    return listenfd;
}

