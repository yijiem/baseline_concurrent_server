#Yijie's learning MEMO for 18-845 Internet Services  
###About CGI(common gateway interface)  
CGI is a standard method used to generate dynamic content on Web pages. CGI, when implemented on web server, provides an interface between the Web server and the program that generate the dynamic content. Just a short code snippet to see how to use it:
```c
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };
    
    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) { /* child */
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
        Execve(filename, emptylist, environ); /* Run CGI program */
    }
    Wait(NULL); /* Parent waits for and reaps child */
}
```
The filename determines the location of our CGI program and we also set the cgiargs before program gets execute. Notice that the cgi result is write to standard output, so we redirect the pipe to our client indicated by fd(socket file descriptor).
<br>
<br>
<br>
<br>
###About dealing with Broken pipe  
Reading or writing to a socket which has already been closed on the other end(like: clicking "stop" on web browser) will result into SIGPIPE signal and EPIPE errno. Either of them will terminate the process. But we don't want web server to terminate due to that. So we should do 2 things: Block SIGPIPE signal and Ignore EPIPE error. Let's see how we do it:
```c
sigset_t mask;

/* Block SIGPIPE */
Sigemptyset(&mask);
Sigaddset(&mask, SIGPIPE);
Sigprocmask(SIG_BLOCK, &mask, NULL);


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
```
<br>
<br>
###About Dynamic Linking
Many web server generate dynamic contents which takes the user input and do some corresponding operation and send back the result. An old fashion of doing this is to use CGI that Fork a new child process and Execve the program under the context of the new process. This way is proved to be time-consuming. High preformanced web server can generate dynamic content more efficiently using dynamic linking.  
The idea is to package functions into shared library. When a request come to the server, the server links the shared library at run time and call it directly under the context of the web server process. And subsequent request can be handled at the cost of a simple request. This can help improve the throughput of the server a lot. Even new functions can be added at run time, without stopping the server which is amazing!  
JNI also uses dynamic linking to call C functions in Java code which is worth to mention.
```c
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

  handle = dlopen(filename, RTLD_LAZY); // loads and links shared library "filename"
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
    exit(1);
  }

  add = dlsym(handle, "add"); // get the address of the symbol "add", which is a function
  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", error);
    exit(1);
  }
  
  /*
   * Now we can use add function as a normal function call!
   */
   
  if (dlclose(handle) < 0) { // unloads the shared library
    fprintf(stderr, "%s\n", dlerror());
    exit(1);
  }
}
```
#####How to compile? Easy
```script
## First compile shared library
gcc -shared -fPIC -o adder.so adder.c

## Compile server program
gcc -rdynamic -O2 -o baseline_server baseline_server.c -ldl
```