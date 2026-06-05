//
// Created by root on 3/17/16.
//
#include "read_config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char * config_path_search[] = {CONFIG_FILE_PATH, "./wsx.conf", "/etc/wushxin/wsx.conf", NULL};

int init_config(wsx_config_t * config){
    const char ** roll = config_path_search;
    FILE * file = NULL;
    for (int i = 0; roll[i] != NULL; ++i) {
        file = fopen(roll[i], "r");
        if (file != NULL)
            break;
    }
    if (NULL == file) {
#if defined(WSX_DEBUG)
        fprintf(stderr, "Check For the Config file, does it stay its life?\n"
                "In Such Path: \n%s\n%s\n%s\n", config_path_search[0], config_path_search[1], config_path_search[2]);
#endif
        exit(-1);
    }
    char buf[PATH_LENGTH] = {"\0"};
    char * ret;
    ret = fgets(buf, PATH_LENGTH, file);
    while (ret != NULL) {
#if defined(WSX_DEBUG_ALL)
        printf("\n\nReading \" %s \" into buf\n", buf);
#endif
        char * pos = strchr(buf, ':');
        char * check = strchr(buf, '#'); /* Start with # will be ignore */
        if (check != NULL)
            *check = '\0';

        if (pos != NULL) {
            *pos++ = '\0';
            if (0 == strncasecmp(buf, "thread", 6)) {
                sscanf(pos, "%d", &config->core_num);
#if defined(WSX_DEBUG)
                fprintf(stderr, "Thread number is %d \n", config->core_num);
#endif
            }
            else if (0 == strncasecmp(buf, "root", 4)) {
                sscanf(pos, "%s", config->root_path);
                /* End up without "/", Add it */
                if ((config->root_path)[strlen(config->root_path)-1] != '/') {
                    strncat(config->root_path, "/", 1);
                }
#if defined(WSX_DEBUG)
                fprintf(stderr, "root path is %s\n", config->root_path);
#endif
            }
            else if (0 == strncasecmp(buf, "port", 4)) {
                sscanf(pos, "%s", config->listen_port);
#if defined(WSX_DEBUG)
                fprintf(stderr, "listen port is %s\n", config->listen_port);
#endif
            }
            else if (0 == strncasecmp(buf, "addr", 4)) {
                sscanf(pos, "%s", config->use_addr);
#if defined(WSX_DEBUG)
                fprintf(stderr, "Listen address is %s\n", config->use_addr);
#endif
            }
        }
        ret = fgets(buf, PATH_LENGTH, file);
    }
    fclose(file);
    return 0;
}