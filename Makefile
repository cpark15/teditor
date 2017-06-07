CFLAGS = -Wall -Wextra -pedantic -std=c99

all: teditor

teditor: teditor.c
	$(CC) $(CFLAGS) -o teditor teditor.c

clean:
	rm -f teditor
