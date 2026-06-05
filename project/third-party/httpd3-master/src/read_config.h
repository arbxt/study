//
// Created by root on 3/17/16.
//

#ifndef HTTPD3_READ_CONFIG_H
#define HTTPD3_READ_CONFIG_H

#define CONFIG_FILE_PATH "../../config/wsx.conf"
#define IPV6_LENGTH_CHAR 128/8

struct init_config_from_file {
    int  core_num;               /* CPU Core numbers */
#define PORT_SIZE 10
    char listen_port[PORT_SIZE]; /*  */
#define ADDR_SIZE IPV6_LENGTH_CHAR
    char use_addr[ADDR_SIZE];    /* NULL For Auto select(By Operating System) */
#define PATH_LENGTH 256
    char root_path[PATH_LENGTH]; /* page root path */
};
typedef struct init_config_from_file wsx_config_t;

/*
 * Read the config file "wsx.conf" in particular path
 * and Put the data to the config object
 * @param  config is aims to be a parameter set
 * @return 0 means Success
 * */
int init_config(wsx_config_t * config);

#endif //HTTPD3_READ_CONFIG_H
