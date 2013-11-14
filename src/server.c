#include <syslog.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "processpool.h"
#define PROCESS_COUNT 10
#define REQUEST_MAX_SIZE 256
#define MAXEVENTS 64
#define FILENAME_LENGTH 256
#define SERVER_STRING "Server: myserver\r\n"

void
daemonize(const char *cmd)
{
	int					i, fd0, fd1, fd2;
	pid_t				pid;
	struct rlimit		rl;
	struct sigaction	sa;

	/*
	 * Clear file creation mask.
	 */
	umask(0);

	/*
	 * Get maximum number of file descriptors.
	 */
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
		printf("can't get file limit");

	/*
	 * Become a session leader to lose controlling TTY.
	 */
	if ((pid = fork()) < 0)
		printf("can't fork");
	else if (pid != 0) /* parent */
		exit(0);
	setsid();

	/*
	 * Ensure future opens won't allocate controlling TTYs.
	 */
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		printf("can't ignore SIGHUP");
	if ((pid = fork()) < 0)
		printf("can't fork");
	else if (pid != 0) /* parent */
		exit(0);

	/*
	 * Change the current working directory to the root so
	 * we won't prevent file systems from being unmounted.
	 */
	if (chdir("/") < 0)
		printf("can't change directory to /");

	/*
	 * Close all open file descriptors.
	 */
	if (rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;
	for (i = 0; i < rl.rlim_max; i++)
		close(i);

	/*
	 * Attach file descriptors 0, 1, and 2 to /dev/null.
	 */
	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);

	/*
	 * Initialize the log file.
	 */
	openlog(cmd, LOG_CONS, LOG_DAEMON);
	if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
		syslog(LOG_ERR, "unexpected file descriptors %d %d %d",
		  fd0, fd1, fd2);
		exit(1);
	}
}

void headers(int client, int size) {
	char buf[1024];
	char strsize[20];
	sprintf(strsize, "%d", size);
	strcpy(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "Connection: keep-alive\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "Content-length: ");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, strsize);
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
}

char *get_requested_resource_filename(char *buf, int length) {
	char method[255], url[255];
	int i = 0, j = 0;
	while (buf[j] != ' ' && (i < sizeof(method) - 1)) {
  		method[i] = buf[j];
  		i++; j++;
 	}
 	method[i] = '\0';
	
	i = 0;

	while (buf[j] == ' ' || buf[j] == '/' && (j < length))
		j++;
	while (buf[j] != ' ' && (i < sizeof(url) - 1) && (j < length)) {
		url[i] = buf[j];
		i++; j++;
	}
	url[i] = '\0';
	return strdup(url);
}

int main() {
	//daemonize("server");
	struct epoll_event event;
	struct epoll_event *events;
	int efd;

	processpool_t *pool;
	childprocess_t *child;
	int sock, listener, bytes_to_send = 0;
	struct sockaddr_in addr;
	char buf[256], fname[FILENAME_LENGTH];
	int bytes_read;
	char request[REQUEST_MAX_SIZE], *fn;
	int request_size, i, n;
	int queries[2], responses[2];
	request_sock_t request_info;
	response_sock_t response_info;
	response_info.filesize = 0;
	FILE *file;
	listener = socket(AF_INET, SOCK_STREAM, 0);
	if(listener < 0) {
		perror("socket");
		exit(1);
	}
	memset(fname, 0, FILENAME_LENGTH);
	fcntl(listener, F_SETFL, O_NONBLOCK);
    
	addr.sin_family = AF_INET;
	addr.sin_port = htons(3425);
	addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(2);
	}
	listen(listener, 1);
	//creating processpool
	pool = processpool_create(PROCESS_COUNT);
	//creating epoll
	efd = epoll_create1(0);
	//adding listening socket to epoll for observing requests
	event.data.fd = listener;
	event.events = EPOLLIN | EPOLLET;
	epoll_ctl(efd, EPOLL_CTL_ADD, listener, &event);
	//adding response queues to epool for observing data from child processes
	for (i = 0; i < PROCESS_COUNT; i++) {
		event.data.fd = pool->processes[i].responses[0];
		event.events = EPOLLIN | EPOLLET;
		epoll_ctl(efd, EPOLL_CTL_ADD, pool->processes[i].responses[0], &event);
    	}
	events = calloc(MAXEVENTS, sizeof(struct epoll_event));
	if (getpid() != pool->parentpid) {
		response_info.size = 0;
		child = get_its_info(pool);
		queries[0] = child->queries[0];
		queries[1] = child->queries[1];
		responses[0] = child->responses[0];
		responses[1] = child->responses[1];
		while (1) {
			if (read(queries[0], &request_info, sizeof(request_sock_t)) == -1) {
				printf("ERROR %d\n", errno);
			}
			if (read(queries[0], fname, request_info.size) == -1) {
				printf("ERROR %d\n", errno);
			}
			file = fopen(fname,"r");
			memset(fname, 0, FILENAME_LENGTH);
			response_info.sock = request_info.sock;
			response_info.done = 0;
			fseek(file, 0L, SEEK_END);
			response_info.filesize = ftell(file);
			fseek(file, 0L, SEEK_SET);
			while (!feof(file)) {
				response_info.size = fread(response_info.response, 1, MAX_RESPONSE_SIZE, file);
				if (feof(file))
					response_info.done = 1;
				write(responses[1], &response_info, sizeof(response_sock_t));
				response_info.filesize = 0;
				memset(response_info.response, 0, REQUEST_MAX_SIZE);
			}
			fclose(file);
			getinline(pool, child);
		}
	}
	else {
		while (1) {
			n = epoll_wait(efd, events, MAXEVENTS, -1);
			for (i = 0; i < n; i++) {
				if (listener == events[i].data.fd) {
					while (1) {
						sock = accept(listener, NULL, NULL);
						if (sock == -1) {
							if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
								break;
							}
							else {
								perror("accept");
								break;
							}
						}
						request_size = recv(sock, request, REQUEST_MAX_SIZE, 0);
						send_request_to_processpool(pool, sock, get_requested_resource_filename(request, request_size), request_size);
						memset(request, 0, REQUEST_MAX_SIZE);
					}
				}
				else {
					while (1) {
						if (read(events[i].data.fd, &response_info, sizeof(response_sock_t)) == -1)
							break;
						if (response_info.filesize != 0)
							headers(response_info.sock, response_info.filesize);
						send(response_info.sock, response_info.response, response_info.size, 0);
						if (response_info.done == 1) {
							close(response_info.sock);
							break;
						}
					}
				}
			}
		}
	}
	processpool_wait(pool); 
	close(listener);
	return 0;
}
