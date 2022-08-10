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

void set_host(const char *hostname)
{
    if ((host = gethostbyname(hostname)) == NULL)
    {
        perror(hostname);
        abort();
    }
}

int Create_Backend_Connection(int port, struct sockaddr_in addr, char *hostname)
{
    int sd;
    int retry_count = 5;

    for (int i = 0; i < retry_count; i++)
    {
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
        } /* Success */

        close(sd);
    }
    return sd;
}

void set_nonblocking(int fd)
{
    int status = fcntl(fd, F_SETFL, O_NONBLOCK);
    printf("setting socket to nonblocking\n");
    if (status == -1)
    {
        perror("calling fcntl");
    }
}

int fd_is_valid(int fd)
{
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
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
    if (read_from_client == FAIL)
    {
#ifdef DEBUG
        printf("%d fd - failed reading from client\n", socket_front);
#endif
        free(client_buffer);
        return null_pointer;
    }
    else if (read_from_client == 0)
    {
#ifdef DEBUG
        printf("%d fd - empty request\n", socket_front);
#endif
        free(client_buffer);
        return null_pointer;
    }
    free(null_pointer);
    return client_buffer;
}

int backend_connect(SSL *ssl)
{
    int ssl_connect_status;
    int ret;
    ssl_connect_status = SSL_connect(ssl);
    if (ssl_connect_status == FAIL) /* perform the connection */
    {
        ret = SSL_get_error(ssl, ssl_connect_status);
        switch (ret)
        {
        case SSL_ERROR_ZERO_RETURN:
            printf("SSL_ERROR_ZERO_RETURN: \n");
        case SSL_ERROR_WANT_CONNECT:
            printf("SSL_ERROR_WANT_CONNECT: \n");
        case SSL_ERROR_SSL:
            printf("A fatal non-recoverable error during SSL_connect: %d\n", ret);
            ERR_print_errors_fp(stderr);
            return FAIL;
        case SSL_ERROR_SYSCALL:
            // retry on SSL_ERROR_SYSCALL
            #ifdef DEBUG
                printf("A fatal non-recoverable error during SSL_ERROR_SYSCALL SSL_Connect: %d\n", ret);
                ERR_print_errors_fp(stderr);
                printf("SSL_connect errno: %d\n", errno);
            #endif
            return RECONNECT;
        case SSL_ERROR_NONE:
            printf("Successful SSL_read: %d\n", ret);
        default:
            printf("Error reason: %d\n", ret);
            return FAIL;
        }
    }
    return SUCCESS;
}

int backend_write(SSL *ssl, char *client_message)
{
    int write_ret;
    int ret;
    write_ret = SSL_write(ssl, client_message, strlen(client_message));
    if (write_ret <= 0)
    {
        ret = SSL_get_error(ssl, write_ret);
        switch (ret)
        {
        case SSL_ERROR_ZERO_RETURN:
            printf("SSL_ERROR_ZERO_RETURN: \n");
        case SSL_ERROR_WANT_WRITE:
            printf("SSL_ERROR_WANT_WRITE: \n");
        case SSL_ERROR_SSL:
            printf("A fatal non-recoverable error during SSL_write: %d\n", ret);
            ERR_print_errors_fp(stderr);
            return FAIL;
        case SSL_ERROR_SYSCALL:
            // retry on SSL_ERROR_SYSCALL
            #ifdef DEBUG
                printf("A fatal non-recoverable error during SSL_ERROR_SYSCALL SSL_Connect: %d\n", ret);
                ERR_print_errors_fp(stderr);
                printf("SSL_connect errno: %d\n", errno);
            #endif
            return RECONNECT;
        case SSL_ERROR_NONE:
            printf("Successful SSL_read: %d\n", ret);
        default:
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
        bytes = SSL_read(ssl, buf, sizeof(buf)); /* get reply & decrypt */
        #ifdef DEBUG
            printf("%d fd - backend read bytes: %d\n", client_socket, bytes);
        #endif
        if (bytes <= 0)
        {
            ret = SSL_get_error(ssl, bytes);
            switch (ret)
            {
            case SSL_ERROR_ZERO_RETURN:
                printf("SSL_ERROR_ZERO_RETURN: \n");
            case SSL_ERROR_WANT_READ:
                printf("SSL_ERROR_WANT_READ: \n");
            case SSL_ERROR_SSL:
                printf("A fatal non-recoverable error during SSL_ERROR_SSL SSL_read: %d\n", ret);
                ERR_error_string(ret, NULL );
                ERR_print_errors_fp(stderr);
                return FAIL;
            case SSL_ERROR_SYSCALL:
                // retry on SSL_ERROR_SYSCALL
                #ifdef DEBUG
                    printf("A fatal non-recoverable error during SSL_ERROR_SYSCALL SSL_Connect: %d\n", ret);
                    ERR_print_errors_fp(stderr);
                    printf("SSL_connect errno: %d\n", errno);
                #endif
                return RECONNECT;
            case SSL_ERROR_NONE:
                printf("Successful SSL_read: %d\n", ret);
            default:
                printf("Error reason: %d\n", ret);
                return FAIL;
            }
        }
        buf[bytes] = 0;
        send(client_socket, buf, bytes, 0);
    } while (SSL_pending(ssl) > 0);
    return SUCCESS;
}
