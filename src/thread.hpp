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
#define THREAD_NUMBER 300

#define STATUS_RETURNING -99
#define STATUS_READY -100
#define STATUS_SLEEPING -101
#define STATUS_INITIAL -102

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
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
    int status;
    int node_no;
};
void queue_myself(struct my_thread *node);
void *handle_backend(void *Node);
void *dummy_function(void *Node);

class my_thread_pool
{
    /**
    new thread will be inserted to the end of the linked list
    and the thread for the task will be taken out from the front of the list
    */
public:
    struct my_thread *head = NULL;
    struct my_thread *temp = NULL;
    struct my_thread *tail = NULL;
    void populate_thread_pool();
    void push(struct my_thread **head, struct my_thread **tail, int node_number);
    int dequeue_worker(int sockfd);
    void delete_thread();
    void check_integrity();
};

void my_thread_pool::populate_thread_pool()
{
    for (int i = 0; i < THREAD_NUMBER; i++)
    {
        my_thread_pool::push(&head, &tail, i);
    }
    if (tail->next_thread == NULL)
    {
#ifdef DEBUG
        printf("next thread of tail is NULL\n");
#endif
    }
}
// insert a new node in front of the list
void my_thread_pool::push(struct my_thread **head, struct my_thread **tail, int node_number)
{
    struct my_thread *newNode = (struct my_thread *)malloc(
        sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) +
        sizeof(struct my_thread) + sizeof(pthread_t) + 3 * sizeof(int));

    /* Put in the data  */
    pthread_mutex_init(&(newNode->condition_mutex), NULL);
    pthread_cond_init(&(newNode->condition_cond), NULL);
    newNode->socket_fd = 0;
    newNode->status = STATUS_INITIAL;
    newNode->node_no = node_number;
    pthread_create(&(newNode->thread), NULL, handle_backend, (void *)(newNode));
    // // /* Link the last node to the new node */
    if (*tail != NULL)
    {
        (*tail)->next_thread = newNode;
    }
    // /* Make the new node as the last node */
    *tail = newNode;
    newNode->next_thread = NULL;
    if (*head == NULL)
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
    if (head == NULL || head->next_thread == NULL)
    {
        return 0;
    }
    // my_thread_pool::check_integrity();
    temp = head;
    temp->socket_fd = sockfd;
    temp->status = STATUS_READY;
    pthread_cond_signal(&temp->condition_cond);
#ifdef DEBUG
    printf("%d - dequeueing \n", temp->node_no);
#endif
    head = head->next_thread;
#ifdef DEBUG
    printf("%d - thread pool head shifted to this\n", head->node_no);
#endif
    return 1;
}
void my_thread_pool::delete_thread()
{
    /**
     currently there is no logic here
     */
}
void my_thread_pool::check_integrity()
{
    temp = head;
#ifdef DEBUG
    printf("checking integrity");
#endif
    while (temp != tail)
    {
#ifdef DEBUG
        printf("%d\n", temp->node_no);
#endif
        temp = temp->next_thread;
    }
}
my_thread_pool *thread_pool = new my_thread_pool;
void queue_myself(struct my_thread *node)
{
    //**queue the thread to the end of the list*/
    if (thread_pool->tail != NULL)
    {
        thread_pool->tail->next_thread = node;
    }
    // as a last node, node's next thread should be null
    node->next_thread = NULL;
    //** Make the new node as the last node */
    thread_pool->tail = node;
    // if thread comes back to thread pool of NULL, it should set head
    if (thread_pool->head == NULL)
    {
        thread_pool->head = node;
    }
}

void *handle_backend(void *Node)
{
    int detach_status;
    detach_status = pthread_detach(pthread_self());
    if (detach_status != 0)
    {
#ifdef DEBUG
        printf("thread is not detached");
#endif
    }
    struct my_thread *node = (struct my_thread *)Node;
    int backend;
    int front_sd;
    int mutex_lock;
    int close_client_fd;
#ifdef DEBUG
    printf("---------------starting thread------------------ %d\n", node->node_no);
#endif
    SSL_CTX *ctx = InitCTX();
    SSL *ssl = SSL_new(ctx);
    int ssl_connect_status;
    while (node->status == STATUS_INITIAL)
    {
        pthread_mutex_lock(&node->condition_mutex);
#ifdef DEBUG
        printf("%d - first hibernation\n", node->node_no);
#endif
        pthread_cond_wait(&node->condition_cond, &node->condition_mutex);
        pthread_mutex_unlock(&node->condition_mutex);
    }
#ifdef DEBUG
    printf("%d - came out of hibernation\n", node->node_no);
#endif
    for (;;)
    {
        pthread_mutex_lock(&node->condition_mutex);
        while (node->status == STATUS_SLEEPING || node->status == STATUS_RETURNING)
        {
            while (node->status == STATUS_RETURNING)
            {
                if (pthread_mutex_trylock(&queue_lock) != 0)
                {
                    // sleep for 300ms
                    sleep(1);
                }
                else
                {
                    node->status = STATUS_SLEEPING;
                }
            }
            mutex_lock = pthread_mutex_unlock(&queue_lock);
            if (mutex_lock != 0)
            {
#ifdef DEBUG
                printf("%d - queue mutex unlock failed: %d\n", node->node_no, mutex_lock);
#endif
            }
#ifdef DEBUG
            printf("%d thread - about to queue this thread back to thread pool\n", node->node_no);
#endif
            queue_myself(node);
#ifdef DEBUG
            printf("%d - thread going into sleep\n", node->node_no);
#endif
            pthread_cond_wait(&node->condition_cond, &node->condition_mutex);
#ifdef DEBUG
            printf("%d - thread woke up to task\n", node->node_no);
#endif
        }
        pthread_mutex_unlock(&node->condition_mutex);
        front_sd = node->socket_fd;
#ifdef DEBUG
        printf("%d thread is handling connection descriptor %d\n", node->node_no, front_sd);
#endif
        // accepted socket is ready for reading
        char *client_message = read_from_client(front_sd);
        if (client_message == NULL)
        {
#ifdef DEBUG
            printf("%d - Error on the client side\n", node->node_no);
#endif
        }
        else
        {
            SSL_clear(ssl);
            backend = Create_Backend_Connection(backend_port, backend_address, backend_hostname);
            SSL_set_fd(ssl, backend);
            ssl_connect_status = SSL_connect(ssl);
            if (ssl_connect_status == FAIL) /* perform the connection */
            {
#ifdef DEBUG
                printf("%d - ERROR WHEN ESTABLISHING BACKEND CONNECTION \n\n", node->node_no);
#endif
            }
            SSL_write(ssl, client_message, strlen(client_message));
            if (read_backend_write_client(ssl, front_sd) == FAIL)
            {
                if (read_backend_write_client(ssl, front_sd) == FAIL)
                {
#ifdef DEBUG
                    printf("%d - Error during reading from the back end\n", node->node_no);
#endif
                }
            }
            shutdown(backend, 2);
            close(backend);
            SSL_shutdown(ssl);
        }
        free(client_message);
        shutdown(front_sd, 2);
        close_client_fd = close(front_sd);
        if (close_client_fd < 0)
        {
#ifdef DEBUG
            printf("%d - FILE DESCRIPTOR not closed\n", node->node_no);
#endif
            if (errno == EBADF)
            {
                printf("invalid file descriptor\n");
            }
            else if (errno == EINTR)
            {
                printf("The close() call was interrupted by a signal\n");
            }
            else if (errno == EIO)
            {
                printf("IO error \n");
            }
        }
        node->socket_fd = 0;
        node->status = STATUS_RETURNING;
    }
}
