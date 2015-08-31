#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include "process.h"

#define header_temp "Content-Type: text/html\r\nConnection: Close\r\n"

long get_file_size(char *filename)
{
        struct stat buf;
        if (stat(filename, &buf) < 0) {
                perror("stat");
                return -1;
        }
        return (long) buf.st_size;
}

size_t serve_file(struct http_header *header, char **output)
{
        size_t total_len, header_len, content_len;
        int filefd;
        char *buff;
        
        header_len  = strlen(header_temp);
        content_len = get_file_size(header->path);
        total_len   = header_len + content_len + 2;

        buff = (char*) malloc((total_len+16)*sizeof(char));
        if (!buff) {
                perror("malloc");
                return -1;
        }

        sprintf(buff, "%s\r\n", header_temp);

        filefd = open(header->path, O_RDONLY);
        if ((content_len = read(filefd, buff+header_len+2, content_len)) == -1) {
                fprintf(stderr, "%s\n", strerror(errno));
                abort();
        }

        total_len = content_len+header_len+2;
        buff[total_len] = '\0';

        *output = buff;
        return total_len;
}

extern char **method_string;
size_t execute_cgi(struct http_header *header, char **output)
{
        int cgi_output[2];
        int cgi_input[2];
        int status;
        pid_t pid;
        char ch;

        if (pipe(cgi_input) < 0) {
                perror("pipe");
                return -1;
        }

        if (pipe(cgi_output) < 0) {
                perror("pipe");
                return -1;
        }

        if ((pid = fork()) < 0) {
                perror("fork");
                return -1;
        }

        if (pid == 0) { // CGI 脚本
                char buff[BUFFER_SIZE];

                dup2(cgi_input[0], STDIN_FILENO);
                dup2(cgi_output[1], STDOUT_FILENO);

                close(cgi_input[1]);
                close(cgi_output[0]);

                sprintf(buff, "REQUEST_METHOD=%s", method_string[header->method]);
                putenv(buff);

                sprintf(buff, "QUERY_STRING=%s", header->query_string);
                putenv(buff);

                execl(header->path, header->path, NULL);
                exit(0);
        } else {
                close(cgi_output[1]);
                close(cgi_input[0]);

                int i = 0;
                char *buff;
                buff = (char *) malloc(BUFFER_SIZE*sizeof(char));
                if (!buff) {
                        return -1;
                }

                sprintf(buff, "%s\r\n", header_temp);
                i = strlen(buff);
                while (read(cgi_output[0], &ch, 1) > 0)
                {
                        buff[i++] = ch;
                }
                buff[i] = '\0';

                close(cgi_output[0]);
                close(cgi_input[1]);
                waitpid(pid, &status, 0);

                *output = buff;

                return i;
        }
}

size_t process_200(struct http_header *header, char **output)
{
        if (header->method == HTTP_METHOD_POST ||
            access(header->path, X_OK) == 0) {
                header->cgi = 1;
        } else {
                header->cgi = 0;
        }

        if (header->cgi) {
                return execute_cgi(header, output);
        } else {
                return serve_file(header, output);
        }
}

size_t process_error(struct http_header *header, char **output, char *str)
{
        char *buff;
        size_t len;

        buff = (char*) malloc(BUFFER_SIZE*sizeof(char));
        if (!buff) {
                perror("malloc");
                return -1;
        }

        len = sprintf(buff, "%s", str);

        *output = buff;
        return len;
}

#define header_400 "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Bad request</h1>"
size_t process_400(struct http_header *header, char **output)
{
        return process_error(header, output, header_400);
}
#define header_403 "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Forbidden</h1>"
size_t process_403(struct http_header *header, char **output)
{
        return process_error(header, output, header_400);
}
#define header_404 "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Not Found</h1>"
size_t process_404(struct http_header *header, char **output)
{
        return process_error(header, output, header_404);
}
#define header_405 "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Method Not Allowed</h1>"
size_t process_405(struct http_header *header, char **output)
{
        return process_error(header, output, header_405);
}
#define header_414 "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Request URI Too Long</h1>"
size_t process_414(struct http_header *header, char **output)
{
        return process_error(header, output, header_414);
}
#define header_505 "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>HTTP Version Not Supported</h1>"
size_t process_505(struct http_header *header, char **output)
{
        return process_error(header, output, header_505);
}

extern char **state_code_string;
extern char **state_code_phrase;

int process_response_line(struct http_header *header, char *buff)
{
        return sprintf(buff, "HTTP/%d.%d %s %s\r\n", 
                       header->http_major,
                       header->http_minor,
                       state_code_string[header->state_code],
                       state_code_phrase[header->state_code]);
}

