#ifndef _PROCESS_H_
#define _PROCESS_H_

#include "parse.h"

long get_file_size(char *filename);
size_t serve_file(struct http_header *header, char **output);
size_t execute_cgi(struct http_header *header, char **output);
size_t process_200(struct http_header *header, char **output);
int process_response_line(struct http_header *header, char *buff);

size_t process_error(struct http_header *header, char **output, char *str);
size_t process_400(struct http_header *header, char **output);
size_t process_403(struct http_header *header, char **output);
size_t process_404(struct http_header *header, char **output);
size_t process_405(struct http_header *header, char **output);
size_t process_414(struct http_header *header, char **output);
size_t process_505(struct http_header *header, char **output);

#endif // _PROCESS_H_
