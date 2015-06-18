#include "io.h"

int set_non_blocking(int sockfd)
{
        int flags;

        flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1) {
                perror("fcntl");
                return -1;
        }

        flags |= O_NONBLOCK;
        if (fcntl(sockfd, F_SETFL, flags) == -1) {
                perror("fcntl");
                return -1;
        }
        return 0;
}

ssize_t readn(int sockfd, char *buff, size_t nbytes)
{
        ssize_t nleft, nread;
        
        nleft = nbytes;
        while (nleft > 0) 
        {
                nread = read(sockfd, buff, nleft);
                if (nread == -1 ) {
                        if (errno == EINTR) {
                                // 从中断恢复后继续读取
                                continue;
                        } else {
                                // errno == EAGAIN or EWOULDBLOCK
                                // 已读取缓存中所有数据
                                // 等待下次再读取不足的数据
                                return READ_AGAIN;
                        }
                } else if (nread == 0) {
                        // 客户端关闭连接
                        return CLIENT_CLOSE;
                } else if (nread > 0) {
                        nleft -= nread;
                        buff  += nread;
                }
        }

        return (nbytes - nleft);
}

ssize_t readline(const char *data, char *buff, size_t maxlen)
{
        ssize_t nleft, nread;
        char tmp;

        nleft = maxlen;

        while (nleft > 0)
        {
                tmp = *data; data++;
                if (tmp == '\r') {
                        if (*data != '\n') {
                                return CRLF_ERROR;
                        } else {
                                // 成功读取一行
                                break;
                        }
                } else {
                        *buff = tmp; buff++;
                        nleft--;
                }
        }

        *buff = '\0';

        return (maxlen - nleft + 2);
}

ssize_t writen(int sockfd, char *buff, size_t *nbytes)
{
        ssize_t nleft, nwrite;

        nleft = *nbytes;
        while (nleft > 0)
        {
                nwrite = write(sockfd, buff, nleft);
                if (nwrite == -1) {
                        if (errno == EINTR) {
                                // 从中断恢复后继续写入
                                continue;
                        } else {
                                // errno == EAGAIN or EWOULDBLOCK
                                // 等待下次再写入剩余的数据
                                *nbytes -= nleft;
                                return WRITE_AGAIN;
                        }
                } else if (nwrite == 0) {
                        // 客户端关闭连接
                        *nbytes -= nleft;
                        return CLIENT_CLOSE;
                } else if (write > 0) {
                        nleft -= nwrite;
                        buff  += nwrite;
                }
        }

        return (*nbytes - nleft);
}

ssize_t writeline(char *data,const char *buff, size_t maxlen)
{
        if (strlen(data) + strlen(buff) > maxlen) {
                return NO_SPACE;
        }


        strcat(data, buff);

        return 0;
}
