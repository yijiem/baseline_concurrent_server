#
# Makefile for baseline server
#
# You may modify is file any way you like (except for the handin
# rule). Autolab will execute the command "make" on your specific 
# Makefile to build your baseline server from sources.
#
CC = gcc
CFLAGS = -g -w -std=gnu99
LDFLAGS = -lpthread -rdynamic -ldl

all: cgi baseline_server

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

baseline_server.o: baseline_server.c csapp.h # cache.h
	$(CC) $(CFLAGS) -c baseline_server.c

cgi:
	(cd cgi-bin; make)

# cache.o: cache.c cache.h
# 	$(CC) $(CFLAGS) -c cache.c

baseline_server: baseline_server.o csapp.o # cache.o

# Creates a tarball in ../proxylab-handin.tar that you should then
# hand in to Autolab. DO NOT MODIFY THIS!
# handin:
# 	(make clean; cd ..; tar cvf proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o baseline_server core *.tar *.zip *.gzip *.bzip *.gz
	(cd cgi-bin; make clean)

