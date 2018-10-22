#include "csapp.h"

typedef struct{
	char full_req[MAXLINE];
	char method[MAXLINE];
	char host[MAXLINE];
	char uri[MAXLINE];
	char version[MAXLINE];
	char req_str[MAXLINE];
	char host_str[MAXLINE];
	int port;
}Request;

typedef struct{
	char *content;
	unsigned int content_size;
}Response;

void *thread(void* vargp);
void *process_request(int fd);
void parse_request(int fd, Request *req);
void parse_url(char *url, char *host, int *port, char *uri);
int forward_request(int clientfd, Request *req, Response *resp);
int forward_response(int clientfd, int serverfd, Response *resp);
void client_error(int fd, char *cause, char *errnum, 
            char *shortmsg, char *longmsg);
int mrio_writen(int fd, void *buf, size_t n);
ssize_t mrio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);


#define NTHREADS 100
#define SBUFSIZE 20