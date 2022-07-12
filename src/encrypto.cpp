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
#include "functions.hpp"
#include "thread.hpp"
#include <pthread.h>
#include <fcntl.h>

#define MAX_FD 1024
#define FAIL -1
#define RECONNECT 0
#define SUCCESS 1
#define MAX_EVENTS 100
#define THREAD_SLEEP 21

pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_cond = PTHREAD_COND_INITIALIZER;
int count = 0;

struct CLIENT_SOCKET client_socket;
struct sockaddr_in address;
socklen_t addrlen;

void set_nonblocking(int fd)
{
    int status = fcntl(fd, F_SETFL, O_NONBLOCK);
    printf("setting socket to nonblocking\n");
    if (status == -1)
    {
        perror("calling fcntl");
    }
}

void *accept_connection(void *)
{
    int n;
    int new_socket_fd;
    while (1)
    {
        new_socket_fd = accept(client_sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        printf("new_socket_fd: %d\n", new_socket_fd);
        n = thread_pool->dequeue_worker(new_socket_fd);
        if (n == 0)
        {
            close(new_socket_fd);
            printf("closed the client socket due to empty thread pool\n");
        }
    }
}

int main(int argc, char *argv[])
{
    int client_port;
    SSL_library_init();
    if (argc < 4)
    {
        printf("correct usage is \"binary file\", listening port, backend server name, backend server port \n");
        return 0;
    }
    client_port = atoi(argv[1]);
    backend_hostname = argv[2];
    backend_port = atoi(argv[3]);
    printf("port: %d, hostname: %s, por num: %d \n", client_port, backend_hostname, backend_port);

    struct hostent *host;
    if ((host = gethostbyname(backend_hostname)) == NULL)
    {
        perror(backend_hostname);
        abort();
    }
    backend_sd = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&backend_address, sizeof(backend_address));
    backend_address.sin_family = AF_INET;
    backend_address.sin_port = htons(backend_port);
    backend_address.sin_addr.s_addr = *(long *)(host->h_addr);

    struct CLIENT_SOCKET client_socket = (struct CLIENT_SOCKET)listen_on_socket(client_port);
    client_sd = client_socket.server_fd;
    printf("client sd: %d\n", client_sd);
    address = client_socket.address;
    addrlen = sizeof(address);

    thread_pool->populate_thread_pool();

    pthread_t acceptor_thread = (pthread_t)THREAD_NUMBER + 1;
    pthread_create(&acceptor_thread, NULL, accept_connection, NULL);
    pthread_join(acceptor_thread, NULL);
    return 0;
}
