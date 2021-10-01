all: webserver

webserver: webserver.c
	gcc webserver.c -W -Wall -lpthread -std=c99 -o webserver

clean:
	rm webserver