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
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "khttpd.h"
#include "list.h"
#include "io.h"
#include "parse.h"
#include "process.h"

static struct client clients;
static struct connector list_head;
extern char **method_string;

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

// client connect server
int handle_connect(int server_fd, int epollfd)
{
        while(1)
        {
                int client_fd;
                struct sockaddr client_addr;
                socklen_t client_addr_len;
                struct epoll_event event;

                // accept client
                client_addr_len = sizeof(client_addr);
                client_fd       = accept(server_fd, &client_addr, &client_addr_len);

                if (client_fd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                                break;
                        } else {
                                perror("accept");
                                abort();
                        }
                }

                // set non-blocking
                if (set_non_blocking(client_fd) == -1) {
                        abort();
                }

                event.data.fd = client_fd;
                event.events  = EPOLLIN | EPOLLET;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                        perror("epoll_ctl");
                        abort();
                }

                break;
        }

        return 0;
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

        // alloc space for a client, then insert it the list and
        // process it's request
        conn = (struct connector*) malloc(sizeof(struct connector));
        conn->connect_sock = connect_sock;
        header = &conn->header;

        // init struct http header
        http_header_init(header);
        header->state = s_request_start;

        list_add(&list_head, conn);

        // read request data to the buffer
        nread = readn(connect_sock, data, sizeof(data));
        // TODO
        switch (nread) {
                case READ_AGAIN   : break; // read again
                case CLIENT_CLOSE : break; // client close
                default :           break; // default
        }

        // parse the request data
        if (parse_request(header, data, nread) == -1) {
                // need to read again
                if (header->error == e_again) {
                        return 0;
                }
        }

        printf("%d\n", header->method);
        printf("%s\n", header->url);
        printf("HTTP/%d.%d\n", header->http_major, header->http_minor);

        // associated the connect socket for write
        // event.data.fd = connect_sock;
        // event.events  = EPOLLOUT | EPOLLET;
        // if (epoll_ctl(epollfd, EPOLL_CTL_MOD, connect_sock, &event) == -1) {
                // perror("epoll_ctl");
                // abort();
        // }

        return 0;
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

        // handle some signal
        signal(SIGABRT, &sighandler);
        signal(SIGTERM, &sighandler);
        signal(SIGINT,  &sighandler);

        if (argc != 2) {
                fprintf(stderr, "Usage: %s port\n", argv[0]);
                exit(EXIT_FAILURE);
        }

        // create socket and bind it to the port
        listen_sock = create_and_bind(argv[1]);
        if (listen_sock == -1) {
                abort();
        }

        // set socket non-blocking
        if (set_non_blocking(listen_sock) == -1) {
                abort();
        }

        // listen socket
        if (listen(listen_sock, SOMAXCONN) == -1) {
                perror("listen");
                abort();
        }

        // create epoll file descriptor
        epollfd = epoll_create1(0);
        if (epollfd == -1) {
                perror("epoll_create");
                abort();
        }

        // associated socket file descriptor to epoll event
        event.data.fd = listen_sock;
        // avaliable for read and,
        // set the Edge Triggered behavior for the associated file descriptor
        event.events  = EPOLLIN | EPOLLET;
        // register the socket to the epoll instance,
        // and associated the event.
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &event) == -1) {
                perror("epoll_ctl");
                abort();
        }

        // array to store the events happened during run-time
        events = (struct epoll_event*) malloc(MAXEVENTS * sizeof(struct epoll_event));

        list_head.connect_sock = 0;
        list_init(&list_head);

        while (1)
        {
                int n, i;

                // wait for event happen,
                // and get the event from events
                n = epoll_wait(epollfd, events, MAXEVENTS, -1);
                if (n == -1) {
                        perror("epoll_wait");
                        continue;
                }

                for (i = 0; i < n; i++)
                {
                        // some error
                        if ((events[i].events & EPOLLERR) ||
                            (events[i].events & EPOLLHUP)) {
                                fprintf(stderr, "epoll error\n");
                                close(events[i].data.fd);
                                continue;
                        // a client connect the server
                        } else if (events[i].data.fd == listen_sock) {
                                handle_connect(listen_sock, epollfd);
                        // we can send response to the client
                        } else if (events[i].events & EPOLLOUT) {
                                handle_response(events[i].data.fd, epollfd);
                        // a http request arrive
                        } else if (events[i].events & EPOLLIN) {
                                handle_request(events[i].data.fd, epollfd);
                        }
                }
        }

        free(events);

        close(listen_sock);

        return 0;
}
