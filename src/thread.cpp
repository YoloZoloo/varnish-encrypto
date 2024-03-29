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
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <signal.h>
#endif
#ifdef __unix__
#endif
#include <unordered_set>

std::unordered_set<int> uset;
void signal_handler(int signum);
WORKER_THREAD *thread_pool = new WORKER_THREAD;

void WORKER_THREAD::populate_thread_pool()
{
    head->next_thread = NULL;
    for (int i = 0; i < THREAD_NUMBER; i++) {
        WORKER_THREAD::push(&head, &tail, i);
    }
}
// insert a new node in front of the list
void WORKER_THREAD::push(worker_thread **head, worker_thread **tail, int node_number)
{
    worker_thread *newNode = (worker_thread *) malloc(
        2 * sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) +
        2 * sizeof(worker_thread) + sizeof(pthread_t) +
        5 * sizeof(int) + sizeof(SSL*)
    );

    /* Put in the data  */
    pthread_mutex_init(&(newNode->condition_mutex), NULL);
    pthread_cond_init(&(newNode->condition_cond), NULL);
    newNode->client_sd = 0;
    newNode->status = STATUS_SLEEPING;
    newNode->node_no = node_number;
    newNode->connection = INACTIVE;
    pthread_create(&(newNode->thread), NULL, &handle_thread_task, (void *)(newNode));

    // // /* Link the last node to the new node */
    if (*tail != NULL) {
        (*tail)->next_thread = newNode;
    }
    // /* Make the new node as the last node */
    *tail = newNode;
    newNode->next_thread = NULL;

    /* Move the head to point to the new node */
    if ((*head)->next_thread == NULL) {
        (*head)->next_thread = newNode;
    }
}
/** take thread from the front of the list
and assign task to it.
*/

int WORKER_THREAD::dequeue_worker(int sockfd)
{
    if(head == NULL) {
        printf("head_idle is NULL\n");
    }
    if(head->next_thread != NULL) {
        head->next_thread->client_sd = sockfd;
        head->next_thread->status = STATUS_READY;
        pthread_cond_signal(&head->next_thread->condition_cond);
        head->next_thread = head->next_thread->next_thread;
        return 1;
    }
    else {
        printf("thread pool is empty\n");
        while(1) {
            if (pthread_mutex_trylock(&queue_mutex) != 0) {
                if (nanosleep(&req, &rem) != 0) {
                     printf("nano sleep execution failed\n");
                }
            }
            else {
                tail = NULL;
                if (pthread_mutex_unlock(&queue_mutex) != 0) {
                    printf("mutex unlock after setting tail to NULL failed\n");
                }
                break;
            }
        }
    }
    return 0;
}

void WORKER_THREAD::queue_to_pool(worker_thread *node)
{
    while (node->status == STATUS_RETURNING)
    {
        if (pthread_mutex_trylock(&queue_mutex) != 0)
        {
            if(nanosleep(&req, &rem) != 0)
            {
                printf("nano sleep execution failed");
            }
        }
        else
        {
            //**queue the thread to the end of the list*/
            if (tail != NULL)
            {
                tail->next_thread = node;
            }
            // as a last node, node's next thread should be null
            node->next_thread = NULL;
            //** Make the new node as the last node */
            tail = node;
            // if thread comes back to thread pool of NULL, it should set head
            if (head->next_thread == NULL)
            {
                head->next_thread = node;
            }

            node->status = STATUS_SLEEPING;
            if (pthread_mutex_unlock(&queue_mutex) != 0)
            {
                printf("%d - queue mutex unlock failed \n", node->node_no);
            }
        }
    }
}

void signal_handler(int signum)
{
    if(signum == SIGPIPE)
    {
        printf("received SIGPIPE\n");
    }
    else
    {
        printf("RECEIVED SIGNAL OTHER THAN SIGPIPE!\n");
        exit(1);
    }
}

void *handle_thread_task(void *Node)
{
    void (*sigHandlerReturn)(int);
    sigHandlerReturn = signal(SIGPIPE, signal_handler);

    int detach_status;
    detach_status = pthread_detach(pthread_self());
    if (detach_status != 0)
    {
        printf("thread is not detached");
    }
    worker_thread *node = (worker_thread *)Node;

    // SSL
    SSL_CTX *ctx = InitCTX();
    SSL *ssl = SSL_new(ctx);
    node->ssl = ssl;

    readbuffer *rb = (readbuffer *) malloc (sizeof(readbuffer));
    rb->buf = (char *) malloc(RB_SIZE * sizeof(char));
    rb->head = rb->buf;
    rb->tail = rb->head;
    rb->data_len = 0;

    for (;;)
    {
        if (node->status == STATUS_RETURNING)
        {
            thread_pool->queue_to_pool(node);
        }

        pthread_mutex_lock(&node->condition_mutex);
        while (node->status == STATUS_SLEEPING) {
            pthread_cond_wait(&node->condition_cond, &node->condition_mutex);
        }
        pthread_mutex_unlock(&node->condition_mutex);

        // REQUEST HANDLING STARTS HERE
        if(node->status == STATUS_READY)
        {
            handle_request(node, rb);
        }
    }
}
void handle_backend(worker_thread *node, char* client_message, readbuffer * rb)
{
    int be_w_ret;
    int rbe_wc_ret;
    if(node->connection == INACTIVE)
    {
        if (SSL_clear(node->ssl) == FAIL)
        {
            printf("SSL_clear failed %s\n", strerror(errno));
        }
        node->backend_sd = create_source_endpoint(backend_port, backend_address, backend_hostname);

        SSL_set_fd(node->ssl, node->backend_sd);
        uset.insert(node->backend_sd);
        if (backend_connect(node->ssl) == SUCCESS)
        {
            be_w_ret = backend_write(node->ssl, client_message);
            if (be_w_ret == SUCCESS) {
                if (read_backend_write_client(node->ssl, node->client_sd, rb) == SUCCESS) {
                    node->connection = IDLE;
                }
                else {
                    node->connection = INACTIVE;
                    printf("ssl read backend  write client failed \n");
                }
            }
            else if (be_w_ret == RECONNECT) {
                close(node->backend_sd);
                node->connection = INACTIVE;
                handle_backend(node, client_message, rb);
            }
            else {
                node->connection = INACTIVE;
                printf("ssl backend  write failed \n");
            }
        }
        else {
            node->connection = INACTIVE;
            printf("ssl connect failed \n");
        }
    }
    else {
        be_w_ret = backend_write(node->ssl, client_message);
        if (be_w_ret == SUCCESS) {
            rbe_wc_ret = read_backend_write_client(node->ssl, node->client_sd, rb);
            if (rbe_wc_ret == SUCCESS) {
                node->connection = IDLE;
            }
            else if (rbe_wc_ret == RECONNECT) {
                close(node->backend_sd);
                node->connection = INACTIVE;
                handle_backend(node, client_message, rb);
            }
        }
        else if (be_w_ret == RECONNECT) {
            close(node->backend_sd);
            node->connection = INACTIVE;
            handle_backend(node, client_message, rb);
        }
        else {
            node->connection = INACTIVE;
            printf("ssl backend  write failed \n");
        }
    }
    if(node->connection == INACTIVE) {
        printf("failed transaction\n");
    }
}

void handle_request(worker_thread *node, readbuffer *rb) {
    char *client_message = read_from_client(node->client_sd);
    // NULL means unsuccessful reading from the client
    if (client_message != NULL)
    {
        handle_backend(node, client_message, rb);
        if(node->connection == INACTIVE)
        {
            shutdown(node->backend_sd, 2);
            close(node->backend_sd);
            uset.erase(node->backend_sd);
        }
    }
    node->status = STATUS_RETURNING;
    free(client_message);
    //check validatity because some clients might leave during the transaction.
    if(fd_is_valid(node->client_sd))
    {
        shutdown(node->client_sd, 2);
        if (close(node->client_sd) < 0) {
            printf("failed to close client side socket %s \n", strerror(errno));
        }
    }
}
