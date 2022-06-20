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

void setnonblocking(int fd) {
    // where socketfd is the socket you want  to make non-blocking
    int status = fcntl(fd, F_SETFL, O_NONBLOCK);
    printf("setting socket to nonblcoking\n");
    if (status == -1){
        perror("calling fcntl");
        // handle the error.  By the way, I've never seen fcntl fail in this way
    }
}

// void *handle_backend(void*){
//     printf("creating thread\n");
//     return NULL;
// }

int main(int argc, char *argv[])
{
    char *backend_hostname;
    int backend_port;
    int client_sd;
    int newsockfd;
    int client_port;
    int back_sd_arr[20];

    if(argc < 4){
      printf("correct usage is \"binary file\", listening port, backend servername, backend server port \n");
      return 0;
    }
    client_port = atoi(argv[1]);
    backend_hostname = argv[2];
    backend_port = atoi(argv[3]);
    printf("port: %d, hostname: %s, por num: %d \n", client_port, backend_hostname, backend_port);

    SSL_library_init();
    int backend;
    SSL_CTX *ctx = InitCTX();
    SSL* ssl = SSL_new(ctx);

    struct CLIENT_SOCKET client_socket = (struct CLIENT_SOCKET) listen_on_socket(client_port);
    client_sd = client_socket.server_fd;
    printf("client sd: %d\n", client_sd);
    setnonblocking(client_sd);
    struct sockaddr_in address = client_socket.address;
    int addrlen = sizeof(address);

    struct BACKEND_SOCKET *backend_socket;
    int backend_ret;
    backend_socket = create_be_socket(backend_hostname, backend_port);
    backend = OpenConnection(backend_socket->sd, backend_socket->address, backend_hostname);
    SSL_set_fd(ssl, backend);


    //create threadpool
    // for (pthread_t thread = 0; thread < 20; thread++)
    // {
    //     pthread_create(&thread, NULL, handle_backend, NULL);
    //     pthread_join(thread, NULL);
    // }


    struct epoll_event ev, events[MAX_EVENTS];
    int epfd = epoll_create(1);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client_sd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_sd, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }
    int ssl_connect_status;
    int n;
    int bytes;
    while(1) {
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
                // The listening socket is ready; this means a new peer is connecting.
                newsockfd = accept(client_sd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
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
                    else
                    {
                        perror ("accept");
                    }
                }
                else 
                {
                    printf("accepted connection to create a read-ready file descriptor\n");
                    // accepted socket is ready for reading
                    char* client_message = read_from_client(newsockfd);
                    printf("%s\n",client_message );
                    ssl_connect_status = SSL_connect(ssl);
                    if ( ssl_connect_status == FAIL )  /* perform the connection */
                    { 
                        printf("ERROR WHEN ESTABLISHING BACKEND CONNECTION \n\n");
                        ERR_print_errors_fp(stderr);
                        close(backend);
                        free(backend_socket);
                        backend_socket = NULL;
                        SSL_free(ssl);
                        SSL_CTX_free(ctx);
                        SSL_CTX *ctx = InitCTX();
                        SSL* ssl = SSL_new(ctx);
                        struct BACKEND_SOCKET *backend_socket = create_be_socket(backend_hostname, backend_port);
                        backend = OpenConnection(backend_socket->sd, backend_socket->address, backend_hostname);
                        SSL_set_fd(ssl, backend);
                        ssl_connect_status = SSL_connect(ssl);
                        printf("ssl_connect_status:%d\n", ssl_connect_status);
                        if(ssl_connect_status < 0)
                        {
                            free(ssl);
                            printf("SSL_connect failed more than twice in a row\n");
                            return 0;
                        }
                        else{
                            printf("ssl_connect_status: %d\n", ssl_connect_status);
                        }
                    }
                    ShowCerts(ssl);
                    printf("backend write: %d\n", SSL_write(ssl, client_message, strlen(client_message)));
                    if(read_backend_write_client(ssl, newsockfd) == FAIL)
                    {
                        close(backend);
                        SSL_free(ssl);
                        SSL_CTX_free(ctx);
                        free(backend_socket);
                        backend_socket = NULL;
                        SSL_CTX *ctx = InitCTX();
                        SSL* ssl = SSL_new(ctx);
                        
                        struct BACKEND_SOCKET *backend_socket = create_be_socket(backend_hostname, backend_port);
                        backend = OpenConnection(backend_socket->sd, backend_socket->address, backend_hostname);
                        SSL_set_fd(ssl, backend);
                        ssl_connect_status = SSL_do_handshake(ssl);
                        printf("Handshake: %d\n", ssl_connect_status );
                        ssl_connect_status = SSL_connect(ssl);
                        SSL_write(ssl, client_message, strlen(client_message));
                        if(read_backend_write_client(ssl, newsockfd) == FAIL)
                        {
                            printf("Error during reading from the back end\n");
                            return 0;
                        }
                    }
                    free(client_message);
                    close(newsockfd);
                }
            }
            else 
            {
                // A peer socket is ready.
                if (events[i].events & EPOLLIN)
                {
                    // Ready for reading.
                    int fd = events[i].data.fd;
                    char *client_buffer = (char*)calloc(1024, sizeof(char));
                    if ( (n = read(fd, client_buffer, 1024)) < 0)
                    {
                        if (errno == ECONNRESET)
                        {
                            close(fd);
                            events[i].data.fd = -1;
                        } 
                        else {
                            printf("readline error\n");
                        }
                    }
                    else if (n == 0)
                    {
                        printf("client socket closed\n");
                        close(fd);
                        events[i].data.fd = -1;
                    }
                    printf("received from client: \n %s", client_buffer);
                    ssl_connect_status = SSL_connect(ssl);
                    if ( ssl_connect_status == FAIL )  /* perform the connection */
                    { 
                        printf("ERROR WHEN ESTABLISHING BACKEND CONNECTION \n\n");
                        ERR_print_errors_fp(stderr);
                        close(backend);
                        free(backend_socket);
                        free(ssl);
                        free(ctx);
                        SSL_CTX *ctx = InitCTX();
                        SSL* ssl = SSL_new(ctx);
                        struct BACKEND_SOCKET *backend_socket = create_be_socket(backend_hostname, backend_port);
                        backend = OpenConnection(backend_socket->sd, backend_socket->address, backend_hostname);
                        SSL_set_fd(ssl, backend);
                        ssl_connect_status = SSL_connect(ssl);
                        if(ssl_connect_status < 0)
                        {
                            free(ssl);
                            printf("SSL_connect failed more than twice in a row\n");
                            return 0;
                        }
                        else{
                            printf("ssl_connect_status: %d\n", ssl_connect_status);
                        }
                    }
                    SSL_write(ssl, client_buffer, strlen(client_buffer));
                    read_backend_write_client(ssl, newsockfd);
                    free(client_buffer);
                    close(fd);
                }
                else if (events[i].events & EPOLLOUT)
                {
                    
                }
            }
        } 
    }
    return 0;
}
