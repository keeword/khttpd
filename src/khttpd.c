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

int set_non_blocking(int sock)
{
        int flags;

        flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) {
                perror("fcntl");
                return -1;
        }

        flags |= O_NONBLOCK;
        if (fcntl(sock, F_SETFL, flags) == -1) {
                perror("fcntl");
                return -1;
        }
        return 0;
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

size_t readn(int sockfd, char *buff, size_t nbytes)
{
        size_t nleft;
        size_t nread;
        
        nleft = nbytes;
        while (nleft > 0) 
        {
                if ( (nread = read(sockfd, buff, nleft)) <= 0 ) {
                        if (errno == EINTR) {
                                continue;
                        } else {
                                return -1;
                        }
                }

                nleft -= nread;
                buff  += nread;
        }

        return (nbytes - nleft);
}

size_t writen(int sockfd, char *buff, size_t nbytes)
{
        size_t nleft;
        size_t nwrite;

        nleft = nbytes;
        while (nleft > 0)
        {
                if ( (nwrite = write(sockfd, buff, nleft)) <= 0 ) {
                        if (errno == EINTR) {
                                continue;
                        } else {
                                return -1;
                        }
                }

                nleft -= nwrite;
                buff  += nwrite;
        }

        return (nbytes - nleft);
}

size_t readline(int sockfd, char *buff, size_t maxlen)
{
        size_t nleft;
        size_t nread;
        char tmp;

        nleft = maxlen;

        while (nleft > 0)
        {
                if ( (nread = read(sockfd, &tmp, 1)) != 1 ) {
                        if (errno == EINTR) {
                                continue;
                        } else {
                                fprintf(stderr, "%s", strerror(errno));
                                return -1;
                        }
                }

                if (tmp == '\r') {
                        nread = read(sockfd, &tmp, 1);
                        if ( (nread == 1) && (tmp == '\n') ) {
                                break;
                        }
                } else {
                        *buff++ = tmp;
                        nleft--;
                }

        }

        *buff = '\0';

        return (maxlen - nleft);
}

size_t writeline(int sockfd, char *buff)
{
        int i, nwrite;
        char ch[] = "\r\n";

        for (i = 0; buff[i] != '\0'; i++);

        nwrite =  writen(sockfd, buff, i);
        return 0;
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

int parse_method(struct http_header *header, char *line)
{
        int i;
        char buff[10];

        for (i = 0; line[i] != ' '; i++)
        {
                buff[i] = LOWER(line[i]);
        }
        buff[i] = '\0';

        for (i = 0; i < METHOD_NUM; i++)
        {
                if (strcmp(buff, method_string[i]) == 0)
                        break;
        }

        if (METHOD_NUM == i) {
                return -1;
        } else {
                header->method = i;
                return 0;
        }
}

int parse_url(struct http_header *header, char *line)
{
        char *buff;
        int i, j;

        buff = (char *) malloc(256*sizeof(char));

        for (i = 0; line[i] != ' '; i++)
        {
                buff[i] = LOWER(line[i]);
        }

        for (i++, j = 0; line[i] != ' '; i++, j++)
        {
                if (IS_ALPHA(line[i])) {
                        buff[j] = LOWER(line[i]);
                } else {
                        buff[j] = line[i];
                }
        }
        buff[j] = '\0';
        header->url = buff;
        return 0;
}

int parse_request_line(struct http_header *header, char *line)
{

}

int handle_request(int connect_sock, int epollfd)
{
        char buff[1024];
        struct http_header header;
        struct epoll_event event;

        readline(connect_sock, buff, sizeof(buff));
        parse_method(&header, buff);
        parse_url(&header, buff);

        while (readline(connect_sock, buff, sizeof(buff)))
        { 
                // printf("%s\n", buff);
        }

        event.data.fd = connect_sock;
        event.events  = EPOLLOUT | EPOLLET;
        if (epoll_ctl(epollfd, EPOLL_CTL_MOD, connect_sock, &event) == -1) {
                perror("epoll_ctl");
                abort();
        }

        // close(connect_sock);
}

int handle_response(int connect_sock, int epollfd)
{
        char buff[512];

#define header_200_start "HTTP/1.1 200 OK\r\nServer: clowwindyserver/1.0\r\n" \
        "Content-Type: text/html\r\nConnection: Close\r\n"

#define header_404 "HTTP/1.1 404 Not Found\r\nServer: clowwindyserver/1.0\r\n" \
        "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Not found</h1>"
        sprintf(buff, header_404);

        writeline(connect_sock, buff);

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
