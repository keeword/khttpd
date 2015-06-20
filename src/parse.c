#include "parse.h"

int parse_request_line(struct http_header *header, char *line)
{
        char method[MAX_HTTP_METHOD_LENGTH];
        char url[MAX_URL_LENGTH];
        char version[HTTP_VERSION_LENGTH];
        int state[3];

        if (strlen(line) > MAX_HTTP_METHOD_LENGTH + 
                           MAX_URL_LENGTH + 
                           HTTP_VERSION_LENGTH) {
                header->state_code = STATE_CODE_414;
                return -1;
        }

        sscanf(line, "%s %s %s", method, url, version);

        state[0] = parse_method(header, method);
        state[1] = parse_url(header, url);
        state[2] = parse_version(header, version);
        if ((state[0] == -1) || (state[1] == -1) || (state[2] == -1)) {
                return -1;
        }

        return 0;
}

int parse_version(struct http_header *header, char *version)
{
        int major, minor;
        char buff[HTTP_VERSION_LENGTH];
        int i;

        memset(buff, 0, sizeof(buff));
        strcat(buff, version+5);

        header->http_major = (int) atof(buff);
        header->http_minor = (int) ((atof(buff) - header->http_major) * 10);

        // only support HTTP/1.0 and HTTP/1.1
        if (header->http_major != 1 && (header->http_minor !=0 || 
                                header->http_minor != 1)) {
                header->state_code = STATE_CODE_505;
                return -1;
        }
        return 0;
}

int parse_url(struct http_header *header, char *url)
{
        int i;
        char dec, hex[3];
        char buff[2*MAX_URL_LENGTH];

        for (i = 0; url[i] != '\0'; i++)
        {
                if (IS_ALPHA(url[i])) {
                        header->url[i] = LOWER(url[i]);
                } else if (url[i] == '%'){
                        hex[0] = url[i+1];
                        hex[1] = url[i+2];
                        hex[2] = '\0';
                        sscanf(hex, "%x", header->url+i);
                        i += 2;
                } else if (url[i] == '?') {
                        parse_query_string(url+i+1, header->query_string);
                        break;          // 不支持中文 url 参数
                } else {
                        header->url[i] = url[i];
                }
        }
        header->url[i] = '\0';

        if (header->url[i-1] == '/') {
                strcat(header->url, INDEX_FILE);
        }

        memset(header->path, 0, sizeof(header->path));

        strcat(header->path, DOCUMENT_ROOT);
        strcat(header->path, header->url);

        if (access(header->path, F_OK) != 0) {
                header->state_code = STATE_CODE_404;
                return -1;
        } else if (access(header->path, R_OK) != 0) {
                header->state_code = STATE_CODE_403;
                return -1;
        }

        return 0;
}

int parse_query_string(char *str, char *buf)
{
        int i;
        char hex[3];
        for (i = 0; str[i] != '\0'; i++)
        {
                if (str[i] == '%') {
                        hex[0] = str[i+1];
                        hex[1] = str[i+2];
                        hex[2] = '\0';
                        sscanf(hex, "%x", buf+i);
                        i += 2;
                } else {
                        buf[i] = str[i];
                }
        }
        buf[i+1] = '\0';

        return i;
}

int parse_method(struct http_header *header, char *method)
{
        int i;

        for (i = 0; i < METHOD_NUM; i++)
        {
                if (strcmp(method, method_string[i]) == 0)
                        break;
        }

        if (METHOD_NUM == i) {
                header->state_code = STATE_CODE_405;
                return -1;
        } else {
                header->method = i;
                return 0;
        }
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
        char buff[BUFFER_SIZE];
        size_t tmp;

        tmp = readline(data, buff, 
                       atoi(header->field[FIELD_Content_Length]));
        buff[tmp] = '\0';
        strcpy(header->query_string, buff);
        return 0;
}
