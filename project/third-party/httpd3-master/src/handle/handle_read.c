//
// Created by root on 3/20/16.
//
#include <string.h>
#include <assert.h>
#include "handle_read.h"


typedef enum {
    PARSE_SUCCESS    = 1 << 1, /* Parse the Reading Success, set the event to Write Event */
    PARSE_BAD_SYNTAX = 1 << 2, /* Parse the Reading Fail, for the Wrong Syntax */
    PARSE_BAD_REQUT  = 1 << 3, /* Parse the Reading Success, but Not Implement OR No Such Resources*/
}PARSE_STATUS;

/* Parse the Reading thing, and Make deal with them
 * */
static PARSE_STATUS parse_reading(conn_client * client);
static int read_n(conn_client * client);

HANDLE_STATUS handle_read(conn_client * client) {
    int err_code = 0;

    /* Reading From Socket */
    err_code = read_n(client);
    if (err_code != READ_SUCCESS) { /* If read Fail then End this connect */
        return HANDLE_READ_FAILURE;
    }
#if defined(WSX_DEBUG)
    fprintf(stderr, "\nRead From Client(%d): %s\n read %d Bytes\n", client->file_dsp, client->r_buf->str, client->r_buf_offset);
    fprintf(stderr, "\nStart Parsing Reading\n");
#endif
    /* Parsing the Reading Data */
    err_code = parse_reading(client);
    if (err_code == MESSAGE_INCOMPLETE)
        return MESSAGE_INCOMPLETE;
    if (err_code != PARSE_SUCCESS) { /* If Parse Fail then End this connect */
        return HANDLE_READ_FAILURE;
    }
    return HANDLE_READ_SUCCESS;
}

/*
 * Read data from peer(TCP Read buffer)
 * read_buf is aim to be a local buffer
 * client->r_buf is the real Storage of the data
 * */
__thread char read_buf2[CONN_BUF_SIZE] = {0};
static int read_n(conn_client * client) {
    //- char   read_buf2[CONN_BUF_SIZE] = {0};
    int    read_offset2 = 0;
    int    fd        = client->file_dsp;
    char * buf       = &read_buf2[0];//client->read_buf;
    int    buf_index = read_offset2;//client->read_offset;
    int read_number = 0;
    int less_capacity = 0;
    while (1) {
        less_capacity = CONN_BUF_SIZE - buf_index;
        if (less_capacity <= 1) {/* Overflow Protection */
            buf[buf_index] = '\0'; /* Flush the buf to the r_buf String */
            append_string(client->r_buf, STRING(buf));
            //client->r_buf->use->append(client->r_buf, APPEND(buf));
            client->r_buf_offset += read_offset2;//- client->read_offset;
            read_offset2 = 0;
            buf_index = 0;
            //- client->read_offset = buf_index = 0;
            //- buf = client->read_buf;
            less_capacity = CONN_BUF_SIZE - buf_index;
        }
        read_number = (int)read(fd, buf+buf_index, less_capacity);
        if (0 == read_number) { /* We must close connection */
            return READ_FAIL;
        }
        else if (-1 == read_number) { /* Nothing to read */
            if (EAGAIN == errno || EWOULDBLOCK == errno) {
                buf[buf_index] = '\0';
                append_string(client->r_buf, STRING(buf));
                //client->r_buf->use->append(client->r_buf, APPEND(buf));
                client->r_buf_offset += read_offset2;//client->read_offset;
                return READ_SUCCESS;
            }
            return READ_FAIL;
        }
        else { /* Continue to Read */
            buf_index += read_number;
            read_offset2 = buf_index;
            //- client->read_offset = buf_index;
        }
    }
}

/******************************************************************************************/
/* Deal with the read buf data which has been read from socket */
#define METHOD_SIZE 128
#define PATH_SIZE METHOD_SIZE*2
#define VERSION_SIZE METHOD_SIZE
typedef struct requ_line{
    char path[PATH_SIZE];
    char method[METHOD_SIZE];
    char version[VERSION_SIZE];
}requ_line;
static int get_line(conn_client * restrict client, char * restrict line_buf, int max_line);
static DEAL_LINE_STATUS deal_requ(conn_client * client, requ_line * status);
static DEAL_LINE_STATUS deal_head(conn_client * client);

/*
 * Now we believe that the GET has no request-content(Request Line && Request Head)
 * The POST Will be considered later
 * parse_reading will parse the request line and Request Head,
 * and Prepare the Page which will set into the Write buffer
 * */
PARSE_STATUS parse_reading(conn_client * client) {
    int err_code = 0;
    requ_line line_status = {0};
    //- client->read_offset  = 0; /* Set the local buffer offset to 0, the end of buf is '\0' */
    client->r_buf_offset = 0; /* Set the real Storage offset to 0, the end of buf is '\0' */

    /* Get Request line */
    err_code = deal_requ(client, &line_status);
    if (MESSAGE_INCOMPLETE == err_code)  /* Incompletely reading */
        return MESSAGE_INCOMPLETE;
    if (DEAL_LINE_REQU_FAIL == err_code) /* Bad Request */
        return PARSE_BAD_REQUT;
#if defined(WSX_DEBUG)
    fprintf(stderr, "Starting Deal_head\n");
#endif
    /* Get Request Head Attribute until /r/n */
    err_code = deal_head(client);  /* The second line to the Empty line */
    if (DEAL_HEAD_FAIL == err_code)
        return PARSE_BAD_SYNTAX;
    
    /* Response Page maker */
    err_code = make_response_page(client);  
    if (MAKE_PAGE_FAIL == err_code)
        return PARSE_BAD_REQUT;

    return PARSE_SUCCESS;
}

/*
 * deal_requ, get the request line
 * */
static DEAL_LINE_STATUS deal_requ(conn_client * client, requ_line * status) {
#define READ_HEAD_LINE 3+256+8+1
    char requ_line[READ_HEAD_LINE] = {'\0'};
    int err_code = get_line(client, requ_line, READ_HEAD_LINE);
#if defined(WSX_DEBUG)
    assert(err_code > 0);
#endif
    if (err_code < 0)
        return DEAL_LINE_REQU_FAIL;
#if defined(WSX_DEBUG)
    fprintf(stderr, "Requset line is : %s \n", requ_line);
#endif
    /* Is the Message Complete */
    if ('\n' != requ_line[strlen(requ_line)-1] && '\r' != requ_line[strlen(requ_line)-2])
        return MESSAGE_INCOMPLETE;

    /* For example GET / HTTP/1.0 */
    err_code = sscanf(requ_line, "%s %s %s", status->method, status->path, status->version);
    if (err_code != 3)
        return DEAL_LINE_REQU_FAIL;
    /* Confirm the Request method for checking Is it Read Done */
    if(0 == strncasecmp(status->method, "GET", 3))
        client->conn_res.request_method = METHOD_GET;
    else if(0 == strncasecmp(status->method, "HEAD", 4))
        client->conn_res.request_method = METHOD_HEAD;
    else if(0 == strncasecmp(status->method, "POST", 4))
        client->conn_res.request_method = METHOD_POST;
    else {
        client->conn_res.request_method = METHOD_UNKNOWN;
        return DEAL_LINE_REQU_FAIL;
    }
    /* Confirm the Request HTTP VERSION for checking Is it Read Done */
    if (0 == strncasecmp(status->version, "HTTP/1.1", 8))
        client->conn_res.request_http_v = HTTP_1_1;
    else if (0 == strncasecmp(status->version, "HTTP/1.0", 8))
        client->conn_res.request_http_v = HTTP_1_0;
    else if (0 == strncasecmp(status->version, "HTTP/0.9", 8))
        client->conn_res.request_http_v = HTTP_0_9;
    else if (0 == strncasecmp(status->version, "HTTP/2.0", 8))
        client->conn_res.request_http_v = HTTP_2_0;
    else {
        client->conn_res.request_http_v = HTTP_UNKNOWN;
        return DEAL_LINE_REQU_FAIL;
    }
    append_string((client->conn_res).requ_res_path, STRING(status->path));
    //(client->conn_res).requ_res_path->use->append((client->conn_res).requ_res_path, APPEND(status->path));
#if defined(WSX_DEBUG)
    fprintf(stderr, "The Request method : %s, path : %s, version : %s\n",
                                    status->method, status->path, status->version);
    fprintf(stderr, "[String] The Request method : \n");
    print_string(client->conn_res.requ_res_path);
    print_string(client->conn_res.requ_http_ver);
    print_string(client->conn_res.requ_method);
    //client->conn_res.requ_res_path->use->print(client->conn_res.requ_res_path);
    //client->conn_res.requ_http_ver->use->print(client->conn_res.requ_http_ver);
    //client->conn_res.requ_method->use->print(client->conn_res.requ_method);
#endif
    return DEAL_LINE_REQU_SUCCESS;
#undef READ_HEAD_LINE
}
/*
 * get the request head
 * */
static DEAL_LINE_STATUS deal_head(conn_client * client) {
    /* TODO
     * Complete the Function of head attribute
     * */
#define READ_ATTRIBUTE_LINE 256
#define TRUE 1
#define FALSE 0
    int nbytes = 0;
    boolean is_post = METHOD_POST == client->conn_res.request_method ? TRUE : FALSE;
    char head_line[READ_ATTRIBUTE_LINE] = {'\0'};
    while((nbytes = get_line(client, head_line, READ_ATTRIBUTE_LINE)) > 0) {
        if (MESSAGE_INCOMPLETE == nbytes)
            return MESSAGE_INCOMPLETE;
        if(0 == strncmp(head_line, "\r\n", 2)) { /* Read the Empty Line */
#if defined(WSX_DEBUG)
            fprintf(stderr, "Read the empty Line\n");
#endif
            break;
        }
        /* If Line is Complete */
        if (TRUE == is_post) {
            if (0 == strncmp(head_line, "Content-Length", 14)){
                /* 15 is For Content-Length: xxx\r\n */
                char * end = strchr(head_line, '\r');
                *end = '\0';
                sscanf(head_line+16, "%d", &(client->conn_res.content_length));
                *end = '\r';
            }
        }
        if (0 == strncmp(head_line, "Connection", 10)) {
            /* Connection: keep-alive\r\n */
            if (0 == strncmp(head_line+12, "close", 5))
                client->conn_res.conn_linger = 1;
            else
                client->conn_res.conn_linger = 0;
        }
#if defined(WSX_DEBUG)
        fprintf(stderr, "The Line we parse(%d Bytes) : ", nbytes);
        fprintf(stderr, "%s", head_line);
#endif
    }
    if (TRUE == is_post) {
        /* TODO POST METHOD
         * make sure the Message has been Receive completely and then Parse them
         * */
    }
    if (nbytes < 0) {
        fprintf(stderr, "Error Reading in deal_head\n");
        return DEAL_HEAD_FAIL;
    }
#if defined(WSX_DEBUG)
    fprintf(stderr, "Deal head Success\n");
#endif
    return DEAL_HEAD_SUCCESS;
#undef READ_ATTRIBUTE_LINE
#undef TRUE
#undef FALSE
}

/* Get One Line(\n\t) From The Reading buffer
 * */
static int get_line(conn_client * restrict client, char * restrict line_buf, int max_line) {
    int nbytes = 0;
    char *r_buf_find = search_content(client->r_buf, "\n", 1);
    //(client->r_buf->use->has(client->r_buf,"\n"));
    if (NULL == r_buf_find){ 
        fprintf(stderr, "get_line has read a incomplete Message\n");
        return MESSAGE_INCOMPLETE;
    } else{
        nbytes = r_buf_find - (client->r_buf->str + client->r_buf_offset) + 1;
        if (max_line-1 < nbytes)
            return READ_BUF_OVERFLOW;
        memcpy(line_buf, client->r_buf->str+client->r_buf_offset, nbytes);
        client->r_buf_offset = r_buf_find - client->r_buf->str + 1;
        *r_buf_find = '\r'; /* Let the \n to be \r */
    }
    line_buf[nbytes] = '\0';
    return nbytes;
}
