#define CONNECTION_AVAILABLE 1
#define NO_CONNECTION_AVAILABLE 0


#define FAIL -1
#define RECONNECT 0
#define SUCCESS 1

#define CLIENT_SOCKET_BACKLOG 100
#define BACKEND_BUFFER 65536
#define CLIENT_BUFFER_SIZE 1024

#define THREAD_NUMBER 10
#define STATUS_RETURNING -99
// #define STATUS_READY_IDLE_CONNECTION -100
#define STATUS_READY -100
#define STATUS_READY_INACTIVE_CONNECTION -101
#define STATUS_SLEEPING -102
#define STATUS_INITIAL -103

#define MAX_EVENTS 10
struct epoll_event ev, events[MAX_EVENTS];
int epollfd;

char *backend_hostname;
int backend_port;
int client_sd;
int backend_sd;
struct sockaddr_in backend_address;