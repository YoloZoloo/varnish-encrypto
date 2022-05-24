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

#define FAIL -1
#define RECONNECT 0
#define SUCCESS 1
#define MAX_EVENTS 100

int main(int argc, char *argv[])
{
    char *backend_hostname;
    int backend_port;
    int client_sd;
    int client_fd;
    int client_port;
    int back_sd_arr[20];

    if(argc < 4){
      printf("correct usage is \"binary file\", listening port, backend servername, backend server port \n");
      return 0;
    }
    client_port = atoi(argv[1]);
    backend_hostname = argv[2];
    backend_port = atoi(argv[3]);

    printf("port: %d, hostname: %s, portnum: %d \n", client_port, backend_hostname, backend_port);

    SSL_CTX *ctx;
    SSL *ssl;
    int backend;
    SSL_library_init();
    ctx = InitCTX();
    ssl = SSL_new(ctx);      /* create new SSL connection state */


    struct CLIENT_SOCKET client_socket = listen_on_socket(client_port);
    client_sd = client_socket.server_fd;
    struct sockaddr_in address = client_socket.address;
    int addrlen = sizeof(address);

    struct BACKEND_SOCKET *backend_socket;
    int backend_ret;
    backend_socket = create_be_socket(backend_hostname, backend_port);
    backend = OpenConnection(backend_socket->sd, backend_socket->address, backend_hostname);
    SSL_set_fd(ssl, backend);

    struct epoll_event ev, events[MAX_EVENTS];
    int epoll_fd = epoll_create(1);
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, backend, &ev);


    while(1) {
	if ((client_fd = accept(client_sd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
	{
	    perror("accept");
	    exit(EXIT_FAILURE);
	}
        printf("accepted connection\n");
        char* client_message = read_from_client(client_fd);
	    printf("%s\n",client_message);


        if ( SSL_connect(ssl) == FAIL )  /* perform the connection */
        { 
            printf("ERROR WHEN ESTABLISHING BACKEND CONNECTION \n\n");
            ERR_print_errors_fp(stderr);
            close(backend);
            free(backend_socket);
            ctx = InitCTX();
            ssl = SSL_new(ctx);
            struct BACKEND_SOCKET *backend_socket = create_be_socket(backend_hostname, backend_port);
            backend = OpenConnection(backend_socket->sd, backend_socket->address, backend_hostname);
            SSL_set_fd(ssl, backend);
            if(SSL_connect(ssl) < 0)
            {
                printf("SSL_connect failed more than twice in a row\n");
                return 0;
            }
        }
        else
        {
            printf("\n\nConnected with %s encryption\n", SSL_get_cipher(ssl));
            ShowCerts(ssl);        /* get any certs */
            backend_ret = backend_write(ssl, client_message);
            if (backend_ret <= 0)
            {
                printf("ERROR DURING BACKEND WRITE\n");
                printf("initiating backend connection\n");
                ctx = InitCTX();
                ssl = SSL_new(ctx);
                close(backend);
                free(backend_socket);
                struct BACKEND_SOCKET *backend_socket = create_be_socket(backend_hostname, backend_port);
                backend = OpenConnection(backend_socket->sd, backend_socket->address, backend_hostname);
                SSL_set_fd(ssl, backend);
                if(SSL_connect(ssl) > 0)
                {
                    SSL_write(ssl, client_message, strlen(client_message));
                }
            }
            //read from backend and relay that data to the client
            backend_ret = read_backend_write_client(ssl, client_fd);
            if(backend_ret <= 0){
                printf("ERROR DURING BACKEND READ\n");
                close(backend);
                free(backend_socket);
                ctx = InitCTX();
                ssl = SSL_new(ctx);
                struct BACKEND_SOCKET *backend_socket = create_be_socket(backend_hostname, backend_port);
                backend = OpenConnection(backend_socket->sd, backend_socket->address, backend_hostname);
                SSL_set_fd(ssl, backend);
                if(SSL_connect(ssl) > 0)
                {
                    SSL_write(ssl, client_message, strlen(client_message));
                    read_backend_write_client(ssl, client_fd);
                }
            }
            if (client_message != NULL)
            {
                free(client_message);
            }
            printf("done reading from backend\n\n");
        }
    }
    close(backend);
    return 0;
}
