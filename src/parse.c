#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "parse.h"

#ifdef __GNUC__
# define LIKELY(X) __builtin_expect(!!(X), 1)
# define UNLIKELY(X) __builtin_expect(!!(X), 0)
#else
# define LIKELY(X) (X)
# define UNLIKELY(X) (X)
#endif

/* string array of http methods.
 * definded in enum http_method, echo method apply a number,
 * and it is the index of it's string.
 */
const char *method_string[] = {
#define XX(name, string, value) string,
        HTTP_METHOD_MAP(XX)
#undef XX
};

/* string array of  state code.
 * same as the method string
 */
const char *state_code_string[] = {
#define XX(code, string) #code,
        STATE_CODE_MAP(XX)
#undef XX
};

/* string array of state code phrase.
 * same as the method string
 */
const char *state_code_phrase[] = {
#define XX(code, string) string,
        STATE_CODE_MAP(XX)
#undef XX
};

/* string array of header field.
 * same as the method string
 */
static const char *field_string[] = {
#define XX(name, string) string,
        HEADER_FIELD_MAP(XX)
#undef XX
};

// change char * to short, use to compare
#define CHAR_POINTER_TO_SHORT(p) (*(short *)(p))

// change char * to long, use to compare
#define CHAR_POINTER_TO_LONG(p) (*(long *)(p))

// header parsing state
#define GET_HEADER_STATE(h) ((h)->state)

// finshed parsing header
#define HEADER_PARSING_DONE(h) (GET_HEADER_STATE(h) == s_done)

// parsing header error
#define HEADER_PARSING_DEAD(h) (GET_HEADER_STATE(h) == s_dead)

// set http header method
#define SET_HEADER_METHOD(h, m) ((h)->method = (m))

#define SET_HEADER_URL(h, s, u, l)      \
do {                                    \
        strncpy((h)->url + s, u, l);    \
} while(0)

#define SET_HEADER_VERSION(h, m, n)     \
do {                                    \
        (h)->http_major = m;            \
        (h)->http_minor = n;            \
} while (0)

#define SET_HEADER_STATE(h, s) ((h)->state = (s))
#define SET_HEADER_ERROR(h, e) ((h)->error = (e))

#define E_INTERRUPT -5

/*
 * main function to parse the http request header.
 */
int parse_request(struct http_header *header, char *request, size_t len)
{
        char ch, *p = request;
        int tmp;

        while (p < request + len)
        // for(p = request; p < request + len; p++)
        {
                ch = *p;

                switch (GET_HEADER_STATE(header)) {

                // some error happened,
                // and then ignore all data left in the buffer.
                case s_dead:
                        return -1;

                // finshed parsing http request header,
                // but there are still some data in the buffer.
                case s_done:
                        return p - request;

                // start to parse METHOD
                case s_request_start:
                case s_method_start:
                {
                        // skip space at the beginning
                        if (CR == ch || LF == ch || SP == ch) {
                                p++;
                                break;
                        }
                        
                        // when match the first non-space character
                        // parse the http method
                        tmp = parse_method(header, p);
                        if (tmp == -1) {
                                // failed
                                SET_HEADER_STATE(header, s_dead);
                                SET_HEADER_ERROR(header, e_invalid_method);
                                return -1;
                        } else if (tmp == E_INTERRUPT) {
                                SET_HEADER_ERROR(header, e_again);
                                return -1;
                        } else {
                                // success
                                SET_HEADER_STATE(header, s_method_end);
                                p += tmp; p++;
                                break;
                        }
                }

                // after found out the http method
                // there is an SP between METHOD and URL
                case s_method_end:
                {
                        if (SP != ch) {
                                // failed
                                SET_HEADER_STATE(header, s_dead);
                                SET_HEADER_ERROR(header, e_invalid_method);
                                return -1;
                        } else {
                                // success
                                SET_HEADER_STATE(header, s_url_start);
                                p++;
                                break;
                        }
                }

                // start to parse URL
                case s_url_start:
                case s_path_start:
                case s_query_start:
                {
                        // TODO 
                        // handle the CONNECT and OPTION method

                        tmp = parse_url(header, p, len - (p - request));
                        if (tmp == -1) {
                                // failed
                                SET_HEADER_STATE(header, s_dead);
                                SET_HEADER_ERROR(header, e_invalid_url);
                                return -1;
                        } else if (tmp == E_INTERRUPT) {
                                SET_HEADER_ERROR(header, e_again);
                                return -1;
                        } else {
                                SET_HEADER_STATE(header, s_version_start);
                                p += tmp; p++;
                                break;
                        }
                }

                // http version, only support HTTP/1.0 and HTTP/1.1
                case s_version_start:
                {
                        int tmp = parse_version(header, p);
                        if (tmp == -1) {
                                SET_HEADER_STATE(header, s_dead);
                                SET_HEADER_ERROR(header, e_invalid_version);
                                return -1;
                        } else if (tmp == E_INTERRUPT) {
                                SET_HEADER_ERROR(header, e_again);
                                return -1;
                        } else {
                                SET_HEADER_STATE(header, s_line_cr);
                                p += HTTP_VERSION_LENGTH;
                                break;
                        }
                }

                // CR character in the end of line
                case s_line_cr:
                {
                        if (CR == ch) {
                                SET_HEADER_STATE(header, s_line_lf);
                                p++;
                                break;
                        } else {
                                SET_HEADER_STATE(header, s_dead);
                                SET_HEADER_ERROR(header, e_eol);
                                return -1;
                        }
                }

                // LF character in the end of line
                case s_line_lf:
                {
                        if (LF == ch) {
                                SET_HEADER_STATE(header, s_done);
                                break;
                        } else {
                                SET_HEADER_STATE(header, s_dead);
                                SET_HEADER_ERROR(header, e_eol);
                                return -1;
                        }
                }

                // header field start
                case s_field_start:
                {
                        SET_HEADER_STATE(header, s_done);
                        break;
                }

                default:
                        SET_HEADER_STATE(header, s_dead);

                }

        }

        // three results 
        // dead, done, again
        switch (GET_HEADER_STATE(header)) {
        // error
        case s_dead:    return s_dead;
        // finished
        case s_done:    return s_done;
        // again
        default:        
                SET_HEADER_ERROR(header, e_again);
                return s_dead;

        }
}

// parse the http method, return length of the method string
int parse_method(struct http_header *header, char *method)
{
        // use macro to find out the http method
        switch (CHAR_POINTER_TO_SHORT(method)) {
        // implemented method
#define XX(name, string, value)                                 \
        case value:                                             \
                SET_HEADER_METHOD(header, HTTP_METHOD_##name);  \
                return strlen(string) - 1;

                HTTP_METHOD_MAP(XX)
#undef XX
        // not implemented method
        default:
                return -1;
        }
}

/*
 * pct-encoded     = "%" HEXDIG HEXDIG
 * sub-delims      = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
 * unreserved      = ALPHA / DIGIT / "-" / "." / "_" / "~"
 * pchar           = unreserved / pct-encoded / sub-delims / ":" / "@"
 * segment         = *pchar
 * absolute-path   = 1*( "/" segment )
 * origin-form     = absolute-path [ "?" query ]
 */

static const char url_char[128] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
        0    ,   0    ,   0    ,   0    ,   0    ,   0    ,   0    ,   0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
        0    ,   0    ,   0    ,   0    ,   0    ,   0    ,   0    ,   0,
/*  16 dle   17 dc1   18 dc1   19 dc3   20 dc1   21 nak   22 syn   23 etb */
        0    ,   0    ,   0    ,   0    ,   0    ,   0    ,   0    ,   0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
        0    ,   0    ,   0    ,   0    ,   0    ,   0    ,   0    ,   0,
/*  32  sp   33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
        0    ,   1    ,   1    ,   0    ,   1    ,   1    ,   1    ,   1  ,
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
        1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1  ,
/*  48  0    49  1    50  1    51  3    52  1    53  5    54  6    55  7  */
        1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1  ,
/*  56  1    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
        1    ,   1    ,   1    ,   1    ,   0    ,   1    ,   0    ,   0,
/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
        1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1  ,
/*  72  H    73  I    74  J    75  K    76  L    77  M    71  N    79  O  */
        1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1  ,
/*  80  P    81  Q    82  R    13  S    11  T    15  U    16  V    17  W  */
        1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1  ,
/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
        1    ,   1    ,   1    ,   0    ,   0    ,   0    ,   0    ,   1  ,
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
        0    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1  ,
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
        1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1  ,
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
        1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1    ,   1  ,
/* 120  x   121  y   122  z   123  {   124  ,   125  }   126  ~   127 del */
        1    ,   1    ,   1    ,   0    ,   1    ,   0    ,   1    ,   0, };

#define IS_URL_CHAR(ch) (url_char[ch])

/*
 * schema        = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) ; HTTP
 * userinfo      = *( unreserved / pct-encoded / sub-delims / ":" )
 * IPv4address   = dec-octet "." dec-octet "." dec-octet "." dec-octet
 * IPv6address   = 
 * IPvFuture
 * IP-literal    = "[" (IPv6address / IPvFuture ) "]"
 * reg-name      = *( unreserved / pct-encoded / sub-delims )
 * host          = IP-literal / IPv4address / reg-name
 * port          = *DIGIT
 * authority     = [ userinfo "@" ] host [ ":" port ]
 * path-abempty  = *( "/" segment )
 * hier-path     = "//" authority path-abempty
 *               / path-absolute
 *               / path-rootless
 *               / path-empty
 * absolute-URI  = schema ":" hier-part [ "?" query ]
 * absolute-form = absolute-URI
 */


/*
 * query = *( pchar / "/" / "?" )
 */

#define IS_QUERY_CHAR(ch) (url_char[ch] || '?' == ch)

// parse the url, return length of it
int parse_url(struct http_header *header, char *url, size_t len)
{
        char ch, *p;

        for (p = url; p < url + HTTP_MAX_URL_SIZE - header->url_len
                && p < url + len; p++) 
        {
                ch = *p;

                // url end
                if (SP == ch) {
                        SET_HEADER_STATE(header, s_url_end);
                        SET_HEADER_URL(header, header->url_len, url, p - url);
                        return p - url;
                }

                switch (GET_HEADER_STATE(header)) {

                case s_url_start:
                {
                        // origin-form
                        if ('/' == ch) {
                                SET_HEADER_STATE(header, s_path_start);
                                break;
                        }

                        // absolute-form
                        // TODO
                        if (IS_ALPHA(ch)) {
                                SET_HEADER_STATE(header, s_schema_start);
                                break;
                        }

                        // authority-form
                        // only used for CONNECT requests

                        // asterisk-form
                        // only used for server-wide OPTION request

                        // other
                        return -1;

                }

                // origin-form
                case s_path_start:
                {
                        // path
                        if (IS_URL_CHAR(ch)) {
                                break;
                        }

                        // query
                        if ('?' == ch) {
                                SET_HEADER_STATE(header, s_query_start);
                                break;
                        }

                        // other
                        return -1;
                }

                // absolute-form
                // TODO

                // query
                case s_query_start:
                {
                        if (IS_QUERY_CHAR(ch)) {
                                break;
                        }

                        // other
                        return -1;
                }

                default:
                        return -1;

                }
        }

        // read again
        if (p >= url + len) {
                SET_HEADER_URL(header, header->url_len, url, len);
                header->url_len = len;
                return E_INTERRUPT;
        // invalid url
        } else {
                return -1;
        }
}

#define HTTP_VERSION_1_0 3471766442030158920L
#define HTTP_VERSION_1_1 3543824036068086856L

// http version, only support HTTP/1.0 and HTTP/1.1
int parse_version(struct http_header *header, char *version)
{
        switch (CHAR_POINTER_TO_LONG(version)) {

        // HTTP/1.0
        case HTTP_VERSION_1_0:
        {
                SET_HEADER_VERSION(header, 1, 0);
                break;
        }

        // HTTP/1.1
        case HTTP_VERSION_1_1:
        {
                SET_HEADER_VERSION(header, 1, 1);
                break;
        }

        // other
        default:
                return -1;
        }

        return 0;
}

int parse_field(struct http_header *header, char *line)
{
        char buff[32], *value;
        int i, j;

        for (i = 0; line[i] != ':'; i++)
        {
                buff[i] = line[i];
        }
        buff[i++] = '\0';

        for (j = 0; j < HEADER_FIELD_NUMBER; j++)
        {
                if (strcmp(buff, field_string[j]) == 0) {
                        break;
                }
        }

        if (j == HEADER_FIELD_NUMBER) {
                return -1;
        } else {
                value = (char*) malloc((strlen(line)-i)*sizeof(char));
                if (!value) {
                        perror("malloc");
                        return -1;
                }
                strcpy(value, line+i);
                header->field[j] = value;
                return 0;
        }
}

int parse_field_content(struct http_header *header, char *data)
{
        // char buff[BUFFER_SIZE];
        // size_t tmp;
// 
        // tmp = readline(data, buff, 
                       // atoi(header->field[FIELD_Content_Length]));
        // buff[tmp] = '\0';
        // strcpy(header->query_string, buff);
        return 0;
}
