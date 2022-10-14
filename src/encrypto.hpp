#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif
#ifdef __unix__
#include <sys/epoll.h>
#endif
#include <map>

#define FAIL -1
#define RECONNECT 0
#define SUCCESS 1
#define THREAD_NUMBER 350
#define MAX_EVENTS 3
#define EPOLL_TIMEOUT -1

#define CLIENT_SOCKET_BACKLOG 100
#define BACKEND_BUFFER 17000

#define IDLE 1
#define INACTIVE 0

#define STATUS_RETURNING -99
#define STATUS_READY -100
#define STATUS_SLEEPING -101
#define STATUS_INITIAL -102
#define STATUS_CLOSING_CONNECTION -103
#define DEQUEUE_RETRY_N 5

struct timespec req, rem = {0, 100000};

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
char *backend_hostname;
int backend_port;
int client_sd;
int BACKEND_SD;
struct sockaddr_in backend_address;

epoll_event events[MAX_EVENTS], ev;
int epollfd;

typedef struct wt
{
    pthread_mutex_t condition_mutex;
    pthread_cond_t condition_cond;
    struct wt *next_thread;
    pthread_t thread;
    SSL *ssl;
    int client_sd;
    int backend_sd;
    int status;
    int node_no;
    int connection;
} worker_thread;

void handle_request(worker_thread *node);
void *handle_thread_task(void *Node);
void handle_backend(worker_thread *node, char* client_message, bool reconnecting);
void* manage_idle_connections(void *Node);

// backend host
struct hostent *host;

struct SOCKET_INFO
{
    int backend_port;
    char *backend_hostname;
    int server_fd;
    struct sockaddr_in address;
};

struct CLIENT_SOCKET
{
    int server_fd;
    struct sockaddr_in address;
};

struct BACKEND_SOCKET
{
    int sd;
    struct sockaddr_in address;
};

struct CLIENT_SOCKET client_socket;
struct sockaddr_in address;
socklen_t addrlen;

class WORKER_THREAD
{
    /**
    new thread will be inserted to the end of the linked list
    and the thread for the task will be taken out from the front of the list
    */
public:
    worker_thread *head = (worker_thread *) malloc(
        2 * sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) +
        2 * sizeof(worker_thread) + sizeof(pthread_t) +
        5 * sizeof(int) + sizeof(SSL*)
    );
    worker_thread *tail = NULL;
    int  check_if_null(worker_thread *node);
    void populate_thread_pool();
    void push(worker_thread **head_inactive, worker_thread **tail_inactive, int node_number);
    int  dequeue_worker(int sockfd);
    void queue_to_pool(worker_thread *node);
    void close_idle_connections(worker_thread *node);
    void dequeue_from_pool(worker_thread *node, int connfd);
};

class WORKER_THREAD;
