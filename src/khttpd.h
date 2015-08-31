#ifndef _KHTTPD_H_
#define _KHTTPD_H_

#include "parse.h"

#define BUFFER_SIZE 1024
#define MAXEVENTS 10240

#define INDEX_FILE "index.html"
#define DOCUMENT_ROOT "/home/neng/workspace/khttpd"

struct connector {
        int connect_sock;
        struct http_header header;
        struct connector *prev;
        struct connector *next;
};

struct client {
        int fd;
        struct http_header header;
        char buff[HTTP_MAX_HEADER_SIZE];
        struct client *prev;
        struct client *next;
};

#endif // _KHTTPD_H_
