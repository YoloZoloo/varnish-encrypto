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
#include <sys/epoll.h>
#endif
#include <unordered_set>

std::unordered_set<int> uset;
void signal_handler(int signum);
WORKER_THREAD *thread_pool = new WORKER_THREAD;

void WORKER_THREAD::populate_thread_pool()
{
    head_inactive->next_thread = NULL;
    head_idle->next_thread = NULL;
    for (int i = 0; i < THREAD_NUMBER; i++) {
        WORKER_THREAD::push(&head_inactive, &tail_inactive, i);
    }
}
// insert a new node in front of the list
void WORKER_THREAD::push(worker_thread **head_inactive, worker_thread **tail_inactive, int node_number)
{
    worker_thread *newNode = (worker_thread *) malloc(
        2 * sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) +
        2 * sizeof(worker_thread) + sizeof(pthread_t) +
        5 * sizeof(int) + sizeof(SSL*)
    );

    /* Put in the data  */
    pthread_mutex_init(&(newNode->condition_mutex), NULL);
    pthread_cond_init(&(newNode->condition_cond), NULL);
    pthread_mutex_init(&(newNode->wakeup_mutex), NULL);
    newNode->client_sd = 0;
    newNode->status = STATUS_INITIAL;
    newNode->node_no = node_number;
    newNode->pool_index = INACTIVE;
    newNode->prev_thread = NULL;
    pthread_create(&(newNode->thread), NULL, &handle_thread_task, (void *)(newNode));

    // // /* Link the last node to the new node */
    if (*tail_inactive != NULL) {
        (*tail_inactive)->next_thread = newNode;
    }
    // /* Make the new node as the last node */
    *tail_inactive = newNode;
    newNode->next_thread = NULL;

    /* Move the head to point to the new node */
    if ((*head_inactive)->next_thread == NULL) {
        (*head_inactive)->next_thread = newNode;
    }
}
/** take thread from the front of the list
and assign task to it.
*/
void WORKER_THREAD::dequeue_from_idle_pool(worker_thread * node, int sockfd)
{
    head_idle->next_thread->client_sd = sockfd;
    head_idle->next_thread->status = STATUS_READY;
    pthread_cond_signal(&head_idle->next_thread->condition_cond);
    head_idle->next_thread = head_idle->next_thread->next_thread;

    if(head_idle->next_thread == NULL) {
        tail_idle = NULL;
    }
}

void WORKER_THREAD::dequeue_from_inactive_pool(worker_thread * node, int sockfd)
{
    node->client_sd = sockfd;
    node->status = STATUS_READY;
    pthread_cond_signal(&node->condition_cond);
    head_inactive->next_thread = node->next_thread;
    if(head_inactive->next_thread == NULL) {
        tail_inactive = NULL;
    }
}

int val_fd (int fd)
{
    printf("fd: %d\n", fd);
    if (uset.find(fd) == uset.end()) {
        printf("not a valid fd \n");
        return 0;
    }
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev) == -1) {
        printf("EPOLL_CTL_DEL failed %s \n", strerror(errno));
    }
    return 1;
}

int WORKER_THREAD::dequeue_worker(int sockfd)
{
    if(head_idle == NULL) {
        printf("head_idle is NULL\n");
    }
    if(head_idle->next_thread != NULL) {
        // the thread must be sleeping and only one of the two; backend connection closing thread or dequeueing thread
        // can wake up threads in the idle connection pool
        if(pthread_mutex_trylock(&(head_idle->next_thread->wakeup_mutex)) == 0) {
            printf("backend socket file descriptor %d \n", head_idle->next_thread->backend_sd);
            if (val_fd (head_idle->next_thread->backend_sd) == 0) {
                head_idle->next_thread->pool_index = INACTIVE;
            }
            dequeue_from_idle_pool(head_idle->next_thread, sockfd);
            return 1;
        }
    }
    if(head_inactive->next_thread != NULL)
    {
        if(pthread_mutex_trylock(&(head_inactive->next_thread->wakeup_mutex)) == 0) {
            dequeue_from_inactive_pool(head_inactive->next_thread, sockfd);
            return 1;
        }
    }
    return 0;
}

void WORKER_THREAD::queue_to_idle_pool(worker_thread *node)
{
    while (node->status == STATUS_RETURNING_TO_IDLE)
    {
        if (pthread_mutex_trylock(&idle_queue_mutex) != 0)
        {
            if(nanosleep(&req, &rem) != 0)
            {
                printf("nano sleep execution failed");
            }
        }
        else
        {
            //**queue the thread to the end of the list*/
            if (tail_idle != NULL)
            {
                tail_idle->next_thread = node;
                node->prev_thread = tail_idle;
            }
            // as a last node, node's next thread should be null
            node->next_thread = NULL;
            //** Make the new node as the last node */
            tail_idle = node;
            // if thread comes back to thread pool of NULL, it should set head
            if (head_idle->next_thread == NULL)
            {
                head_idle->next_thread = node;
                node->prev_thread = head_idle;
            }
            node->pool_index = IDLE;

            ev.data.fd = node->backend_sd;
            if (epoll_ctl(epollfd, EPOLL_CTL_ADD, node->backend_sd, &ev) == -1) {
                if(errno != EEXIST)
                {
                    perror("epoll_ctl: add node backend connection socket descriptor");
                }
            }

            node->status = STATUS_SLEEPING;
            if (pthread_mutex_unlock(&idle_queue_mutex) != 0)
            {
                printf("%d - queue mutex unlock failed \n", node->node_no);
            }
        }
    }
}
void WORKER_THREAD::queue_to_inactive_pool(worker_thread *node)
{
    while(node->status == STATUS_RETURNING_TO_INACTIVE)
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
            if (tail_inactive != NULL)
            {
                tail_inactive->next_thread = node;
            }
            // as a last node, node's next thread should be null
            node->next_thread = NULL;
            //** Make the new node as the last node */
            tail_inactive = node;
            // if thread comes back to thread pool of NULL, it should set head
            if (head_inactive->next_thread == NULL)
            {
                head_inactive->next_thread = node;
            }
            //queueing back to sleep
            node->status = STATUS_SLEEPING;
            node->pool_index  = INACTIVE;

            if (pthread_mutex_unlock(&queue_mutex) != 0)
            {
                printf("%d - queue mutex unlock failed: \n", node->node_no);
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

    while (node->status == STATUS_INITIAL)
    {
        pthread_mutex_lock(&node->condition_mutex);
        pthread_cond_wait(&node->condition_cond, &node->condition_mutex);
        pthread_mutex_unlock(&node->condition_mutex);
    }

    for (;;)
    {
        pthread_mutex_lock(&node->condition_mutex);
        while (node->status != STATUS_READY)
        {
            if (node->status == STATUS_RETURNING_TO_INACTIVE)
            {
                thread_pool->queue_to_inactive_pool(node);
            }
            if (node->status == STATUS_RETURNING_TO_IDLE)
            {
                thread_pool->queue_to_idle_pool(node);
            }

            if(pthread_mutex_unlock(&(node->wakeup_mutex)) != 0)
            {
                printf("unlocking wake up mutex after completing thread task failed \n");
            }

            pthread_cond_wait(&node->condition_cond, &node->condition_mutex);
        }
        pthread_mutex_unlock(&node->condition_mutex);

        // REQUEST HANDLING STARTS HERE
        if(node->status == STATUS_READY)
        {
            handle_request(node);
        }
    }
}
void handle_backend(worker_thread *node, char* client_message)
{
    int be_w_ret;
    int rbe_wc_ret;
    if(node->pool_index == INACTIVE)
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
                if (read_backend_write_client(node->ssl, node->client_sd) == SUCCESS) {
                    printf("successful transaction, be_fd: %d, client_f: %dd\n",
                        SSL_get_fd(node->ssl), node->client_sd);
                    node->status = STATUS_RETURNING_TO_IDLE;
                }
                else {
                    printf("ssl read backend  write client failed \n");
                }
            }
            else if (be_w_ret == RECONNECT) {
                printf("trying to reconnect \n");
                node->pool_index = INACTIVE;
                handle_backend(node, client_message);
            }
            else {
                printf("ssl backend  write failed \n");
            }
        }
        else {
            printf("ssl connect failed \n");
        }
    }
    else {
        be_w_ret = backend_write(node->ssl, client_message);
        if (be_w_ret == SUCCESS) {
            rbe_wc_ret = read_backend_write_client(node->ssl, node->client_sd);
            if (rbe_wc_ret == SUCCESS) {
                node->status = STATUS_RETURNING_TO_IDLE;
            }
            else if (rbe_wc_ret == RECONNECT) {
                printf("trying to reconnect rbe_wc\n");
                node->pool_index = INACTIVE;
                handle_backend(node, client_message);
            }
        }
        else if (be_w_ret == RECONNECT) {
            printf("trying to reconnect w_be\n");
            node->pool_index = INACTIVE;
            handle_backend(node, client_message);
        }
        else {
            printf("ssl backend  write failed \n");
        }
    }
    if(node->status == STATUS_RETURNING_TO_INACTIVE) {
        printf("failed transaction\n");
    }
}

void handle_request(worker_thread *node) {
    //always gravitate towards inactive pool
    node->status = STATUS_RETURNING_TO_INACTIVE;

    char *client_message = read_from_client(node->client_sd);
    // NULL means unsuccessful reading from the client
    if (client_message != NULL)
    {
        handle_backend(node, client_message);
        if(node->status == STATUS_RETURNING_TO_INACTIVE)
        {
            shutdown(node->backend_sd, 2);
            close(node->backend_sd);
            uset.erase(node->backend_sd);
        }
    }
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

void* monitor_idle_connections(void*)
{
    int n;
    for(;;)
    {
        n = epoll_wait(epollfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
        for(int i = 0 ; i < n; i++)
        {
            if((events[i].events & EPOLLRDHUP) && events[i].data.fd != BACKEND_SD)
            {
                printf("event detected, closing fd %d\n", events[i].data.fd);
                if (epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, &ev) == -1) {
                    printf("epoll_ctl: delete failed %s", strerror(errno));
                }
                if (close(events[i].data.fd) != 0) {
                    printf("failed closing backend socket on epoll event %s\n", strerror (errno));
                }
                uset.erase(events[i].data.fd);
            }
        }
    }
}
