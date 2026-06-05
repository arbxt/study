//
// Created by root on 3/22/16.
//
#include "handle_write.h"

HANDLE_STATUS handle_write(conn_client * client) {
    /* String Version */
    char*    w_buf    = client->w_buf->str;
    int      w_offset = client->w_buf_offset;
    int nbyte = w_offset;
    int count = 0;
    int fd = client->file_dsp;
    //- char * buf = client->write_buf;
    while (nbyte > 0) {
       //-  buf += count;
        w_buf += count;
        count = write(fd, w_buf, nbyte);
        if (count < 0) {
            if (EAGAIN == errno || EWOULDBLOCK == errno) {
                memcpy(client->w_buf->str, w_buf, strlen(w_buf));
                client->w_buf_offset = nbyte;
                return HANDLE_WRITE_AGAIN;
            }
            else /* if (EPIPE == errno) */
                return HANDLE_WRITE_FAILURE;
        }
        else if (0 == count)
            return HANDLE_WRITE_FAILURE;
        nbyte -= count;
    }
    return HANDLE_WRITE_SUCCESS;
}


