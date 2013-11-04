all : server client

server : server.c
	gcc -o ../bin/server server.c processpool.c
client : client.c
	gcc -o ../bin/client client.c
clean :
	rm ../bin/server ../bin/client
