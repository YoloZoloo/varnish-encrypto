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
#include <sys/epoll.h>
#include "functions.hpp"
#include <pthread.h>
#include <fcntl.h>

#define MAXFDS 1024
#define FAIL -1
#define RECONNECT 0
#define SUCCESS 1
#define MAX_EVENTS 100
#define THREAD_NUMBER 1
#define THREAD_SLEEP 21

pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_cond = PTHREAD_COND_INITIALIZER;
pthread_t thread_pool;
int thread_id;
int count = 0;

struct CLIENT_SOCKET client_socket;
struct sockaddr_in address;
struct sockaddr_in backend_address;
socklen_t addrlen;

int client_port;
char *backend_hostname;
int backend_port;
int client_sd;
int backend_sd;

struct epoll_event ev, events[MAX_EVENTS];
int epfd = epoll_create(1);

int newsockfd;

void setnonblocking(int fd)
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
    while (1)
    {
        n = epoll_wait(epfd, events, MAX_EVENTS, 100);
        for (int i = 0; i < n; i++)
        {
            printf("i: %d\n", i);
            if (events[i].events & EPOLLERR)
            {
                perror("epoll_wait returned EPOLLERR");
            }

            if (events[i].data.fd == client_sd)
            {
                printf("count: %d\n", count);
                thread_id = 0;
                // pthread_cond_signal(&condition_cond);
                // The listening socket is ready; this means a new peer is connecting.
                newsockfd = accept(client_sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                printf("newsocket_fd: %d\n", newsockfd);
                // epoll_ctl(epfd, EPOLLONESHOT, client_sd, &ev);
                if (newsockfd < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        // This can happen due to the nonblocking socket mode; in this
                        // case don't do anything, but print a notice (since these events
                        // are extremely rare and interesting to observe...)
                        printf("accept returned EAGAIN or EWOULDBLOCK\n");
                    }
                    else if (errno == EPOLLRDHUP)
                    {
                        // close(backend_sd);
                    }
                    else
                    {
                        perror("accept");
                    }
                }
                else
                {
                    pthread_cond_signal(&condition_cond);
                }
                count++;
                printf("count2: %d\n", count);
            }
        }
    }
}

void *handle_backend(void *)
{
    int backend;
    int front_sd;

    printf("---------------starting thread-------------------\n");
    SSL_CTX *ctx = InitCTX();

    int ssl_connect_status;

    for (;;)
    {
        printf("here %d\n", pthread_self());
        pthread_mutex_lock(&condition_mutex);
        while (thread_id != 0)
        {
            printf("went into sleep\n");
            pthread_cond_wait(&condition_cond, &condition_mutex);
            front_sd = newsockfd;
        }
        pthread_mutex_unlock(&condition_mutex);

        printf("accepted connection to create a read-ready file descriptor\n");
        SSL *ssl = SSL_new(ctx);
        backend = Create_Backend_Connection(backend_port, backend_address, backend_hostname);
        // backend = OpenConnection(backend_sd, backend_address, backend_hostname);
        SSL_set_fd(ssl, backend);
        // accepted socket is ready for reading
        char *client_message = read_from_client(front_sd);
        printf("%s\n", client_message);
        ssl_connect_status = SSL_connect(ssl);
        if (ssl_connect_status == FAIL) /* perform the connection */
        {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(backend);
            printf("ERROR WHEN ESTABLISHING BACKEND CONNECTION \n\n");
            ERR_print_errors_fp(stderr);
            exit(0);
        }
        ShowCerts(ssl);
        printf("backend write: %d\n", SSL_write(ssl, client_message, strlen(client_message)));
        if (read_backend_write_client(ssl, front_sd) == FAIL)
        {
            if (read_backend_write_client(ssl, front_sd) == FAIL)
            {
                close(backend);
                SSL_shutdown(ssl);
                SSL_free(ssl);
                free(client_message);
                close(front_sd);
                printf("Error during reading from the back end\n");
                exit(0);
            }
        }
        SSL_shutdown(ssl);
        SSL_free(ssl);
        free(client_message);
        close(front_sd);

        thread_id = 1;
    }
}

int main(int argc, char *argv[])
{
    SSL_library_init();
    if (argc < 4)
    {
        printf("correct usage is \"binary file\", listening port, backend servername, backend server port \n");
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
    setnonblocking(client_sd);
    address = client_socket.address;
    addrlen = sizeof(address);

    // initialize threadpool
    //  int thread_id;
    //  thread_id
    for (pthread_t thread = 0; thread < THREAD_NUMBER; thread++)
    {
        thread_id = 1;
        pthread_create(&thread, NULL, handle_backend, NULL);
        printf("thread_id: %d\n", thread_id);
        thread_pool = thread_id;
        // thread_pool[thread] = thread_id;
        // pthread_join(thread, NULL);
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client_sd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_sd, &ev) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    // int n;
    pthread_t acceptor_thread = 101;
    pthread_create(&acceptor_thread, NULL, accept_connection, NULL);
    pthread_join(acceptor_thread, NULL);
    return 0;
}
