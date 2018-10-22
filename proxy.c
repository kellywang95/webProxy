#include "csapp.h"
#include "cache.h"
#include "proxy.h"
#include "sbuf.h"
#include <stdio.h>
#include <stdlib.h>

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";
static const char *default_http_version = "HTTP/1.0\r\n";

sbuf_t sbuf; 
static cache_list *cl;

int main(int argc, char **argv)	
{
    printf("----FUNCTION: main----\n");

    int listenfd,connfd,i;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    clientlen = sizeof(clientaddr);

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutex_lock,NULL);

    /* Initialize cache */
    cl = (cache_list *)malloc(sizeof(cache_list));
    init_cache_list(cl);

    /* A SIGPIPE is sent to a process if it tried to write to a socket that had been shut down for writing or isn't connected anymore*/
    /* Ignore SIGPIPE signal , then the send*()/write() would return -1 and set errno to EPIPE */
    Signal(SIGPIPE, SIG_IGN);

    sbuf_init(&sbuf,SBUFSIZE); 

    printf("Initialize the thread, the max thread number is %d\n", NTHREADS);
    for(i = 0; i<NTHREADS; i++){
        Pthread_create(&tid,NULL,thread,NULL);
    }

    /* Open listening port */
    printf("Opening listening port\n");
    listenfd = Open_listenfd(argv[1]);

    while(1){
    	connfd = Accept(listenfd,(SA *)&clientaddr,&clientlen);
        sbuf_insert(&sbuf,connfd);
    }
    pthread_mutex_destroy(&mutex_lock);
}

/* 
 * New thread to process the request 
 */
void *thread(void* vargp) {
    /* Detach the new thread so that it can be handled automatically after it finishes to avoid memory leak */
    Pthread_detach(pthread_self());
    while (1) {
        int clientfd = sbuf_remove(&sbuf);
        process_request(clientfd);
    }
}

/* 
 * Process a request from a connection socket with cache mechanism
 */
 void *process_request(int fd){
    printf("----FUNCTION: process_request----\n");

    Request req;
    Response resp;
    int content_size = 0;
    char* content_copy = NULL;

    parse_request(fd,&req);

    /* Check if cache exists*/
    char *id = (char*)malloc(sizeof(req.req_str)+sizeof(req.host_str));
    strcpy(id,req.req_str);
    strcat(id,req.host_str);
    content_copy = read_cache(cl,id,&content_size);

    /* Cache hit: send cached response back to client */
    if(content_size>0){
        if(content_copy==NULL){
            printf("Content in cahce error\n");
            return NULL;
        }
        printf("Cache Hit. Write response to client\n");
        /* Write Response to Client */
        if(rio_writen(fd,content_copy,content_size)<0){
            client_error(fd,"","404","Not found","Unable to write response to client");
            return NULL;
        }
    }else{
        /* Cache miss: send request to server to get the response */
        printf("Cache Miss\n");
        if(forward_request(fd,&req,&resp)<0){
            close(fd);
            return NULL;
        }
        /* Cache the response if it fits in the max object size */
        printf("The response content size is %d\n", resp.content_size);
        if(resp.content_size<=MAX_OBJECT_SIZE){
            update_cache(cl,id,resp.content,resp.content_size);
        }
        
    }
    free(id);
    close(fd);
    return NULL;
 }

/* 
 * Parse the http request
 */
void parse_request(int fd, Request *req){
    printf("----FUNCTION: parse_request----\n");
    char buffer[MAXLINE], full_req[MAXLINE], method[MAXLINE], url[MAXLINE], host[MAXLINE], uri[MAXLINE], version[MAXLINE];
    int port;
    rio_t rio;

    rio_readinitb(&rio, fd);
    if(mrio_readlineb(&rio, buffer, MAXLINE)<0){
        printf("rio_readlineb error\n");
        return;
    }

    sscanf(buffer,"%s %s %s", method, url, version);
    printf("The method, uri and version from the request line is %s,%s,%s\n", method,url,version);

    /* Check if the method is GET. */
    if(strcasecmp(method,"GET")) return;

    if(strcmp(version,"HTTP/1.0")){
        strcpy(version, "HTTP/1.0");
    }

    sprintf(full_req,"%s %s %s", method, url, version);
    
    parse_url(url,host,&port,uri);

    strcpy(req->full_req,full_req);
    strcpy(req->method,method);
    strcpy(req->host,host);
    strcpy(req->uri,uri);
    strcpy(req->version,version);
    req->port = port;

    sprintf(req->req_str,"%s %s %s", method, uri, default_http_version);
    sprintf(req->host_str,"Host:%s\r\n",host);
    
    
    strcat(req->req_str, user_agent_hdr);
    strcat(req->req_str, accept_hdr);
    strcat(req->req_str, accept_encoding_hdr);
    strcat(req->req_str, connection_hdr);
    strcat(req->req_str, proxy_connection_hdr);

    printf("----FUNCTION: parse_reqeust end, the request is----\n");
    printf("full_req: %s\n",req->full_req);
    printf("method: %s\n",req->method);
    printf("host: %s\n",req->host);
    printf("uri: %s\n",req->uri);
    printf("version: %s\n",req->version);
    printf("port: %d\n",req->port);
    printf("req_str: %s\n",req->req_str);
    printf("host_str: %s\n",req->host_str);
}

/* 
 *  Process the url string
 */
void parse_url(char *url, char *host, int *port, char *uri){
    printf("----FUNCTION: parse_url----\n");
    char buf[MAXLINE];
    char *port_loc;
    char *uri_loc;
    *port = 80;

    printf("The url is %s\n", url);
    sscanf(url,"http://%s",buf);
    printf("The buf is %s\n", buf);

    /* Parse URI if / is found */
    uri_loc=strstr(buf,"/");
    printf("The uri_loc is %s\n",uri_loc );
    if(uri_loc!=NULL){
        sscanf(uri_loc,"%s",uri);
        *uri_loc = '\0';
    }else{
        uri[0] = '/';
        uri[1] = '\0';
    }

    /* Parse PORT if : is found */
    port_loc=strstr(buf,":");
    printf("The port_loc is %s\n",port_loc );
    if(port_loc != NULL){
        sscanf(port_loc+1,"%d",port);
        *port_loc = '\0';
    }

    strcpy(host,buf);
}

/* 
 * Forward the request to the server
 */
int forward_request(int clientfd, Request *req, Response *resp){
    printf("----FUNCTION: forward_request----\n");
    rio_t server_rio;
    int serverfd;
    char port[16];
    snprintf(port,sizeof(port),"%d",req->port);
    /* getaddrinfo must use string instead of int, otherwise, it will throw segmentation fault */
    if ((serverfd = open_clientfd(req->host, port)) < 0) {
        sleep(2);
        if ((serverfd = open_clientfd(req->host, port)) < 0) {
            sleep(2);
            if ((serverfd = open_clientfd(req->host, port)) < 0) {
                client_error(clientfd,req->host,"404","Not found","Proxy couldn't connect to this server");
                close(serverfd);
                return -1;
            }
        }
    }

    printf("Write Request to Server\n");
    /* Write request to server */
    rio_readinitb(&server_rio, serverfd);
    mrio_writen(serverfd, req->req_str, strlen(req->req_str));
    mrio_writen(serverfd, req->host_str, strlen(req->host_str));
    mrio_writen(serverfd, "\r\n", strlen("\r\n"));

    if(forward_response(clientfd,serverfd,resp)<0){
        close(serverfd);
        return -1;
    }
    close(serverfd);
    return 1;
}

/* 
 * Forward the response to the client
 */
int forward_response(int clientfd, int serverfd, Response *resp){
    size_t n;
    char header_buffer[MAXLINE], header[MAX_OBJECT_SIZE], content_buffer[MAX_OBJECT_SIZE], content[MAX_OBJECT_SIZE*10];
    rio_t server_rio;

    rio_readinitb(&server_rio,serverfd);

    /* Copy Response Header */
    int header_size = 0;
    while((n=mrio_readlineb(&server_rio,header_buffer,MAXLINE))!=0){
        memcpy(header+header_size,header_buffer,n*sizeof(char));
        header_size+=n;
        if(!strcmp(header_buffer,"\r\n")){
            break;
        }
    }

    /* Copy Response Content */
    int content_size = 0;
    while((n=mrio_readlineb(&server_rio,content_buffer,MAX_OBJECT_SIZE))!=0){
        memcpy(content+content_size,content_buffer,n*sizeof(char));
        content_size+=n;
    }

    /* Write Response to Client */
    printf("Write Response to Client\n");
    if(mrio_writen(clientfd,header,header_size)<0){
        client_error(clientfd,"","404","Not found","Unable to write response header to client");
        return -1;
    }
    if(mrio_writen(clientfd,content,content_size)<0){
        client_error(clientfd,"","404","Not found","Unable to write response content to client");
        return -1;
    }

    /* Malloc content if right size to be future cached */
    if(content_size<=MAX_OBJECT_SIZE){
        resp->content = Malloc(content_size*sizeof(char));
        memcpy(resp->content, content, content_size*sizeof(char));
        resp->content_size = content_size;
    } else{
        resp->content_size = content_size;
    }

    return 1;

}

/* 
 * Returns an error message to the client
 */
void client_error(int fd, char *cause, char *errnum, 
            char *shortmsg, char *longmsg) {
    printf("----FUNCTION: client_error----\n");
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Request Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The proxy</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

int mrio_writen(int fd, void *buf, size_t n){
    if(rio_writen(fd,buf,n)!=n){
        if(errno==EPIPE||errno==ECONNRESET){
            printf("Write Error %s\n", strerror(errno));
            return 0;
        }else{
            return -1;
        }
    }
    return n;
}

ssize_t mrio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen){
    ssize_t rc = rio_readlineb(rp,usrbuf,maxlen);
    if(rc<0){
        if(errno==EPIPE||errno==ECONNRESET){
            printf("Read Error %s\n", strerror(errno));
            return 0;
        }else{
            return -1;
        }
    }
    return rc;
}