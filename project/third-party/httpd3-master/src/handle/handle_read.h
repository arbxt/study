//
// Created by root on 3/20/16.
//

#ifndef HTTPD3_HANDLE_READ_H
#define HTTPD3_HANDLE_READ_H
#include "http_response.h"

typedef enum {
    DEAL_LINE_REQU_SUCCESS = 0,
    DEAL_LINE_REQU_FAIL    = -1,
    DEAL_HEAD_SUCCESS = -2,
    DEAL_HEAD_FAIL    = -3,
    MESSAGE_INCOMPLETE = -4,
}DEAL_LINE_STATUS;

typedef enum {
    /* read_n error */
            READ_SUCCESS  = -(1 << 0), /* Read From Socket success, continue to parse */
            READ_FAIL     = -(1 << 1), /* Read From Socket Fail, close connection */
    // Discard
            READ_OVERFLOW = -(1 << 2), /* Read Buffer is Overflow, Reject the Connection,
                                Log it and Report to the Server Developer */

    /* get_line error */
            READ_BUF_FAIL     = -(1 << 3), /* Read From conn_client buf fail */
            READ_BUF_OVERFLOW = -(1 << 4), /* Read from conn_client buf is more than the deal buf */
}READ_STATUS;

/* Read the request from the client
 * Prepare data for the handle_write
 * */
HANDLE_STATUS handle_read(conn_client * client);


#endif //HTTPD3_HANDLE_READ_H
