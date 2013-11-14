#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

char message[] = "input.txt";
char buf[256];

int main(int argc, char **argv)
{
    int sock;
    int response_size = 0, bytes_received = 0;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("socket");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(3425); // или любой другой порт...
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        exit(2);
    }

    
    send(sock, argv[1], strlen(argv[1]), 0);
    while(recv(sock, buf, 256, 0) != 0)
    	printf("%s\n", buf);
    //while (bytes_received != response_size) {
//	bytes_received += recv(sock, buf, 256, 0);	
  //  	printf("%s\n", buf);
//	printf("%d\n", bytes_received);
  //  }
    close(sock);

    return 0;
}
