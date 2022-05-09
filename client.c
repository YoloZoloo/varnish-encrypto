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

struct SERVER_SOCKET {
     int server_fd;
     struct sockaddr_in address;
};

struct SERVER_SOCKET listen_on_socket (int server_port) {
    //hard coded listening port
//    int server_port = 9999;
    // Creating socket file descriptor
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
        struct SERVER_SOCKET server_socket = {server_fd, address};
        return server_socket;
}

struct BACKEND_SOCKET {
    int sd;
    struct sockaddr_in address;
};

struct BACKEND_SOCKET create_be_socket (const char *hostname, int port) {
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
    struct BACKEND_SOCKET backend_socket = {sd, addr};
    return backend_socket;
}

int OpenConnection(int sd, struct sockaddr_in addr, char* hostname)
{
    if ( connect(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
    {
        close(sd);
        perror(hostname);
        abort();
    }
    return sd;
}
//Load crypto
SSL_CTX* InitCTX(void)
{
    SSL_METHOD *method;
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

int main(int argc, char const *argv[])
{
    SSL_CTX *ctx;
    int backend;
    SSL *ssl;

    char buf[1024] = {0};
    char buffer[1024] = {0};
    int bytes;
    char *hostname;
    int portnum;
    int server_fd;
    int new_socket;
    int client_port;

    if(argc < 4){
      printf("correct usage is \"binary file\", listening port, backend servername, backend server port ");
      return 0;
    }
    client_port = atoi(argv[1]);
    hostname = argv[2];
    portnum = atoi(argv[3]);

    printf("port: %d, hostname: %s, portnum: %d \n", client_port, hostname, portnum);

    SSL_library_init();
    ctx = InitCTX();
    ssl = SSL_new(ctx);      /* create new SSL connection state */
    SSL *ssl_front = SSL_new(ctx);

    struct SERVER_SOCKET server_socket = listen_on_socket(client_port);
    server_fd = server_socket.server_fd;
    struct sockaddr_in address = server_socket.address;
    int addrlen = sizeof(address);

    int valread;
    char client_buffer[1024] = {0};

    struct BACKEND_SOCKET backend_socket;

//    hostname="node3.codeatyolo.link";
//    portnum=443;
    backend_socket = create_be_socket(hostname, portnum);
    backend = OpenConnection(backend_socket.sd, backend_socket.address, hostname);

    while(1) {
	if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
	{
	    perror("accept");
	    exit(EXIT_FAILURE);
	}
        valread = read( new_socket , client_buffer, 1024);

	printf("%s\n",client_buffer );

        SSL_set_fd(ssl, backend);

        if ( SSL_connect(ssl) == FAIL )   /* perform the connection */
             ERR_print_errors_fp(stderr);
        else
        {
            printf("\n\nConnected with %s encryption\n", SSL_get_cipher(ssl));
            ShowCerts(ssl);        /* get any certs */
            if(SSL_write(ssl, client_buffer, strlen(client_buffer)) <= 0)
            {
               ERR_print_errors_fp(stderr);
               return 0;
            }
            memset(client_buffer, 0, 1024);
            printf("iterating... \n");
            do{
                bytes = SSL_read(ssl, buf, sizeof(buf)); /* get reply & decrypt */
                buf[bytes] = 0;
                printf("Received: \"%s\"\n", buf);
                send(new_socket , buf , strlen(buf) , 0 );
                printf("Bytes: \"%d\"\n", bytes);
             }while(SSL_pending(ssl) > 0);

            printf("done reading from backend\n");
            memset(buf, 0, 1024);
        }
        printf("Backend closed\n\n\n\n");
    }
    close(backend);
    return 0;
}
