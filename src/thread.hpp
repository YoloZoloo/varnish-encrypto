#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>

#define FAIL -1
#define RECONNECT 0
#define SUCCESS 1
#define THREAD_NUMBER 100

int thread_id;
pthread_mutex_t queue_lock;
char *backend_hostname;
int backend_port;
int client_sd;
int backend_sd;
struct sockaddr_in backend_address;
class my_thread_pool;

struct my_thread
{
    pthread_mutex_t condition_mutex;
    pthread_cond_t condition_cond;
    my_thread *next_thread;
    pthread_t thread;
    int socket_fd;
};
void queue_myself(struct my_thread *node);
void *handle_backend(void *Node);

class my_thread_pool
{
    /**
    new thread will be inserted to the end of the linked list
    and the thread for the task will be taken out from the front of the list
    */
public:
    struct my_thread *head = NULL;
    struct my_thread *tail = NULL;
    void spawn_thread();
    void push(struct my_thread **head, struct my_thread **tail);
    int dequeue_worker(int sockfd);
    void delete_thread();
    // void queue_myself(struct my_thread *node);
};

void my_thread_pool::spawn_thread()
{
    for (int i = 0; i < THREAD_NUMBER; i++)
    {
        my_thread_pool::push(&head, &tail);
    }
}
// insert a new node in front of the list
void my_thread_pool::push(struct my_thread **head, struct my_thread **tail)
{
    struct my_thread *newNode = (struct my_thread *)malloc(sizeof(struct my_thread));

    /* Put in the data  */
    pthread_mutex_init(&(newNode->condition_mutex), NULL);
    pthread_cond_init(&(newNode->condition_cond), NULL);
    pthread_create(&(newNode->thread), NULL, handle_backend, (void *)(newNode));

    /* Link the last node to the new node */
    (*tail)->next_thread = newNode;
    /* Make the new node as the last node */
    *tail = newNode;
    newNode->next_thread = NULL;
    if (head == NULL)
    {
        /* Move the head to point to the new node */
        (*head) = newNode;
    }
}
/** take thread from the front of the list
and assign task to it.
*/
int my_thread_pool::dequeue_worker(int sockfd)
{
    printf("here");
    if (head == NULL)
    {
        return 0;
    }
    head->socket_fd = sockfd;
    pthread_cond_signal(&head->condition_cond);
    head = head->next_thread;
    return 1;
}
void my_thread_pool::delete_thread()
{
    /**
     currently there is no logic here
     */
}
my_thread_pool *thread_pool = new my_thread_pool;
void queue_myself(struct my_thread *node)
{
    node->next_thread = thread_pool->head->next_thread;
    thread_pool->head = node;
}

void *handle_backend(void *Node)
{
    struct my_thread *node = (struct my_thread *)Node;
    int backend;
    int front_sd;

    printf("---------------starting thread-------------------\n");
    SSL_CTX *ctx = InitCTX();

    int ssl_connect_status;

    for (;;)
    {
        printf("here %d\n", pthread_self());
        pthread_mutex_lock(&node->condition_mutex);
        while (thread_id == 0)
        {
            printf("trying to lock mutex\n");
            while (pthread_mutex_trylock(&queue_lock) != 0)
            {
                // sleep for 300ms
                sleep(3);
            }
            queue_myself(node);
            pthread_mutex_unlock(&queue_lock);
            printf("thread went into sleep");
            pthread_cond_wait(&node->condition_cond, &node->condition_mutex);
            front_sd = node->socket_fd;
        }
        printf("front_sd: %d\n", front_sd);
        pthread_mutex_unlock(&node->condition_mutex);
        // front_sd = node->socket_fd;

        printf("accepted connection to create a read-ready file descriptor\n");
        SSL *ssl = SSL_new(ctx);
        backend_sd = create_be_socket(backend_hostname, backend_port, backend_address);
        backend = OpenConnection(backend_sd, backend_address, backend_hostname);
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
        front_sd = 0;
        node->socket_fd = 0;
    }
}
