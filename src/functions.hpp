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

int create_source_endpoint(int port, struct sockaddr_in addr, char *hostname)
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
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        free(line); /* free the malloc'ed string */
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
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
    int ret;
    int err_ssl;
    for (int i=0; i<2; i++) {
        ret = SSL_connect(ssl);
        if (ret > 0) {
            return SUCCESS;
        }
        else if (ret <= 0) /* perform the connection */
        {
            err_ssl = SSL_get_error(ssl, ret);
            printf("error with SSL connect: be_fd = %d ,", SSL_get_fd(ssl));
            switch (err_ssl)
            {
            case SSL_ERROR_ZERO_RETURN:
                printf("SSL_ERROR_ZERO_RETURN %s\n", strerror(errno));
            case SSL_ERROR_WANT_CONNECT:
                printf("SSL_ERROR_WANT_CONNECT %s\n", strerror(errno));
            case SSL_ERROR_SSL:
                printf("SSL_ERROR_SSL, %s\n", strerror(errno));
                return FAIL;
            case SSL_ERROR_SYSCALL:
                printf("SSL_ERROR_SYSCALL %s\n", strerror(errno));
            case SSL_ERROR_NONE:
                printf("SSL_ERROR_NONE %s\n", strerror(errno));
                return SUCCESS;
            default:
                printf("Error reason: %d\n", ret);
                return FAIL;
            }
        }
    }
    return FAIL;
}

int backend_write(SSL *ssl, char *client_message)
{
    int ret;
    int err_ssl;
    for (int i=0; i<2; i++) {
        if (!fd_is_valid (SSL_get_fd(ssl)) ) {
            printf("fd for SSL_write is invalid fd\n");
            return RECONNECT;
        }
        ret = SSL_write(ssl, client_message, strlen(client_message));
        if (ret > 0) {
            return SUCCESS;
        }
        else if (ret == 0) {
            return FAIL;
        }
        else {
            err_ssl = SSL_get_error(ssl, ret);
            printf("Error during backend write, ");
            switch (err_ssl)
            {
            case SSL_ERROR_ZERO_RETURN:
                printf("SSL_ERROR_ZERO_RETURN %s\n", strerror(errno));
            case SSL_ERROR_WANT_WRITE:
                printf("SSL_ERROR_WANT_WRITE %s\n", strerror(errno));
            case SSL_ERROR_SSL:
                printf("SSL_ERROR_SSL %s\n", strerror(errno));
                return FAIL;
            case SSL_ERROR_SYSCALL:
                // retry on SSL_ERROR_SYSCALL
                printf("SSL_ERROR_SYSCALL %s\n", strerror(errno));
            case SSL_ERROR_NONE:
                printf("SSL_ERROR_NONE%s\n", strerror(errno));
                return SUCCESS;
            default:
                printf("Error reason: %d\n", ret);
                return FAIL;
            }
        }
    }
    return FAIL;
}

int read_backend_write_client(SSL *ssl, int client_socket)
{
    int ret;
    int bytes;
    char buf[BACKEND_BUFFER] = {0};
    do
    {
        if (! fd_is_valid (SSL_get_fd(ssl)) ) {
            return FAIL;
        }
        bytes = SSL_read(ssl, buf, BACKEND_BUFFER + 1); /* get reply & decrypt */
        if (bytes == 0) {
            return RECONNECT; //it can be clean shutdown on the peer end
        }
        else if (bytes < 0)
        {
            printf("Error during backend read, ");
            ret = SSL_get_error(ssl, bytes);
            switch (ret)
            {
            case SSL_ERROR_ZERO_RETURN:
                printf("SSL_ERROR_ZERO_RETURN\n");
                return FAIL;
            case SSL_ERROR_WANT_READ:
                printf("SSL_ERROR_WANT_READ\n");
            case SSL_ERROR_SSL:
                printf("SSL_ERROR_SSL\n");
                return FAIL;
            case SSL_ERROR_SYSCALL:
                // retry on SSL_ERROR_SYSCALL
                printf("SSL_ERROR_SYSCALL\n");
            case SSL_ERROR_NONE:
                printf("SSL_ERROR_SSL\n");
            default:
                printf("Error reason: %s\n", strerror(errno));
            }
            return FAIL;
        }
        buf[bytes] = 0;
        send(client_socket, buf, bytes, 0);
    } while (SSL_pending(ssl) > 0 || bytes == BACKEND_BUFFER);
    return SUCCESS;
}
