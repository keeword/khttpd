#ifndef _PARSE_H_
#define _PARSE_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "khttpd.h"
#include "io.h"

int parse_request_line(struct http_header *header, char *line);
int parse_version(struct http_header *header, char *version);
int parse_url(struct http_header *header, char *url);
int parse_query_string(char *str, char *buf);
int parse_method(struct http_header *header, char *method);
int parse_field(struct http_header *header, char *line);
int parse_field_content(struct http_header *header, char *data);

#endif // _PARSE_H_
