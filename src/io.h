#ifndef _IO_H_
#define _IO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>


#define CLIENT_CLOSE -2
#define READ_AGAIN   -3
#define WRITE_AGAIN  -4
#define CRLF_ERROR   -5
#define NO_SPACE     -6

int set_non_blocking(int sockfd);

ssize_t readn(int      sockfd, char   *buff, size_t nbytes);
ssize_t writen(int     sockfd, char   *buff, size_t *nbytes);
ssize_t readline(const char    *data, char   *buff, size_t maxlen);
ssize_t writeline(char *data,  const  char   *buff, size_t maxlen);

#endif
