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
#include <dlfcn.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);
void *thread(void *vargp);
void Rio_writen_new(int fd, void *usrbuf, size_t n);
ssize_t Rio_readnb_new(rio_t *rp, void *usrbuf, size_t n);
ssize_t Rio_readlineb_new(rio_t *rp, void *usrbuf, size_t maxlen);

#define DEBUG 1

int main(int argc, char **argv)
{
  int listenfd, port, clientlen, *connfdp;
  struct sockaddr_in clientaddr;
  sigset_t mask;
  pthread_t tid;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  port = atoi(argv[1]);

  /* Block SIGPIPE */
  Sigemptyset(&mask);
  Sigaddset(&mask, SIGPIPE);
  Sigprocmask(SIG_BLOCK, &mask, NULL);

  listenfd = Open_listenfd(port);
  while (1) {
    connfdp = Malloc(sizeof(int));
    clientlen = sizeof(clientaddr);
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Pthread_create(&tid, NULL, thread, connfdp);                                                                                                
  }
}

void *thread(void *vargp)
{
  long tid = 0l;
  if (DEBUG) {
    tid = syscall(SYS_gettid);
    fprintf(stdout, "DEBUG: thread#:%ld\n", tid);
  }
  int fd = *((int *) vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(fd);
  Close(fd);
  if (DEBUG) {
    fprintf(stdout, "DEBUG: thread#:%ld end...\n\n", tid);
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
  Rio_readlineb_new(&rio, buf, MAXLINE); // read the first line of the request
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")) {                     // 501 method not implemented
    clienterror(fd, method, "501", "Not implemented",
		"Baseline Server does not implement this method");
    return;
  }
  read_requesthdrs(&rio); // print request header
  
  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {                     // 404 file not found, stat(): statistic function to calculate the size of file
    clienterror(fd, filename, "404", "Not found",
		"Couldn't find this file");
    return;
  }

  if (is_static) { /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {  // 403 couldn't read the file                                        
      clienterror(fd, filename, "403", "Forbidden",
		  "Couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);                                                           
  }
  else { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 403 could't run CGI program                                             
      clienterror(fd, filename, "403", "Forbidden",
		  "Couldn't run the CGI program");
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
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen_new(fd, buf, strlen(buf)); // write header

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);                                                                                      
  Rio_writen_new(fd, srcp, filesize); // write content body                                                    
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
 * Use dynamic linking to load the shared library at run time and call the function
 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], content[MAXLINE], arg1[MAXLINE], arg2[MAXLINE];
  void *handle;
  void (*add)(int, int, char *); // do we know this?
  int result, num1, num2;
  char *error, *ptr;
  
  handle = dlopen(filename, RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
    exit(1);
  }

  add = dlsym(handle, "add");
  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", error);
    exit(1);
  }
  
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen_new(fd, buf, strlen(buf));
  sprintf(buf, "Server: Baseline Server\r\n");
  Rio_writen_new(fd, buf, strlen(buf));
  
  if (strcmp(cgiargs, "")) {
    ptr = strchr(cgiargs, '&');
    *ptr = '\0';
    strcpy(arg1, cgiargs);
    strcpy(arg2, ptr + 1);
    num1 = atoi(arg1);
    num2 = atoi(arg2);
    add(num1, num2, content);
    Rio_writen_new(fd, content, strlen(content));
  }

  if (dlclose(handle) < 0) {
    fprintf(stderr, "%s\n", dlerror());
    exit(1);
  }
}



/*
 * Run a new CGI process to serve the dynamic content to client
 */
/*
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen_new(fd, buf, strlen(buf));
  sprintf(buf, "Server: Baseline Server\r\n");
  Rio_writen_new(fd, buf, strlen(buf));
  
  if (Fork() == 0) { // child
    // set CGI vars
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO); // redirect child process's stdout to client
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}
*/

/*
 * read and print the request header
 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
  
  Rio_readlineb_new(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb_new(rp, buf, MAXLINE);
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
    strcat(filename, ".so"); // modified to shared object file
    return 0;
  }
}

/*
 * Send back error message to client when specific types of error occurs
 */
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Baseline Server Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Baseline Server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen_new(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen_new(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen_new(fd, buf, strlen(buf));
  Rio_writen_new(fd, body, strlen(body));
}


/*****************************************************/
/* New wrapper series for Rio, ignore EPIPE error    */
/* so that broken pipe does not terminte the process */
/*****************************************************/
void Rio_writen_new(int fd, void *usrbuf, size_t n)
{
  if (rio_writen(fd, usrbuf, n) != n) {
    /* Ignore */
    return;
  }

}

ssize_t Rio_readnb_new(rio_t *rp, void *usrbuf, size_t n)
{
  size_t rc;

  if ((rc = rio_readnb(rp, usrbuf, n)) < 0) {
    /* Ignore */
    return rc;
  }

}

ssize_t Rio_readlineb_new(rio_t *rp, void *usrbuf, size_t maxlen)
{
  size_t rc;

  if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
    /* Ignore */
    return rc;
  }

}
