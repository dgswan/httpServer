#define MAX_RESPONSE_SIZE 256

typedef struct childprocess {
	int queries[2];
	int responses[2];
	pid_t process;
} childprocess_t;

typedef struct processpool {
	childprocess_t *processes;
	pid_t parentpid;
	int pcount;
	int processqueue[2];
} processpool_t;

typedef struct response_sock {
	int size;
	int sock;
	char response[MAX_RESPONSE_SIZE];
	int done;
	int filesize;
} response_sock_t;

typedef struct request_sock {
	int size;
	int sock;
} request_sock_t;

processpool_t *processpool_create(int pcount);

void processpool_wait(processpool_t *pool);

void send_request_to_processpool(processpool_t *pool, int sock, char *request, int size);

childprocess_t *get_its_info(processpool_t *pool);

void getinline(processpool_t *pool, childprocess_t *cp);
