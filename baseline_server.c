/*
 * 18-845 Internet Service
 * Baseline concurrent server
 * 
 * Implements HTTP/1.0 GET request for static and dynamic content.
 * Assumes one connection per request(no persistent connection).
 * Uses CGI protocol to serve dynamic content.
 * Serves HTML(.html), image(.jpg, .gif) and text(.txt) file.
 * Accept a command line argument(the port to listen).
 * Implements concurrency using either threads or I/O multiplexing.
 *
 * Yijie Ma(yijiem)
 *
 */

#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd, port, clientlen;
  struct sockaddr_in clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  port = atoi(argv[1]);

  listenfd = Open_listenfd(port);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);                                                       
    doit(connfd);                                                                                                    
    Close(connfd);                                                                                                    
  }
}

/*                                                                                                                                             
 * doit - handle one HTTP request/response transaction                                                                                         
 */
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // read request line and headers
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE); // read the first line of the request
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")) {                     // 501 method not implemented
    clienterror(fd, method, "501", "Not implemented",
		"Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); // print request header
  
  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {                     // 404 file not found, stat(): statistic function to calculate the size of file
    clienterror(fd, filename, "404", "Not found",
		"Tiny couldn't find this file");
    return;
  }

  if (is_static) { /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {  // 403 couldn't read the file                                        
      clienterror(fd, filename, "403", "Forbidden",
		  "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);                                                           
  }
  else { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 403 could't run CGI program                                             
      clienterror(fd, filename, "403", "Forbidden",
		  "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

/*
 * serve static content based on filetype(html, text, jpg, gif)
 */
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Baseline Server\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", filetype);
  Rio_writen(fd, buf, strlen(buf)); // write header

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);                                                                                      
  Rio_writen(fd, srcp, filesize); // write content body                                                    
  Munmap(srcp, filesize); 
}

/*
 * helper method for getting the file type
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpg");
  } else {
    strcpy(filetype, "text/plain");
  }
}

/*
 * Run a new CGI process to serve the dynamic content to client
 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Baseline Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  
  if (Fork() == 0) { // child
    // set CGI vars
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO); // redirect child process's stdout to client
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}

/*
 * read and print the request header
 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
  
  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/*
 * parse uri to determine between static or dynamic content(cgi arguments) 
 * and corresponding filename
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) { // static content
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/') {
      strcat(filename, "home.html");
    }
    return 1;
  } else { // dynamic content
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    } else {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void clienterror(int fd, char *cause, )
