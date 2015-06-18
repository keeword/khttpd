#ifndef _KHTTPD_H_
#define _KHTTPD_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "io.h"

#define BUFFER_SIZE 1024
#define MAXEVENTS 10240
#define MAX_HTTP_METHOD_LENGTH 10
#define MAX_URL_LENGTH 256
#define HTTP_VERSION_LENGTH 10

#define INDEX_FILE "index.htm"
#define ROOT_DOCUMENT "/home/neng/workspace/khttpd"

struct connecter {
        int connect_sock;
};

#define CR                  '\r'
#define LF                  '\n'
#define LOWER(c)            (unsigned char)(c | 0x20)
#define IS_ALPHA(c)         (LOWER(c) >= 'a' && LOWER(c) <= 'z')
#define IS_NUM(c)           ((c) >= '0' && (c) <= '9')
#define IS_ALPHANUM(c)      (IS_ALPHA(c) || IS_NUM(c))
#define IS_HEX(c)           (IS_NUM(c) || (LOWER(c) >= 'a' && LOWER(c) <= 'f'))
#define IS_SEPARATOR(C)     ((c) == '(' || (c) == ')' || (c) == '<' || (c) == '>' ||    \
        (c) == '@' || (c) == ',' || (c) == ';' || (c) == ':' || (c) == '\\' ||          \
        (c) == '\"' || (c) == '?' || (c) == '[' || (c) == ']' || (c) == '=' ||          \
        (c) == '/' || (c) == '{' || (c) == '}' || (c) == ' ' || (c) == '\t')

#define METHOD_NUM 4
#define HTTP_METHOD_MAP(XX)            \
        XX(GET,    "GET")              \
        XX(POST,   "POST")             \
        XX(PUT,    "PUT")              \
        XX(DELETE, "DELETE")           \

enum http_method {
#define XX(name, string) HTTP_##name,
        HTTP_METHOD_MAP(XX)
#undef XX
};

#define STATE_CODE_MAP(XX)                      \
        XX(100, "Continue")                     \
        XX(101, "Switching Protocols")          \
        XX(200, "OK")                           \
        XX(201, "Created")                      \
        XX(202, "Accepted")                     \
        XX(203, "Non-Authoritative Information")\
        XX(204, "No Content")                   \
        XX(205, "Reset Content")                \
        XX(206, "Partial Content")              \
        XX(300, "Multiple Choices")             \
        XX(301, "Moved Permanently")            \
        XX(302, "Found")                        \
        XX(303, "See Other")                    \
        XX(304, "Not Modified")                 \
        XX(305, "Use Proxy")                    \
        XX(307, "Temporary Redirect")           \
        XX(400, "Bad Request")                  \
        XX(401, "Unauthorized")                 \
        XX(403, "Forbidden")                    \
        XX(404, "Not Found")                    \
        XX(405, "Method Not Allowed")           \
        XX(406, "Not Acceptable")               \
        XX(407, "Proxy Authentication Required")\
        XX(408, "Request Timeout")              \
        XX(409, "Conflict")                     \
        XX(410, "Gone")                         \
        XX(411, "Length Required")              \
        XX(412, "Precondition Failed")          \
        XX(413, "Request Entity Too Large")     \
        XX(414, "Request URI Too Long")         \
        XX(416, "Requested Range Not Satisfiable")      \
        XX(500, "Internal Server Error")        \
        XX(501, "Not Implemented")              \
        XX(502, "Bad Gateway")                  \
        XX(503, "Service Unavailable")          \
        XX(504, "Gateway Timeout")              \
        XX(505, "HTTP Version Not Supported")   \

enum state_code {
#define XX(code, phrase) STATE_CODE_##code = code,
        STATE_CODE_MAP(XX)
#undef XX
};

struct http_header {
        unsigned int type;
        unsigned short http_major;
        unsigned short http_minor;

        // request only
        unsigned int state_code;
        char url[256];

        // response only
        unsigned int method;

        unsigned int http_errno;
};

#endif // _KHTTPD_H_
