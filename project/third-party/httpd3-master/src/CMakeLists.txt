cmake_minimum_required(VERSION 3.3)
project(httpd3)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror -O2 -pthread")
set(SOURCE_FILES
        read_config.h read_config.c main.c
        handle/handle_core.h
        handle/handle.h memop/manage.h handle/handle_read.h handle/handle_write.h handle/http_response.h
        handle/handle.c memop/manage.c handle/handle_read.c handle/handle_write.c handle/http_response.c
        util/wsx_string.h util/wsx_string.c)

add_executable(httpd3 ${SOURCE_DIR} ${SOURCE_FILES} )
