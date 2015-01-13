Yijie's learning MEMO for 18-845 Internet Services  
<br>
About CGI(common gateway interface)  
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
About dealing with Broken pipe  
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