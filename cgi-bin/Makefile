CC = gcc
CFLAGS = -shared -fPIC -I ..

all: adder

adder: adder.c
	$(CC) $(CFLAGS) -o adder.so adder.c

clean:
	rm -f adder *~
