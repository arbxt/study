//
// Created by root on 3/17/16.
//

#ifndef HTTPD3_HANDLE_H
#define HTTPD3_HANDLE_H

#include <signal.h>
#include <sys/socket.h>

#include "handle_read.h"
#include "handle_write.h"
/* For wsx_config_t */
#include "../read_config.h"
#define MAX_LISTEN_EPFD_SIZE 1
#define OPEN_FILE 50000

enum open_listener{
    ERR_PARA_EMPTY = -1,
    ERR_GETADDRINFO = -2,
    ERR_BINDIND = -3
};

/*
 * Open The Listen Socket With the specific host(IP address) and port
 * That must be compatible with the IPv6 And IPv4
 * host_addr could be NULL
 * port MUST NOT BE NULL !!!
 * sock_type is the pointer to a memory ,which comes from the Outside(The Caller)
 * */
int open_listenfd(const char * restrict host_addr,const char * restrict port, int * restrict sock_type);

/*
 * Let the file description to be Nonblock
 * */
int set_nonblock(int file_dsption);

/*
 * Set the SO_REUSEADDR and TCP_NODELAY
 */
void optimizes(int file_dsption);

/* Called in main function
 * Main Structure, create Workers' thread and listen's thread
 * @param file_dsption  means that linsten socket, which will be used in listen's thread
 *        sock_type     IPv4 or IPv6
 *        config        contains the configuration which wsx.conf says.
 * */
void handle_loop(int file_dsption, int sock_type, const wsx_config_t * config);
#endif //HTTPD3_HANDLE_H
