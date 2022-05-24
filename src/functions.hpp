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

#define FAIL -1
#define RECONNECT 0
#define SUCCESS 1
#define MAX_EVENTS 100

struct CLIENT_SOCKET {
     int server_fd;
     struct sockaddr_in address;
};

struct CLIENT_SOCKET listen_on_socket (int server_port) {
    int server_fd;
    int opt = 1;
    struct sockaddr_in address;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
     {
     perror("socket failed");
          exit(EXIT_FAILURE);
     }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR , &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( server_port );

    // Forcefully attaching socket to the port 9999
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
	if (listen(server_fd, 3) < 0)
	{
            perror("listen");
            exit(EXIT_FAILURE);
	}
        struct CLIENT_SOCKET server_socket = {server_fd, address};
        return server_socket;
}

struct BACKEND_SOCKET {
    int sd;
    struct sockaddr_in address;
};

struct BACKEND_SOCKET *create_be_socket (const char *hostname, int port) {
    int sd;
    struct hostent *host;
    struct sockaddr_in addr;
    if ( (host = gethostbyname(hostname)) == NULL )
    {
        perror(hostname);
        abort();
    }
    sd = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = *(long*)(host->h_addr);
    free(host);
    struct BACKEND_SOCKET *backend_socket = (struct BACKEND_SOCKET *) malloc(sizeof(addr) + sizeof(int)+sizeof(sd));
    backend_socket->address = addr;
    backend_socket->sd = sd;
    return backend_socket;
}

int OpenConnection(int sd, struct sockaddr_in addr, char* hostname)
{
    printf("trying backend connection");
    if ( connect(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
    {
        printf("closing backend connection due to error");
        close(sd);
        perror(hostname);
        abort();
    }
    return sd;
}
//Load crypto
SSL_CTX* InitCTX(void)
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    OpenSSL_add_all_algorithms();  /* Load cryptos, et.al. */
    SSL_load_error_strings();   /* Bring in and register error messages */
    method = TLSv1_2_client_method();  /* Create new client-method instance */
    ctx = SSL_CTX_new(method);   /* Create new context */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

void ShowCerts(SSL* ssl)
{
    X509 *cert;
    char *line;
    cert = SSL_get_peer_certificate(ssl); /* get the server's certificate */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);       /* free the malloc'ed string */
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);       /* free the malloc'ed string */
        X509_free(cert);     /* free the malloc'ed certificate copy */
    }
    else
        printf("Info: No client certificates configured.\n");
}

char* read_from_client(int socket_front)
{
    printf("reading from the client side\n");
    char *client_buffer = (char*)calloc(1024, sizeof(char));
    int valread;

    valread = read( socket_front , client_buffer, 1024); /* get reply & decrypt */
    return client_buffer;
}

int backend_write(SSL* ssl, char *client_message){
    int write_ret;
    int ret;
    write_ret = SSL_write(ssl, client_message, strlen(client_message));
    if(write_ret <= 0)
    {   
        ret = SSL_get_error(ssl ,write_ret);
        if (ret == SSL_ERROR_ZERO_RETURN)
        {
            printf("SSL_ERROR_ZERO_RETURN\n");
            return RECONNECT;
        }
        else if(ret == SSL_ERROR_WANT_WRITE)
        {
            printf("SSL_ERROR_WANT_WRITE\n");
            return RECONNECT;
        }
        else if(ret == SSL_ERROR_WANT_CONNECT){
            printf("SSL_ERROR_WANT_CONNECT\n");
            return RECONNECT;
        }
        else if (ret == SSL_ERROR_WANT_X509_LOOKUP){
            printf("SSL_ERROR_WANT_X509_LOOKUP\n");
            return RECONNECT;
        }
        else{
            // read_backend_write_client(ssl, client_socket);
            printf("Error reason: %d\n", ret);
            return FAIL;
        }
    }
    return SUCCESS;
}

int read_backend_write_client(SSL *ssl, int client_socket){
    int ret;
    int bytes;
    char buf[1024] = {0};
    do{
        printf("iterating... \n");
        bytes = SSL_read(ssl, buf, sizeof(buf)); /* get reply & decrypt */
        if(bytes <= 0)
        {   
            ret = SSL_get_error(ssl ,bytes);
            if (ret == SSL_ERROR_ZERO_RETURN)
            {
                printf("SSL_ERROR_ZERO_RETURN: \n");
                return 0;
            }
            else if(ret == SSL_ERROR_WANT_WRITE)
            {
                printf("SSL_ERROR_WANT_WRITE\n");
                return 0;
            }
            else if(ret == SSL_ERROR_WANT_CONNECT){
                printf("SSL_ERROR_WANT_CONNECT\n");
                return 0;
            }
            else if (ret == SSL_ERROR_WANT_X509_LOOKUP){
                printf("SSL_ERROR_WANT_X509_LOOKUP\n");
                return 0;
            }
            else{
                // read_backend_write_client(ssl, client_socket);
                printf("Error reason: %d\n", ret);
                return FAIL;
            }
        }
        buf[bytes] = 0;
        send(client_socket , buf , strlen(buf) , 0 );
        printf("Bytes: \"%d\"\n", bytes);
    }while(SSL_pending(ssl) > 0);
    return 1;
} 
