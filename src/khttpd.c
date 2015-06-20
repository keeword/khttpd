#include "khttpd.h"
#include "list.h"
#include "io.h"
#include "parse.h"
#include "process.h"

static struct connector list_head;

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

int clean_up(struct connector *conn, int connect_sock)
{
        list_del(conn);
        free(conn);
        close(connect_sock);
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

int handle_request(int connect_sock, int epollfd)
{
        char data[BUFFER_SIZE];
        char buff[BUFFER_SIZE];
        ssize_t nread;
        size_t header_len;
        int line = 0, tmp;
        struct http_header *header;
        struct epoll_event event;
        struct connector *conn;

        conn = (struct connector*) malloc(sizeof(struct connector));
        conn->connect_sock = connect_sock;
        header = &conn->header;
        list_add(&list_head, conn);

        nread = readn(connect_sock, data, sizeof(data));
        // TODO
        switch (nread) {
                case READ_AGAIN   : break; // read again
                case CLIENT_CLOSE : break; // client close
                default :           break; // default
        }

        line = readline(data + line, buff, sizeof(buff));
        if (parse_request_line(header, buff) == -1) {
                // printf("error\n");
        } else {
                header->state_code = STATE_CODE_200;
        }

        memset(header->field, 0, HEADER_FIELD_NUMBER*sizeof(char*));

        while (1)
        { 
                tmp = readline(data + line, buff, sizeof(buff));
                parse_field(header, buff);
                if (tmp == 2) break;
                line += tmp;
                // TODO
        }
        header_len = line+2;

        for (int i = 0; i < HEADER_FIELD_NUMBER; i++)
        {
                if (header->field[i] == 0) {
                        continue;
                }
                switch(i) 
                {
                        case FIELD_Content_Length: 
                                parse_field_content(header, data+header_len);
                                break;
                        default: break;
                }
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
        struct connector *conn;
        struct http_header *header;
        char buff[BUFFER_SIZE];
        char response_header[BUFFER_SIZE];
        size_t response_line, len;
        char **output;

        conn   = list_find(&list_head, connect_sock);
        header = &conn->header;

        if (process_response_line(header, buff) == -1) {
                clean_up(conn, connect_sock);
                return -1;
        }
        response_line = sprintf(response_header, "%s", buff);

        switch (header->state_code)
        {
                case STATE_CODE_200: len = process_200(header, output); break;
                case STATE_CODE_403: len = process_403(header, output); break;
                case STATE_CODE_404: len = process_404(header, output); break;
                case STATE_CODE_405: len = process_405(header, output); break;
                case STATE_CODE_414: len = process_414(header, output); break;
                case STATE_CODE_505: len = process_505(header, output); break;
                default : len = process_400(header, output); break;
        }

        if (len == -1) {
                perror("process_200");
                clean_up(conn, connect_sock);
                return -1;
        } else {
                writen(connect_sock, response_header, &response_line);
                writen(connect_sock, *output, &len);
                clean_up(conn, connect_sock);
        }

        return 0;
}

void sighandler(int sig) 
{
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

        list_head.connect_sock = 0;
        list_init(&list_head);

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
