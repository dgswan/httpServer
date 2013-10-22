#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

int main()
{
    int sock, listener, bytes_to_send = 0;
    struct sockaddr_in addr;
    char buf[256], fname[1024];
    int bytes_read;

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
    
    while(1)
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
            while(1)
            {
                bytes_read = recv(sock, fname, 1024, 0);
                if (bytes_read <= 0) break;
		FILE *input = fopen(fname,"r");
		fseek(input, 0L, SEEK_END);
		bytes_to_send = ftell(input);
		fseek(input, 0L, SEEK_SET);
		printf("%d\n", bytes_to_send);
		send(sock, &bytes_to_send, sizeof(int), 0);
		while (!feof(input)) {
			//bytes_to_send += fread( buf, 1, 256, input);
                	send(sock, buf, sizeof(char) * fread(buf, sizeof(char), 256, input), 0);
		}
//		send(sock, buf, bytes_to_send, 0); 
            }

            close(sock);
            exit(0);
            
        default:
            close(sock);
        }
    }
    
    close(listener);

    return 0;
}
