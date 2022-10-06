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
#include "encrypto.hpp"
#include "functions.hpp"
#include "thread.cpp"
#include <pthread.h>
#include <fcntl.h>

void *accept_connection(void *)
{
    int n;
    int new_socket_fd;
    while (1)
    {
        new_socket_fd = accept(client_sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

        n = thread_pool->dequeue_worker(new_socket_fd);
        if (n == 0)
            close(new_socket_fd);
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

    set_host(backend_hostname);
    BACKEND_SD = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&backend_address, sizeof(backend_address));
//    set_nonblocking(BACKEND_SD);
    backend_address.sin_family = AF_INET;
    backend_address.sin_port = htons(backend_port);
    backend_address.sin_addr.s_addr = *(long *)(host->h_addr);

    epollfd = epoll_create(MAX_EVENTS);
    if(epollfd == -1)
    {
        printf("failed to create epoll file descriptor\n");
    }
//    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = BACKEND_SD;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, BACKEND_SD, &ev) == -1) {
        perror("epoll_ctl: add node backend socket descriptor");
    }

    struct CLIENT_SOCKET client_socket = (struct CLIENT_SOCKET)listen_on_socket(client_port);
    client_sd = client_socket.server_fd;
    address = client_socket.address;
    addrlen = sizeof(address);
    thread_pool->populate_thread_pool();

    pthread_t acceptor_thread = (pthread_t)THREAD_NUMBER + 1;
    pthread_create(&acceptor_thread, NULL, accept_connection, NULL);
    pthread_t idle_connection_keeper = (pthread_t)THREAD_NUMBER + 2;
    pthread_create(&idle_connection_keeper, NULL, monitor_idle_connections, NULL);
    if (pthread_detach(idle_connection_keeper) != 0)
    {
        printf("thread is not detached");
    }
    pthread_join(acceptor_thread, NULL);
    return 0;
}
