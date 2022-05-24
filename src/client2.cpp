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
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include "load_root_certificate.hpp"

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
namespace ssl = net::ssl;       // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#define FAIL -1

struct SERVER_SOCKET {
     int server_fd;
     struct sockaddr_in address;
};

struct SERVER_SOCKET listen_on_socket (int server_port) {
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
char* read_from_client(int socket_front)
{
    printf("reading from the client side\n");
    char *client_buffer = (char*)calloc(1024, sizeof(char));
    int valread;

    valread = read( socket_front , client_buffer, 1024); /* get reply & decrypt */
    // std::string str;
    // str = client_buffer;
    return client_buffer;
}

int read_backend_write_client(SSL *ssl, int client_socket){
    int bytes;
    char buf[1024] = {0};
    do{
        printf("iterating... \n");
        bytes = SSL_read(ssl, buf, sizeof(buf)); /* get reply & decrypt */
        buf[bytes] = 0;
//        printf("Received: \"%s\"\n", buf);
        send(client_socket , buf , strlen(buf) , 0 );
        printf("Bytes: \"%d\"\n", bytes);
    }while(SSL_pending(ssl) > 0);
    return 1;
} 

int main(int argc, char *argv[])
{

    char *hostname;
    char *portnum;
    int client_socket;
    int client_port;
    int server_fd;

    if(argc < 4){
      printf("correct usage is \"binary file\", listening port, backend servername, backend server port \n");
      return 0;
    }
    try
    {
        client_port = atoi(argv[1]);
        hostname = argv[2];
        portnum = argv[3];

        printf("port: %d, hostname: %s, portnum: %s \n", client_port, hostname, portnum);

        
        // The io_context is required for all I/O
        net::io_context ioc;

        // The SSL context is required, and holds certificates
        ssl::context ctx(ssl::context::tlsv12_client);

        // This holds the root certificate used for verification
        load_root_certificates(ctx);
        
        struct SERVER_SOCKET server_socket = listen_on_socket(client_port);
        server_fd = server_socket.server_fd;
        struct sockaddr_in address = server_socket.address;
        int addrlen = sizeof(address);

        // Verify the remote server's certificate
        ctx.set_verify_mode(ssl::verify_peer);

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
        while(1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
            printf("accepted connection\n");
            char* client_message = read_from_client(client_socket);
            printf("%s\n",client_message );
            free(client_message);

            // Set SNI Hostname (many hosts need this to handshake successfully)
            if(! SSL_set_tlsext_host_name(stream.native_handle(), hostname))
            {
                beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                throw beast::system_error{ec};
            }

            // Look up the domain name
            auto const results = resolver.resolve(hostname, portnum);

            // Make the connection on the IP address we get from a lookup
            beast::get_lowest_layer(stream).connect(results);

            // Perform the SSL handshake
            stream.handshake(ssl::stream_base::client);

            // Set up an HTTP GET request message
            http::request<http::string_body> req{http::verb::get, "/", 11};
            req.set(http::field::host, hostname);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

            // Send the HTTP request to the remote host
            http::write(stream, req);

            // This buffer is used for reading and must be persisted
            beast::flat_buffer buffer;

            // Declare a container to hold the response
            http::response<http::dynamic_body> res;

            // Receive the HTTP response
            http::read(stream, buffer, res);

            //write to client
            char *client_response = malloc(std::strlen(res) * sizeof(char));
            send(server_fd, res, strlen(res), 0); 

            // Write the message to standard out
            std::cout << res << std::endl;
        }
       // Gracefully close the stream
        beast::error_code ec;
        stream.shutdown(ec);
        if(ec == net::error::eof)
        {
            // Rationale:
            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
            ec = {};
        }
        if(ec)
            throw beast::system_error{ec};

        // If we get here then the connection is closed gracefully
        
        return 0;
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
