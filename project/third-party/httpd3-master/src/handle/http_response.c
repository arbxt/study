//
// Created by root on 3/21/16.
//
#include "http_response.h"
#include "handle_core.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
extern char * cache_file;
extern char * website_root_path;
/* HTTP Date */
static const char * date_week[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char * date_month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
/* HTTP Content-Type */
typedef enum {
    TEXT_HTML = 0, TEXT_PLAIN = 1/* .txt .sor .sol or nothingand so on */,
    IMAGE_GIF = 2, IMAGE_JPEG = 3/* .jfif .jpeg .jpg .jpe*/,
    IMAGE_PNG = 4, IMAGE_BMP = 5,
}CONTENT_TYPE;
static const char * content_type[] = {
        "text/html; charset=utf-8",
        "text/plain; charset=utf-8",
        "image/git", "image/jpeg",
        "image/png", "image/bmp",
};

static const char * http_ver[] = {
        "HTTP/0.9", /* HTTP_0_9 */
        "HTTP/1.0", /* HTTP_1_0 */
        "HTTP/1.1", /* HTTP_1_1 */
        "HTTP/2.0", /* HTTP_2_0 */
        "UNKNOWN",  /* HTTP_UNKNOWN */
};

/* Connection Status */
enum {
    CONN_CLOSE = 0, CONN_KEEP_ALIVE,
};
/*
 * TODO implement
 * */
__attribute__ ((unused)) static const char * conn_status[] = { "close", "keep-alive" };

/* HTTP Status Code */
static const char * const
        ok_200_status[] = { "200",
                            "OK", ""};
static const char * const
        redir_304_status[] = {"304",
                              "Not Modify", "Your file is the Newest"};
static const char * const
        clierr_400_status[] = {"400",
                               "Bad Request", "WuShxin Server can not Resolve that Apply"};
static const char * const
        clierr_403_status[] = {"403",
                               "Forbidden", "Request Forbidden"};
static const char * const
        clierr_404_status[] = {"404",
                               "Not Found", "WuShxin Server can not Find the page"};
static const char * const
        clierr_405_status[] = {"405",
                               "Method Not Allow", "WuShxin Server has not implement that method yet ~_~"};
static const char * const
        sererr_500_status[] = {"500",
                               "Internal Server Error", "Apply Incomplete, WuShxin Server Meet Unknown Status"};
static const char * const
        sererr_503_status[] = {"503",
                               "Service Unavailable", "WuShxin Server is Overload Or go die : )"};

/*
 * Let the URI to be the Valid Way.
 * For example peer request the "/index.html"
 * make it to be -> "/root/real/path/index.html"
 * */
static inline void deal_uri_string(string_t uri) {
#if defined(WSX_DEBUG)
    fputs("\n[String]The resource is ",stderr);
    print_string(uri);
    fprintf(stderr, "Website_root_path: %s\n", website_root_path);
#endif
    if (1 == get_length(uri) && NULL != search_content(uri, "/", 1)){
    //if (2 == uri->use->length(uri) && NULL != uri->use->has(uri, "/")){
        clear_string(uri);
        append_string(uri, STRING(website_root_path));
        append_string(uri, "index.html", 10);
        //uri->use->clear(uri);
        //uri->use->append(uri, APPEND(website_root_path));
        //uri->use->append(uri, APPEND("index.html"));
    } else {
        char tmp[1024] = {0};
        snprintf(tmp, 1024, "%s%s", website_root_path, uri->str+1);
        clear_string(uri);
        append_string(uri, STRING(tmp));
        //uri->use->clear(uri);uri->use->append(uri, APPEND(tmp));
    }
#if defined(WSX_DEBUG)
    uri->use->print(uri);
#endif
}

typedef enum {
    IS_NORMAL_FILE = 0,
    NO_SUCH_FILE = 1,
    FORBIDDEN_ENTRY = 2,
    IS_DIRECTORY = 3,
}URI_STATUS;
/*
 * Check if the Resource which CLient apply is A REAL File Resource(Include the Authorization)
 * */
static URI_STATUS check_uri_str(string_t restrict uri_str, int * restrict file_size) {
    struct stat buf = {0};
    if (stat(uri_str->str, &buf) < 0) {
        fprintf(stderr, "Get File %s ", uri_str->str);
        perror("Message: ");
        return NO_SUCH_FILE; /* No such file */
    }
    if ( 0 == (buf.st_mode & S_IROTH) ) {
        fprintf(stderr, "FORBIDDEN READING\n");
        return FORBIDDEN_ENTRY; /* Forbidden Reading */
    }
    if (S_ISDIR(buf.st_mode)) {
        return IS_DIRECTORY; /* Is Directiry */
    }
    *file_size = (int)(buf.st_size);
    return IS_NORMAL_FILE;
}

/*
 * Put the Respone Message to the conn_client's write_buf
 * return 0 if Success, Or the remaining Bytes that can not pull into the write buffer(Not Big Enough)
 *
 * */
__thread char local_write_buf[CONN_BUF_SIZE] = {0};
static int write_to_buf(conn_client * restrict client, // connection client message
                        const char * const * restrict status,  int rsource_size) {
#define STATUS_CODE 0
#define STATUS_TITLE 1
#define STATUS_CONTENT 2
    char *   write_buf = &local_write_buf[0];//- client->write_buf; /* Local write buffer */
    string_t resource  = client->conn_res.requ_res_path; /* Resource that peer request */
    string_t w_buf     = client->w_buf;     /* Real data buffer */
    int w_count = 0;
    struct tm * utc;   /* Get GMT time Format */
    time_t      now;
    time(&now);
    utc = gmtime(&now);/* Same As before */

    /* Construct the HTTP head */
    w_count += snprintf(write_buf+w_count, (size_t)(CONN_BUF_SIZE-w_count), "%s %s %s\r\n",
                        http_ver[client->conn_res.request_http_v],
                        status[STATUS_CODE], status[STATUS_TITLE]);
    w_count += snprintf(write_buf+w_count, (size_t)CONN_BUF_SIZE-w_count, "Date: %s, %02d %s %d %02d:%02d:%02d GMT\r\n",
                        date_week[utc->tm_wday], utc->tm_mday,
                        date_month[utc->tm_mon], 1900+utc->tm_year,
                        utc->tm_hour, utc->tm_min, utc->tm_sec);
    w_count += snprintf(write_buf+w_count, (size_t)CONN_BUF_SIZE-w_count, "Content-Type: %s\r\n", content_type[client->conn_res.content_type]);
    w_count += snprintf(write_buf+w_count, (size_t)CONN_BUF_SIZE-w_count, "Content-Length: %u\r\n", 0 == rsource_size
                                                                                            ? (unsigned int)strlen(status[2]):(unsigned int)rsource_size);
    w_count += snprintf(write_buf+w_count, (size_t)CONN_BUF_SIZE-w_count, "Connection: close\r\n");
    w_count += snprintf(write_buf+w_count, (size_t)CONN_BUF_SIZE-w_count, "\r\n");
    write_buf[w_count] = '\0';
    append_string(w_buf, STRING(write_buf));
    //w_buf->use->append(w_buf, APPEND(write_buf));
    client->w_buf_offset = w_count;

    /* If Server do not wanna to sent local file */
    if (0 == rsource_size) {  /* GET Method */
        append_string(w_buf, STRING(status[STATUS_CONTENT]));
        //w_buf->use->append(w_buf, APPEND(status[STATUS_CONTENT]));
        //snprintf(write_buf+w_count, (size_t)(CONN_BUF_SIZE-w_count), status[2]);
        return 0;
    } else if (-1 == rsource_size) { /* HEAD Method */
        return 0;
    }
    if (0 != rcmp_content(resource, "index.html", 10)) {
        /* Open and map the file to memory */
        int fd = open(resource->str, O_RDONLY);
        if (fd < 0) {
            return -1; /* Write again */
        }
        char *file_map = mmap(NULL, (size_t)rsource_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (NULL == file_map) {
            assert(file_map != NULL);
        }
        close(fd);

        /* Construct the HTTP Content if needed */
        append_string(w_buf, file_map, (uint32_t)rsource_size);
        //w_buf->use->append(w_buf, file_map, (unsigned int)rsource_size);
        client->w_buf_offset += rsource_size;
        munmap(file_map, (unsigned int)rsource_size);
    }
    else {
        char *file_map = cache_file;
        append_string(w_buf, file_map, (uint32_t)rsource_size);
        //w_buf->use->append(w_buf, file_map, (unsigned int)rsource_size);
        client->w_buf_offset += rsource_size;
    }
    return 0;
#undef STATUS_CODE
#undef STATUS_TITLE
#undef STATUS_CONTENT
}

static CONTENT_TYPE check_res_type(string_t uri){
    int length = get_length(uri);
    //uri->use->length(uri);
    char * search = uri->str;
    char * type = NULL;
    for (int i = length - 1; i >= 0 && (search[i] != '/'); --i) {
        if ('.' == search[i]) {
            type = search + i;
            break;
        }
    }
    if (NULL == type) return TEXT_PLAIN;
    if (0 == strncasecmp(type, ".jepg", 5) || 0 == strncasecmp(type, ".jpg", 4) ||
        0 == strncasecmp(type, ".jpe", 4)  || 0 == strncasecmp(type, ".jfif", 5) )
        return IMAGE_JPEG;
    if (0 == strncasecmp(type, ".png", 4)) return IMAGE_PNG;
    if (0 == strncasecmp(type, ".bmp", 4)) return IMAGE_BMP;
    if (0 == strncasecmp(type, ".gif", 4)) return IMAGE_GIF;
    if (0 == strncasecmp(type, ".html", 5)) return TEXT_HTML;
    return TEXT_PLAIN;
}
static inline int set_res_type(conn_client * client) {
    client->conn_res.content_type = check_res_type(client->conn_res.requ_res_path);
    return 0;
}

/*
 * Return 0 if it is safe, 1 for evil request path
 * */
static inline int check_evil_path(string_t uri) {
    return (NULL != search_content(uri, "..", 2)) ? 1 : 0;
    //return NULL != uri->use->has(uri, "..") ? 1 : 0;
}

/*
 * Universal Response Page maker
 * */
MAKE_PAGE_STATUS make_response_page(conn_client * client)
{
    int err_code = 0;
    int uri_file_size = 0;

    string_t uri_str = client->conn_res.requ_res_path;
    /* Check Is it Empty? */
    if (1 <= get_length(uri_str)) {
    //if (1 <= uri_str->use->length(uri_str)) {
        /* Evil Request! */
        err_code = check_evil_path(uri_str);
        if (1 == err_code) {
            err_code = write_to_buf(client, clierr_403_status, 0);
            if (err_code < 0)
                return MAKE_PAGE_FAIL;
            return MAKE_PAGE_SUCCESS;
        }
        /* Make it valid */
        deal_uri_string(uri_str);
    } else {
        goto SERVER_ERROR;
    }
    if (METHOD_GET == client->conn_res.request_method) {
    //if(0 == strncasecmp(client->conn_res.requ_method->str, "GET", 3)) { /* Check for real */
        /* Is it a real file ? and get its file attribute */
        err_code = check_uri_str(uri_str, &uri_file_size);
        if (IS_NORMAL_FILE == err_code) {
            set_res_type(client);
            if((err_code = write_to_buf(client, ok_200_status, uri_file_size)) < 0)
                goto SERVER_ERROR;
        }
        else if (FORBIDDEN_ENTRY == err_code){
            err_code = write_to_buf(client, clierr_403_status, 0);
            if (err_code < 0)
                return MAKE_PAGE_FAIL;
            return MAKE_PAGE_SUCCESS;
        }
        else /*if (NO_SUCH_FILE == err_code || IS_DIRECTORY == err_code)*/ {
            write_to_buf(client, clierr_404_status, 0);
        }
    }
    else if (METHOD_HEAD == client->conn_res.request_method) {
//    else if ( 0 == strncasecmp(client->conn_res.requ_method->str, "HEAD", 4)) {
        err_code = check_uri_str(uri_str, &uri_file_size);
        if (IS_NORMAL_FILE == err_code) {
            /* Check what kind of Resource does peer Request */
            set_res_type(client);
            if((err_code = write_to_buf(client, ok_200_status, -1)) < 0)
                goto SERVER_ERROR;
        }
        else if (FORBIDDEN_ENTRY == err_code){
            write_to_buf(client, clierr_403_status, -1);
        }
        else /*if (NO_SUCH_FILE == err_code || IS_DIRECTORY == err_code)*/ {
            write_to_buf(client, clierr_404_status, -1);
        }
    }
    else if (METHOD_POST == client->conn_res.request_method) {
    //else if ( 0 == strncasecmp(client->conn_res.requ_method->str, "POST", 4)) {
        /* TODO
         * POST Method
         * */
        write_to_buf(client, clierr_405_status, 0);
    }
    else { /* Unknown Method */
        write_to_buf(client, clierr_400_status, 0);
    }
    return MAKE_PAGE_SUCCESS;

SERVER_ERROR:
    err_code = write_to_buf(client, sererr_500_status, 0);
    if (err_code < 0)
        return MAKE_PAGE_FAIL;
    return MAKE_PAGE_SUCCESS;
}