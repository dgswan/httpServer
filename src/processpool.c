#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include "processpool.h"

/*typedef struct childprocess {
	int queries[2];
	int responses[2];
	pid_t process;
} childprocess_t;

typedef struct processpool {
	childprocess_t *processes;
	pid_t parentpid;
	int pcount;
	int processqueue[2];
} processpool_t;*/

childprocess_t *childprocess_create() {
	childprocess_t *cp = (childprocess_t*)malloc(sizeof(childprocess_t));
	pipe(cp->queries);
	pipe(cp->responses);
	cp->process = fork();
	switch (cp->process) {
		case -1 :
			printf("fork");
			return 0;
		case 0 :
			//close queries pipe for child for reading
			close(cp->queries[1]);
			//close responses pipe for child for writing
			close(cp->responses[0]);
			break;
		case 1 :
			//close queries pipe for parent for writing
			close(cp->queries[0]);
			//close responses pipe for parent for reading
			close(cp->responses[1]);
	}
	return cp;
}

int childprocess_init(childprocess_t *cp) {
	pipe(cp->queries);
	pipe2(cp->responses, O_NONBLOCK);
	cp->process = fork();
	switch (cp->process) {
		case -1 :
			printf("fork");
			return -1;
		case 0 :
			//close queries pipe for child for reading
			close(cp->queries[1]);
			//close responses pipe for child for writing
			close(cp->responses[0]);
			return 0;
		case 1 :
			//close queries pipe for parent for writing
			close(cp->queries[0]);
			//close responses pipe for parent for reading
			close(cp->responses[1]);
			//fcntl(cp->responses[0], F_SETFL, O_NONBLOCK);
			return 0;
	}
	return 0;
}

void childprocess_delete(childprocess_t *cp) {
	free(cp);
}

void getinline(processpool_t *pool, childprocess_t *cp) {
	write(pool->processqueue[1], cp, sizeof(childprocess_t));
	printf("GOT IN LINE\n");
}

processpool_t *processpool_create(int pcount) {
	int i;
	processpool_t *pool = (processpool_t*)malloc(sizeof(processpool_t));
	pool->processes = (childprocess_t*)malloc(sizeof(childprocess_t)*pcount);
	pool->pcount = pcount;
	pool->parentpid = getpid();
	pipe(pool->processqueue);
	for (i = 0; i < pcount; i++) {
		childprocess_init(&(pool->processes[i]));
		//if child is executing then close processqueue for writing and get in line
		if (pool->processes[i].process == 0) {
			//close processqueue for child for writing to make children be able to get in line
			close(pool->processqueue[0]);
			getinline(pool, &(pool->processes[i]));
			break;
		}
		else {
			//close processqueue for parent for reading to make it be able to get waiting child from line
			close(pool->processqueue[1]);
		}
	}
	return pool;
}

void processpool_wait(processpool_t *pool) {
	int i;
	for (i = 0; i < pool->pcount; i++) {
		waitpid(pool->processes[i].process, 0, 0);
	}
}

void processpool_delete(processpool_t *pool) {
	int i;
	for (i = 0; i < pool->pcount; i++) {
		childprocess_delete(&(pool->processes[i]));
	}
	free(pool);
}

void send_request_to_processpool(processpool_t *pool, int sock, char *request, int size) {	
	childprocess_t cp;// = (childprocess_t*)malloc(sizeof(childprocess_t));
	printf("I'm trying to take spare process\n");
	read(pool->processqueue[0], &cp, sizeof(childprocess_t));
	printf("I got spare process\n");
	request_sock_t rs;
	rs.sock = sock;
	rs.size = size;
	write(cp.queries[1], &rs, sizeof(request_sock_t));
	printf("sending request to childprocess %s\n", request);
	write(cp.queries[1], request, size);
}

childprocess_t *get_its_info(processpool_t *pool) {
	int i;
	for (i = 0; i < pool->pcount; i++) {
		if (pool->processes[i].process == 0)
			return &(pool->processes[i]);
	}
	return NULL;
}
