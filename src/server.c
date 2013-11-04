#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include "processpool.h"
#define PROCESS_COUNT 10
#define REQUEST_MAX_SIZE 256

int main()
{
    //printf("BEGIN");
    processpool_t *pool;
    childprocess_t *child;
    int sock, listener, bytes_to_send = 0;
    struct sockaddr_in addr;
    char buf[256], fname[1024];
    int bytes_read;
    char request[REQUEST_MAX_SIZE];
    int request_size, i;
    int queries[2], responses[2];
    request_sock_t request_info;
    response_sock_t response_info;
    FILE *file;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if(listener < 0)
    {
        perror("socket");
        exit(1);
    }
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3425);
    addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(2);
    }

    listen(listener, 1);
    pool = processpool_create(PROCESS_COUNT);
    //printf("getpid() = %d, parentpid = %d\n", getpid(), pool->parentpid);
    //sock = accept(listener, NULL, NULL);
    //if (sock < 0) {
    // perror("accept");
    // exit(3);
    //}
    //recv(sock, request, 256);
    //send_request_to_processpool(pool, sock, request);
    //for (i = 0; i < pool->pcount; i++) {
    //	res = read(pool->processes[i].responses[0], response_info, sizeof(response_sock));
    //  if (res != -1 ) {
    //		read(pool->processes[i].responses[0], buf, 256);
    //		send(pool->processes[i].sock, buf, 256);
    //	}
    //}
	//printf("%d\n", getpid()==pool->parentpid);
	if (getpid() != pool->parentpid) {
		//printf("CHILD\n");
		child = get_its_info(pool);
		queries[0] = child->queries[0];
		queries[1] = child->queries[1];
		responses[0] = child->responses[0];
		responses[1] = child->responses[1];
		//printf("GETPID() = %d QUERIES[0] = %d QUERIES[1] = %d\n", getpid(), child->queries[0], child->queries[1]);
		while (1) {
			if (read(queries[0], &request_info, sizeof(request_sock_t)) == -1) {
				printf("ERROR %d\n", errno);
			}
			if (read(queries[0], request, request_info.size) == -1) {
				printf("ERROR %d\n", errno);
			}
			printf("file to open : %s\n", request);
			file = fopen(request,"r");
			response_info.sock = request_info.sock;
			while (!feof(file)) {
				bytes_to_send = fread(request, 1, 256, file);
				response_info.size = bytes_to_send;
				write(responses[1], &response_info, sizeof(response_sock_t));
				write(responses[1], request, response_info.size);
				printf("%s has been written to response\n", request);
			}
			fclose(file);
			getinline(pool, child);
			printf("serving is done\n");
		}
	}
	else {
		//printf("PARENT\n");
		while (1) {
			printf("I'm listening\n");
			sock = accept(listener, NULL, NULL);
			if (sock < 0) {
				perror("accept");
				exit(3);
			}
			printf("preparing for sending\n");
			request_size = recv(sock, request, REQUEST_MAX_SIZE, 0);
			send_request_to_processpool(pool, sock, request, request_size);
			printf("request sended : open %s\n", request);
			for (i = 0; i < pool->pcount; i++) {
				if (read(pool->processes[i].responses[0], &response_info, sizeof(response_sock_t)) != -1) {;
					printf("response info has been read succesfully\n");
					read(pool->processes[i].responses[0], buf, response_info.size);
					printf("response data has been read: \n");
					printf("%s\n", buf);
					printf("Sending data");
					send(response_info.sock, buf, response_info.size, 0);
					printf("DONE\n");
				}
			}
			//send(sock, request, request_size, 0);
		}
	}
	//printf("IT'S OVER\n");
    /*while(1)
    {
        sock = accept(listener, NULL, NULL);
        if(sock < 0)
        {
            perror("accept");
            exit(3);
        }
        
        switch(fork())
        {
        case -1:
            perror("fork");
            break;
            
        case 0:
            close(listener);
            while(1) {
                bytes_read = recv(sock, fname, 1024, 0);
                //if (bytes_read <= 0) break;
		//FILE *input = fopen(fname,"r");
		//fseek(input, 0L, SEEK_END);
		//bytes_to_send = ftell(input);
		//fseek(input, 0L, SEEK_SET);
		//printf("%d\n", bytes_to_send);
		send(sock, fname, bytes_read, 0);
		//while (!feof(input)) {
			//bytes_to_send += fread( buf, 1, 256, input);
                //	send(sock, buf, sizeof(char) * fread(buf, sizeof(char), 256, input), 0);
		//}
//		send(sock, buf, bytes_to_send, 0); 
            }

            close(sock);
            return 0;
            
        default:
            close(sock);
        }
    }*/
    processpool_wait(pool); 
    //close(listener);

    return 0;
}
