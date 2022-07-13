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
#include <fcntl.h>

#define FAIL -1
#define RECONNECT 0
#define SUCCESS 1
#define CLIENT_SOCKET_BACKLOG 100
#define BACKEND_BUFFER 65536

//backend host
struct hostent *host;

//EPOLL
#define MAX_EVENTS 10
struct epoll_event ev, events[MAX_EVENTS];
int epollfd = epoll_create(1);

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

struct CLIENT_SOCKET listen_on_socket(int server_port)
{
    int server_fd;
    int opt = 1;
    struct sockaddr_in address;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(server_port);

    // Forcefully attaching socket to the port 9999
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, CLIENT_SOCKET_BACKLOG) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    struct CLIENT_SOCKET server_socket = {server_fd, address};
    return server_socket;
}

struct BACKEND_SOCKET
{
    int sd;
    struct sockaddr_in address;
};

void set_host(const char *hostname){
    if ((host = gethostbyname(hostname)) == NULL)
    {
        perror(hostname);
        abort();
    }
}

int Create_Backend_Connection(int port, struct sockaddr_in addr, char* hostname)
{
    int sd;
    int retry_count = 5;

    for (int i=0; i<retry_count; i++) {
        sd = socket(PF_INET, SOCK_STREAM, 0);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = *(long *)(host->h_addr);
        if (sd == -1)
        {
            continue;
        }

        if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) != -1)
        {
            break;     
        }             /* Success */

        close(sd);
    }
    return sd;
}

int OpenConnection(int sd, struct sockaddr_in addr, char *hostname)
{
    // printf("trying backend connection\n");
    if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        printf("closing backend connection due to error");
        close(sd);
        perror(hostname);
        abort();
    }
    return sd;
}
// Load crypto
SSL_CTX *InitCTX(void)
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    OpenSSL_add_all_algorithms();     /* Load cryptos, et.al. */
    SSL_load_error_strings();         /* Bring in and register error messages */
    method = TLSv1_2_client_method(); /* Create new client-method instance */
    ctx = SSL_CTX_new(method);        /* Create new context */
    if (ctx == NULL)
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

void ShowCerts(SSL *ssl)
{
    X509 *cert;
    char *line;
    cert = SSL_get_peer_certificate(ssl); /* get the server's certificate */
    if (cert != NULL)
    {
        // printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        // printf("Subject: %s\n", line);
        free(line); /* free the malloc'ed string */
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        // printf("Issuer: %s\n", line);
        free(line);      /* free the malloc'ed string */
        X509_free(cert); /* free the malloc'ed certificate copy */
    }
    else
        printf("Info: No client certificates configured.\n");
}

char *read_from_client(int socket_front)
{
    char *client_buffer = (char *)calloc(1024, sizeof(char));
    char *null_pointer = NULL;
    int read_from_client;

    read_from_client = read(socket_front, client_buffer, 1024); /* get reply & decrypt */
    printf("%d fd - read_from_client: %d\n", socket_front, read_from_client);
    if(read_from_client == FAIL)
    {
       printf("%d fd - failed reading from client\n", socket_front);
       return null_pointer;
    }
    else if(read_from_client == 0){
        printf("%d fd - empty request\n", socket_front);
        return null_pointer;
    }
    return client_buffer;
}

int backend_write(SSL *ssl, char *client_message)
{
    int write_ret;
    int ret;
    write_ret = SSL_write(ssl, client_message, strlen(client_message));
    if (write_ret <= 0)
    {
        ret = SSL_get_error(ssl, write_ret);
        if (ret == SSL_ERROR_ZERO_RETURN)
        {
            printf("SSL_ERROR_ZERO_RETURN\n");
            return RECONNECT;
        }
        else if (ret == SSL_ERROR_WANT_WRITE)
        {
            printf("SSL_ERROR_WANT_WRITE\n");
            return RECONNECT;
        }
        else if (ret == SSL_ERROR_WANT_CONNECT)
        {
            printf("SSL_ERROR_WANT_CONNECT\n");
            return RECONNECT;
        }
        else if (ret == SSL_ERROR_WANT_X509_LOOKUP)
        {
            printf("SSL_ERROR_WANT_X509_LOOKUP\n");
            return RECONNECT;
        }
        else
        {
            // read_backend_write_client(ssl, client_socket);
            printf("Error reason: %d\n", ret);
            return FAIL;
        }
    }
    return SUCCESS;
}

int read_backend_write_client(SSL *ssl, int client_socket)
{
    int ret;
    int bytes;
    char buf[BACKEND_BUFFER] = {0};
    do
    {
        // printf("backend-read iterating... \n");
        bytes = SSL_read(ssl, buf, sizeof(buf)); /* get reply & decrypt */
        printf("%d fd - backend read bytes: %d\n", client_socket, bytes);
        if (bytes <= 0)
        {
            ret = SSL_get_error(ssl, bytes);
            if (ret == SSL_ERROR_ZERO_RETURN)
            {
                printf("SSL_ERROR_ZERO_RETURN: \n");
                return 0;
            }
            else if (ret == SSL_ERROR_WANT_WRITE)
            {
                printf("SSL_ERROR_WANT_WRITE\n");
                return 0;
            }
            else if (ret == SSL_ERROR_WANT_CONNECT)
            {
                printf("SSL_ERROR_WANT_CONNECT\n");
                return 0;
            }
            else if (ret == SSL_ERROR_WANT_X509_LOOKUP)
            {
                printf("SSL_ERROR_WANT_X509_LOOKUP\n");
                return 0;
            }
            else if (ret == SSL_ERROR_WANT_X509_LOOKUP)
            {

            }
            else
            {
                printf("Error reason: %d\n", ret);
                return FAIL;
            }
        }
        buf[bytes] = 0;
        send(client_socket, buf, bytes, 0);
    } while (SSL_pending(ssl) > 0);
    return 1;
}
