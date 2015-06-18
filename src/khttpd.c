#include "khttpd.h"

void die(const char* str)
{
        fprintf(stderr, "%s\n", str);
        exit(1);
}

int create_and_bind(char *port)
{
        int listen_sock, s, on;
        struct addrinfo hints;
        struct addrinfo *result, *rp;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags    = AI_PASSIVE;

        if (getaddrinfo(NULL, port, &hints, &result) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
                return -1;
        }

        for (rp = result; rp != NULL; rp = rp->ai_next)
        {
                listen_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (listen_sock == -1) {
                        continue;
                }

                on = 1;
                setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );

                if (bind(listen_sock, rp->ai_addr, rp->ai_addrlen) == 0) {
                        break;
                } else {
                        fprintf(stderr, "bind: %s\n", strerror(errno));
                }

                close(listen_sock);
        }

        if (rp == NULL) {
                fprintf(stderr, "Could not bind\n");
                return -1;
        }

        freeaddrinfo(result);

        return listen_sock;
}

int handle_connect(int listen_sock, int epollfd)
{
        while(1)
        {
                int connect_sock;
                struct sockaddr client_addr;
                socklen_t client_addr_len;
                struct epoll_event event;

                client_addr_len = sizeof(client_addr);
                connect_sock    = accept(listen_sock, &client_addr, &client_addr_len);
                if (connect_sock == -1) {
                        if ((errno = EAGAIN) || 
                            (errno = EWOULDBLOCK)) {
                                break;
                        } else {
                                perror("accept");
                                break;
                        }
                }

                if (set_non_blocking(connect_sock) == -1) {
                        abort();
                }

                event.data.fd = connect_sock;
                event.events  = EPOLLIN | EPOLLET;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connect_sock, &event) == -1) {
                        perror("epoll_ctl");
                        abort();
                }
        }
}

size_t get_string(char *line, char *buff, int index)
{
        int start, end, ch;
        int i;
}

static const char *method_string[] = {
#define XX(name, string) string,
        HTTP_METHOD_MAP(XX)
#undef XX
};

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
                        break;          // 不支持 url 参数
                } else {
                        header->url[i] = url[i];
                }
        }
        header->url[i] = '\0';

        if (header->url[i-1] == '/') {
                strcat(header->url, INDEX_FILE);
        }

        memset(buff, 0, sizeof(buff));

        strcat(buff, ROOT_DOCUMENT);
        strcat(buff, header->url);


        if (access(buff, F_OK) != 0) {
                header->state_code = STATE_CODE_404;
                return -1;
        } else if (access(buff, R_OK) != 0) {
                header->state_code = STATE_CODE_403;
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

        if (header->http_major != 1 && (header->http_minor !=0 || 
                                header->http_minor != 1)) {
                header->state_code = STATE_CODE_505;
                return -1;
        }
        return 0;
}

int parse_request_line(struct http_header *header, char *line)
{
        char method[MAX_HTTP_METHOD_LENGTH];
        char url[MAX_URL_LENGTH];
        char version[HTTP_VERSION_LENGTH];

        if (strlen(line) > MAX_HTTP_METHOD_LENGTH + 
                           MAX_URL_LENGTH + 
                           HTTP_VERSION_LENGTH) {
                header->state_code = STATE_CODE_414;
                return -1;
        }

        sscanf(line, "%s %s %s", method, url, version);

        if (parse_method(header, method) == -1) {
                printf("method\n");
                return -1;
        }

        if (parse_url(header, url) == -1) {
                printf("url\n");
                return -1;
        }

        if (parse_version(header, version) == -1) {
                printf("version\n");
                return -1;
        }

        return 0;
}

int handle_request(int connect_sock, int epollfd)
{
        char data[BUFFER_SIZE];
        char buff[128];
        ssize_t nread;
        int line = 0, tmp;
        struct http_header header;
        struct epoll_event event;

        nread = readn(connect_sock, data, sizeof(data));
        switch (nread) {
                case READ_AGAIN   : break; // read again
                case CLIENT_CLOSE : break; // client close
                default : break;           // default
        }

        line = readline(data + line, buff, sizeof(buff));
        if (parse_request_line(&header, buff) == -1)
                printf("error\n");

        while (1)
        { 
                tmp = readline(data + line, buff, sizeof(buff));
                if (tmp == 2) break;
                line += tmp;
                // TODO
        }

        event.data.fd = connect_sock;
        event.events  = EPOLLOUT | EPOLLET;
        if (epoll_ctl(epollfd, EPOLL_CTL_MOD, connect_sock, &event) == -1) {
                perror("epoll_ctl");
                abort();
        }
}

int handle_response(int connect_sock, int epollfd)
{
        char buff[512];
        size_t len;

#define header_404 "HTTP/1.1 404 Not Found" \
        "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Not found</h1>"
        sprintf(buff, "%s\r\n", header_404);
        len = strlen(buff);

        writen(connect_sock, buff, &len);

        close(connect_sock);

        return 0;
}

void sighandler(int sig) {
        exit(0);
}

int main(int argc, char* argv[])
{
        int listen_sock, epollfd;
        struct epoll_event event;
        struct epoll_event *events;

        signal(SIGABRT, &sighandler);
        signal(SIGTERM, &sighandler);
        signal(SIGINT,  &sighandler);

        if (argc != 2) {
                fprintf(stderr, "Usage: %s port\n", argv[0]);
                exit(EXIT_FAILURE);
        }

        listen_sock = create_and_bind(argv[1]);
        if (listen_sock == -1) {
                abort();
        }

        if (set_non_blocking(listen_sock) == -1) {
                abort();
        }

        if (listen(listen_sock, SOMAXCONN) == -1) {
                perror("listen");
                abort();
        }

        epollfd = epoll_create1(0);
        if (epollfd == -1) {
                perror("epoll_create");
                abort();
        }

        event.data.fd = listen_sock;
        event.events  = EPOLLIN | EPOLLET;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &event) == -1) {
                perror("epoll_ctl");
                abort();
        }

        events = (struct epoll_event*) malloc(MAXEVENTS * sizeof(struct epoll_event));

        while (1)
        {
                int n, i;

                n = epoll_wait(epollfd, events, MAXEVENTS, -1);
                if (n == -1) {
                        perror("epoll_wait");
                        continue;
                }

                for (i = 0; i < n; i++)
                {
                        if ((events[i].events & EPOLLERR) ||
                            (events[i].events & EPOLLHUP)) {
                                fprintf(stderr, "epoll error\n");
                                close(events[i].data.fd);
                                continue;
                        } else if (events[i].data.fd == listen_sock) {
                                handle_connect(listen_sock, epollfd);
                        } else {
                                if (events[i].events & EPOLLIN) {
                                        handle_request(events[i].data.fd, epollfd);
                                } else if (events[i].events & EPOLLOUT) {
                                        handle_response(events[i].data.fd, epollfd);
                                }
                        }
                }
        }

        free(events);

        close(listen_sock);

        return 0;
}
