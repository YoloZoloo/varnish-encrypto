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
#include <map>

#define FAIL -1
#define RECONNECT 0
#define SUCCESS 1
#define THREAD_NUMBER 50
#define MAX_EVENTS 3
#define EPOLL_TIMEOUT 800

#define STATUS_RETURNING_TO_INACTIVE -97
#define STATUS_RETURNED_TO_IDLE -98
#define STATUS_RETURNING_TO_IDLE -99
#define STATUS_READY -100
#define STATUS_SLEEPING -101
#define STATUS_INITIAL -102

pthread_mutex_t idle_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
char *backend_hostname;
int backend_port;
int client_sd;
int BACKEND_SD;
struct sockaddr_in backend_address;

std::map<int, SSL*> hashmap;
class WORKER_THREAD;

typedef struct worker_thread
{
    pthread_mutex_t condition_mutex;
    pthread_cond_t condition_cond;
    worker_thread *next_thread;
    pthread_t thread;
    int client_sd;
    int backend_sd;
    int status;
    int node_no;
} worker_thread;
void queue_worker(worker_thread *node);
void *handle_backend(void *Node);
void *dummy_function(void *Node);

class WORKER_THREAD
{
    /**
    new thread will be inserted to the end of the linked list
    and the thread for the task will be taken out from the front of the list
    */
public:
    worker_thread *head = (worker_thread *) malloc(
        sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) +
        sizeof(struct worker_thread) + sizeof(pthread_t) + 3 * sizeof(int));
    worker_thread *temp = NULL;
    worker_thread *tail = NULL;
    worker_thread *head_idle = (worker_thread *) malloc(
        sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) +
        sizeof(struct worker_thread) + sizeof(pthread_t) + 3 * sizeof(int));
    struct worker_thread *tail_idle = NULL;
    void populate_thread_pool();
    void push(struct worker_thread **head, struct worker_thread **tail, int node_number);
    int dequeue_worker(int sockfd);
    void delete_thread();
    void check_integrity();
};

void WORKER_THREAD::populate_thread_pool()
{
    head->next_thread = NULL;
    head_idle->next_thread = NULL;
    for (int i = 0; i < THREAD_NUMBER; i++)
    {
        WORKER_THREAD::push(&head, &tail, i);
    }
    if (tail->next_thread == NULL)
    {
#ifdef DEBUG
        printf("next thread of tail is NULL\n");
#endif
    }
}
// insert a new node in front of the list
void WORKER_THREAD::push(struct worker_thread **head, struct worker_thread **tail, int node_number)
{
    struct worker_thread *newNode = (struct worker_thread *) malloc(
        sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) +
        sizeof(struct worker_thread) + sizeof(pthread_t) + 3 * sizeof(int));

    /* Put in the data  */
    pthread_mutex_init(&(newNode->condition_mutex), NULL);
    pthread_cond_init(&(newNode->condition_cond), NULL);
    newNode->client_sd = 0;
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
    if ((*head)->next_thread == NULL)
    {
        /* Move the head to point to the new node */
        (*head)->next_thread = newNode;
    }
}
/** take thread from the front of the list
and assign task to it.
*/
int WORKER_THREAD::dequeue_worker(int sockfd)
{
    if (head->next_thread == NULL && head_idle->next_thread == NULL)
    {
        return 0;
    }
    if(head_idle->next_thread != NULL)
    {
        #ifdef DEBUG
            printf("%d - dequeueing idle connection\n", temp->node_no);
        #endif
        temp = head_idle->next_thread;
        temp->client_sd = sockfd;
        temp->status = STATUS_READY;
        // pthread_cond_signal(&temp->condition_cond);
        #ifdef DEBUG
            printf("%d - dequeueing \n", temp->node_no);
        #endif
        head_idle->next_thread = temp->next_thread;
        #ifdef DEBUG
            if(head_idle->next_thread != NULL)
            {
                printf("%d - thread pool head shifted to this\n", head_idle->next_thread->node_no);
            }
            else
            {
                printf("%d - thread pool head shifted to this\n", NULL);
            }
        #endif
        return 1;
    }
    temp = head->next_thread;
    temp->client_sd = sockfd;
    temp->backend_sd = 0;
    temp->status = STATUS_READY;
    pthread_cond_signal(&temp->condition_cond);
    #ifdef DEBUG
        printf("%d - dequeueing \n", temp->node_no);
    #endif
    head->next_thread = temp->next_thread;
    #ifdef DEBUG
         if(head->next_thread != NULL)
        {
            printf("%d - thread pool head shifted to this\n", head->next_thread->node_no);
        }
        else
        {
            printf("%d - thread pool head shifted to this\n", NULL);
        }
    #endif
    return 1;
}
void WORKER_THREAD::delete_thread()
{
    /**
     currently there is no logic here
     */
}
void WORKER_THREAD::check_integrity()
{
    temp = head->next_thread;
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
WORKER_THREAD *thread_pool = new WORKER_THREAD;
void queue_worker(struct worker_thread *node)
{
    //**queue the thread to the end of the list*/
    if (thread_pool->tail_idle != NULL)
    {
        thread_pool->tail_idle->next_thread = node;
    }
    // as a last node, node's next thread should be null
    node->next_thread = NULL;
    //** Make the new node as the last node */
    thread_pool->tail = node;
    // if thread comes back to thread pool of NULL, it should set head
    if (thread_pool->head_idle->next_thread == NULL)
    {
        thread_pool->head_idle->next_thread = node;
    }
}
void queue_to_inactive_pool(worker_thread *node)
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
    if (thread_pool->head->next_thread == NULL)
    {
        thread_pool->head->next_thread = node;
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
    struct worker_thread *node = (struct worker_thread *)Node;
    //socket descriptors and its closed statuses
    int backend;
    int front_sd;
    int close_client_fd;

    //mutex lock/unlock return value
    int mutex_lock;

    //EPOLL
    epoll_event events[MAX_EVENTS], ev;
    int epollfd;
    int n;
    epollfd = epoll_create(MAX_EVENTS);
    if(epollfd == -1)
    {
        printf("failed to create epoll file descriptor\n");
    }
    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    ev.data.fd = BACKEND_SD;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, BACKEND_SD, &ev) == -1) {
        perror("epoll_ctl: add node backend socket descriptor");
        // exit(EXIT_FAILURE);
    }

    // SSL
    SSL_CTX *ctx = InitCTX();
    int ssl_connect_status;
    #ifdef DEBUG
        printf("---------------starting thread------------------ %d\n", node->node_no);
    #endif

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
        while (node->status == STATUS_RETURNING_TO_IDLE)
        {
            if (pthread_mutex_trylock(&idle_queue_mutex) != 0)
            {
                // sleep for 1000ms
                sleep(1);
            }
            else
            {
                node->status = STATUS_RETURNED_TO_IDLE;
                queue_worker(node);
                mutex_lock = pthread_mutex_unlock(&idle_queue_mutex);
                if (mutex_lock != 0)
                {
                    #ifdef DEBUG
                        printf("%d - queue mutex unlock failed: %d\n", node->node_no, mutex_lock);
                    #endif
                }
                // ev.data.fd = node->backend_sd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, node->backend_sd, &ev) == -1) {
                    perror("epoll_ctl: add node backend socket descriptor");
                    // exit(EXIT_FAILURE);
                }
            }

        }
        while(node->status == STATUS_RETURNED_TO_IDLE)
        {
            n = epoll_wait(epollfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
            for(int i = 0 ; i < n; i++)
            {
                printf("event detected %d\n", node->backend_sd);
                if(events[i].events & EPOLLRDHUP)
                {
                    printf("closing backend connection %d\n", node->backend_sd);
                    shutdown(node->backend_sd, 2);
                    if(close(node->backend_sd) == FAIL)
                    {  
                        perror("closing backend sd after remote peer has closed their end\n");
                    }
                    node->status == STATUS_RETURNING_TO_INACTIVE;
                }
            }
        }
        while (node->status == STATUS_SLEEPING || node->status == STATUS_RETURNING_TO_INACTIVE)
        {
            while (node->status == STATUS_RETURNING_TO_INACTIVE)
            {
                if (pthread_mutex_trylock(&queue_mutex) != 0)
                {
                    // sleep for 300ms
                    sleep(1);
                }
                else
                {
                    node->status = STATUS_SLEEPING;
                }
            }
            #ifdef DEBUG
                printf("%d thread - about to queue this thread back to thread pool\n", node->node_no);
            #endif
            queue_to_inactive_pool(node);
            mutex_lock = pthread_mutex_unlock(&queue_mutex);
            if (mutex_lock != 0)
            {
                #ifdef DEBUG
                    printf("%d - queue mutex unlock failed: %d\n", node->node_no, mutex_lock);
                #endif
            }
            #ifdef DEBUG
                printf("%d - thread going into sleep\n", node->node_no);
            #endif
            pthread_cond_wait(&node->condition_cond, &node->condition_mutex);
            #ifdef DEBUG
                printf("%d - thread woke up to task\n", node->node_no);
            #endif
        }
        pthread_mutex_unlock(&node->condition_mutex);
        front_sd = node->client_sd;
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
            printf("about to do backend write and read\n");
            if(node->backend_sd == 0){
                printf("creating new SSL\n");
                SSL *ssl = SSL_new(ctx);
                backend = Create_Backend_Connection(backend_port, backend_address, backend_hostname);
                hashmap[backend] = ssl;
                // set_nonblocking(backend);
                SSL_set_fd(ssl, backend);
            if( ssl != NULL){
                do
                {
                    ssl_connect_status = SSL_connect(ssl);
                    switch(ssl_connect_status) {
                        case 1:
                            //successful case
                            break;
                        case 0:
                            printf("%d - SSL connection was shutdown controller by SSL protocol \n\n", node->node_no);
                            break;
                        default:
                            switch(SSL_get_error(ssl, ssl_connect_status)) {
                                case SSL_ERROR_WANT_READ:
                                    //do nothing
                                case SSL_ERROR_WANT_WRITE:
                                    //do nothing
                                default:
                                    #ifdef DEBUG
                                        printf("%d - ERROR WHEN ESTABLISHING BACKEND CONNECTION \n\n", node->node_no);
                                    #endif
                                    break;
                            }
                    }
                } while (ssl_connect_status == FAIL);
           }
           }
            else{
                printf("setting backend from node itself\n");
                backend = node->backend_sd;
                printf("setting ssl from hashmap\n");
            }

            SSL_write(hashmap[backend], client_message, strlen(client_message));
            if (read_backend_write_client(hashmap[backend], front_sd) == FAIL)
            {
                if (read_backend_write_client(hashmap[backend], front_sd) == FAIL)
                {
                    #ifdef DEBUG
                        printf("%d - Error during reading from the back end\n", node->node_no);
                    #endif
                    shutdown(backend, 2);
                    close(backend);
//                    SSL_shutdown(hashmap[node->backend_sd]);
                    SSL_free(hashmap[node->backend_sd]);
                    node->status = STATUS_RETURNING_TO_INACTIVE;
                }
                else
                {
                    node->status = STATUS_RETURNING_TO_IDLE;
                    node->backend_sd = backend;
                }
            }
            else
            {
                node->status = STATUS_RETURNING_TO_IDLE;
                node->backend_sd = backend;
            }
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
        node->client_sd = 0;
    }
}
