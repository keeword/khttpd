#ifndef _PARSE_H_
#define _PARSE_H_

#include <stdio.h>
#include <stdlib.h>

/*
 * maximum of the http header size.
 * change the default value by defining
 * the macro in build environment.
 */
#ifndef HTTP_MAX_HEADER_SIZE
#define HTTP_MAX_HEADER_SIZE (80*1024)
#endif

/* maximum of the url size
 * change the default value by defining
 * the macro in build environment.
 */
#ifndef HTTP_MAX_URL_SIZE
#define HTTP_MAX_URL_SIZE 256
#endif

/* http version length, for HTTP/1.0 and HTTP/1.1
 * not support HTTP/2
 */
#define HTTP_VERSION_LENGTH 8

#define BUFFER_SIZE 1024
#define MAXEVENTS 10240

#define CR                  '\r'
#define LF                  '\n'
#define SP                  ' '
#define LOWER(c)            (unsigned char)(c | 0x20)
#define IS_ALPHA(c)         (LOWER(c) >= 'a' && LOWER(c) <= 'z')
#define IS_NUM(c)           ((c) >= '0' && (c) <= '9')
#define IS_ALPHANUM(c)      (IS_ALPHA(c) || IS_NUM(c))
#define IS_HEX(c)           (IS_NUM(c) || (LOWER(c) >= 'a' && LOWER(c) <= 'f'))
#define IS_SEPARATOR(C)     ((c) == '(' || (c) == ')' || (c) == '<' || (c) == '>' ||    \
        (c) == '@' || (c) == ',' || (c) == ';' || (c) == ':' || (c) == '\\' ||          \
        (c) == '\"' || (c) == '?' || (c) == '[' || (c) == ']' || (c) == '=' ||          \
        (c) == '/' || (c) == '{' || (c) == '}' || (c) == ' ' || (c) == '\t')

/* request methods 
 * (name, string, value)
 * use CHAR_POINTER_TO_SHORT macro to work out the value
*/
#define HTTP_METHOD_NUMBER 4
#define HTTP_METHOD_MAP(XX)            \
        XX(GET,    "GET",       17735) \
        XX(POST,   "POST",      20304) \
        XX(PUT,    "PUT",       21840) \
        XX(DELETE, "DELETE",    17732) \

enum http_method {
#define XX(name, string, value) HTTP_METHOD_##name,
        HTTP_METHOD_MAP(XX)
#undef XX
};

/* response state code 
 * (code, phrase)
 */
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
#define XX(code, phrase) STATE_CODE_##code,
        STATE_CODE_MAP(XX)
#undef XX
};

/* header fields 
 * (name, string)
 */
#define HEADER_FIELD_NUMBER 21
#define HEADER_FIELD_MAP(XX)                     \
        XX(Accept,           "Accept")           \
        XX(Accept_Charset,   "Accept-Charset")   \
        XX(Accept_Encoding,  "Accept-Encoding")  \
        XX(Accept_Language,  "Accept-Language")  \
        XX(Accept_Datetime,  "Accept-Datetime")  \
        XX(Authorization,    "Authorization")    \
        XX(Cache_Control,    "Cache-Control")    \
        XX(Connection,       "Connection")       \
        XX(Cookie,           "Cookie")           \
        XX(Date,             "Date")             \
        XX(Expect,           "Expect")           \
        XX(From,             "From")             \
        XX(Host,             "Host")             \
        XX(Referer,          "Referer")          \
        XX(User_Agent,       "User-Agent")       \
        XX(Content_Encoding, "Content-Encoding") \
        XX(Content_Language, "Content-Language") \
        XX(Content_Length,   "Content-Length")   \
        XX(Content_Type,     "Content-Type")     \
        XX(Server,           "Server")           \
        XX(Set_Cookie,       "Set-Cookie")       \

enum header_field {
#define XX(name, string) FIELD_##name,
        HEADER_FIELD_MAP(XX)
#undef XX
};

enum state {
        // state
        s_dead = 1,
        s_done,
        s_request_start,
        s_method_start,
        s_method_end,
        s_url_start,
        s_url_end,
        s_path_start,
        s_schema_start,
        s_query_start,
        s_version_start,
        s_field_start,
        s_line_cr,
        s_line_lf,

        // error
        e_invalid_method,
        e_invalid_url,
        e_invalid_version,
        e_eol,
        e_again,
};

// zero the http header
#define http_header_init(h)                             \
do {                                                    \
        memset((h), 0, sizeof(struct http_header));     \
} while(0)

#define HTTP_TYPE_REQUEST  0
#define HTTP_TYPE_RESPONSE 1

/* struct of http header */
struct http_header {
        unsigned int type;              // request or response
        unsigned int state;             // http header processing state
        unsigned int error;             // error number
        unsigned short http_major;
        unsigned short http_minor;

        size_t nread;                   // byte have read

        // response only
        unsigned int state_code;

        // request only
        unsigned int method;
        int url_len;
        char url[HTTP_MAX_URL_SIZE];

        int cgi;
        unsigned int http_errno;
        char path[512];
        char query_string[256];

        char *field[HEADER_FIELD_NUMBER];
};

int parse_request(struct http_header*, char*, size_t);
int parse_method(struct http_header*, char*);
int parse_url(struct http_header*, char*, size_t);
int parse_version(struct http_header*, char*);
int parse_field(struct http_header*, char*);
int parse_field_content(struct http_header*, char*);

#endif // _PARSE_H_
