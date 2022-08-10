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
#include <map>

std::map<int, worker_thread*> hashmap;
void signal_handler(int signum);

WORKER_THREAD *thread_pool = new WORKER_THREAD;

void WORKER_THREAD::populate_thread_pool()
{
    head_inactive->next_thread = NULL;
    head_idle->next_thread = NULL;
    for (int i = 0; i < THREAD_NUMBER; i++)
    {
        WORKER_THREAD::push(&head_inactive, &tail_inactive, i);
    }
    if (tail_inactive->next_thread == NULL)
    {
#ifdef DEBUG
        printf("next thread of tail is NULL\n");
#endif
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
    if (*tail_inactive != NULL)
    {
        (*tail_inactive)->next_thread = newNode;
    }
    // /* Make the new node as the last node */
    *tail_inactive = newNode;
    newNode->next_thread = NULL;
    if ((*head_inactive)->next_thread == NULL)
    {
        /* Move the head to point to the new node */
        (*head_inactive)->next_thread = newNode;
    }
}
/** take thread from the front of the list
and assign task to it.
*/
void WORKER_THREAD::dequeue_from_idle_pool(worker_thread * node, int sockfd)
{
    #ifdef DEBUG
        printf("%d - dequeueing idle connection\n", head_idle->next_thread->node_no);
    #endif
    head_idle->next_thread->client_sd = sockfd;
    head_idle->next_thread->status = STATUS_READY;
    pthread_cond_signal(&head_idle->next_thread->condition_cond);
    head_idle->next_thread = head_idle->next_thread->next_thread;
    if(head_idle->next_thread != NULL)
    {
        #ifdef DEBUG
            printf("%d - thread pool head shifted to this\n", head_idle->next_thread->node_no);
        #endif
    }
    else
    {
        tail_idle = NULL;
        #ifdef DEBUG
            printf("thread pool head shifted to NULL\n");
        #endif
    }
}

void WORKER_THREAD::dequeue_from_inactive_pool(worker_thread * node, int sockfd)
{
    #ifdef DEBUG
        printf("%d - dequeueing INACTIVE connection\n", head_inactive->next_thread->node_no);
     #endif
    node->client_sd = sockfd;
    node->status = STATUS_READY;
    pthread_cond_signal(&node->condition_cond);
    head_inactive->next_thread = node->next_thread;
    if(head_inactive->next_thread != NULL)
    {
        #ifdef DEBUG
            printf("%d - thread pool head shifted to this\n", head_inactive->next_thread->node_no);
        #endif
    }
    else
    {
        tail_inactive = NULL;
        #ifdef DEBUG
            printf("thread pool head shifted to NULL\n");
        #endif
    }
}
int WORKER_THREAD::dequeue_worker(int sockfd)
{
    if(head_idle == NULL)
    {
        printf("head_idle is NULL\n");
    }
    if(head_idle->next_thread != NULL)
    {
        #ifdef DEBUG
             printf("trying to dequeue idle thread\n");
        #endif
        // the thread must be sleeping and only one of the two; backend connection closing thread or dequeueing thread
        // can wake up threads in the idle connection pool
        if(pthread_mutex_trylock(&(head_idle->next_thread->wakeup_mutex)) == 0)
        {
            dequeue_from_idle_pool(head_idle->next_thread, sockfd);
            return 1;
        }
    }
    if(head_inactive->next_thread != NULL)
    {
        if(pthread_mutex_trylock(&(head_inactive->next_thread->wakeup_mutex)) == 0){
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
            hashmap[node->backend_sd] = node;
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

            #ifdef DEBUG
                printf("queued worker to idle pool!\n");
            #endif

            #ifdef DEBUG
                printf("adding fd %d to Epoll\n",node->backend_sd);
            #endif
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
                #ifdef DEBUG
                    printf("%d - queue mutex unlock failed \n", node->node_no);
                #endif
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
            #ifdef DEBUG
                printf("queued %d thread to INACTIVE pool\n", node->node_no);
            #endif

            if (pthread_mutex_unlock(&queue_mutex) != 0)
            {
                #ifdef DEBUG
                    printf("%d - queue mutex unlock failed: \n", node->node_no);
                #endif
            }

        }
    }
}
void WORKER_THREAD::close_idle_connections(worker_thread *node)
{
    #ifdef DEBUG
        printf("closing backend connection %d\n", node->backend_sd);
    #endif
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, node->backend_sd, &ev) == -1) {
        perror("epoll_ctl: delete node backend connection socket descriptor");
    }
    shutdown(node->backend_sd, 2);
    if(close(node->backend_sd) == FAIL)
    {
        perror("closing backend sd after remote peer has closed their end\n");
    }
    //take the node out of the linked list
    if(node->prev_thread != NULL)
    {
        node->prev_thread->next_thread = node->next_thread;
        if(node == tail_idle)
        {
            if(head_idle->next_thread == node)
            {
                tail_idle = NULL;
            }
            else if(head_idle->next_thread != node)
            {
                tail_idle = node->prev_thread;
            }
        }
        //close client connection as well
        if(fd_is_valid(node->client_sd))
        {
            shutdown(node->client_sd, 2);
            if(close(node->client_sd) == FAIL)
            {
                perror("closing client sd after remote peer has closed their end\n");
            }
            else
            {
                printf("Client connection closed - %d\n", node->client_sd);
            }
        }
    }
    else
    {
        printf("NODE TO BE CLOSED IS NULL!!!!\n");
    }
}

void signal_handler(int signum)
{
    if(signum == SIGPIPE)
    {
        printf("received SIGPIPE");
    }
    else
    {
        printf("RECEIVED SIGNAL OTHER THAN SIGPIPE!");
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
        #ifdef DEBUG
            printf("thread is not detached");
        #endif
    }
    worker_thread *node = (worker_thread *)Node;

    // SSL
    SSL_CTX *ctx = InitCTX();
    SSL *ssl = SSL_new(ctx);
    node->ssl = ssl;

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
            if (node->status == STATUS_CLOSING_CONNECTION)
            {
                thread_pool->close_idle_connections(node);
                node->status = STATUS_RETURNING_TO_INACTIVE;
                thread_pool->queue_to_inactive_pool(node);
            }

            #ifdef DEBUG
                printf("%d - thread going into sleep\n", node->node_no);
            #endif
            if(pthread_mutex_unlock(&(node->wakeup_mutex)) != 0)
            {
                printf("unlocking wake up mutex after closing connection has failed \n");
            }
            pthread_cond_wait(&node->condition_cond, &node->condition_mutex);
            #ifdef DEBUG
                printf("%d - thread woke up to task\n", node->node_no);
            #endif
        }
        pthread_mutex_unlock(&node->condition_mutex);
        #ifdef DEBUG
            printf("%d thread is handling connection descriptor %d\n", node->node_no, node->client_sd);
        #endif

        // REQUEST HANDLING STARTS HERE
        if(node->status == STATUS_READY)
        {
            handle_request(node);
        }
    }
}
void handle_backend(worker_thread *node, char* client_message, bool reconnecting)
{
    int be_write;
    int be_r_client_w;
    if(node->pool_index == INACTIVE)
    {
        if (SSL_clear(node->ssl) == FAIL)
        {
            printf("SSL_clear failed\n");
        }
        node->backend_sd = Create_Backend_Connection(backend_port, backend_address, backend_hostname);
        #ifdef DEBUG
            printf("created backend connection socket %d\n", node->backend_sd);
        #endif
        SSL_set_fd(node->ssl, node->backend_sd);
        if (backend_connect(node->ssl) == FAIL)
        {
            shutdown(node->backend_sd, 2);
            close(node->backend_sd);
        }
    }
    be_write = backend_write(node->ssl, client_message);
    if(be_write == SUCCESS)
    {
        be_r_client_w = read_backend_write_client(node->ssl, node->client_sd);
        if(be_r_client_w == SUCCESS)
        {
            node->status = STATUS_RETURNING_TO_IDLE;
        }
        else if(be_r_client_w == RECONNECT)
        {
            if(reconnecting)
            {
                printf("RECONNECT returned twice BE_READ\n");
            }
            else
            {
                shutdown(node->backend_sd, 2);
                close(node->backend_sd);
                node->pool_index = INACTIVE;
                handle_backend(node, client_message, true);
            }
        }
        else
        {
             printf("FAIL BE_READ\n");
        }
    }
    else if(be_write == RECONNECT)
    {
        if(reconnecting)
        {
            printf("RECONNECT returned twice BE_WRITE\n");
        }
        else
        {
            shutdown(node->backend_sd, 2);
            close(node->backend_sd);
            node->pool_index = INACTIVE;
            handle_backend(node, client_message, true);
        }
    }
    else
    {
        printf("FAIL BE_WRITE\n");
    }
}

void handle_request(worker_thread *node){
    //always gravitate towards inactive pool
    node->status = STATUS_RETURNING_TO_INACTIVE;

    char *client_message = read_from_client(node->client_sd);
    if (client_message == NULL)
    {
        #ifdef DEBUG
            printf("%d - Error on the client side\n", node->node_no);
        #endif
    }
    else
    {
        handle_backend(node, client_message, false);
        if(node->status == STATUS_RETURNING_TO_INACTIVE)
        {
            shutdown(node->backend_sd, 2);
            close(node->backend_sd);
        }
        else
        {
            hashmap[node->backend_sd] = node;
        }
    }
    free(client_message);
    if(fd_is_valid(node->client_sd))
    {
        shutdown(node->client_sd, 2);
        if (close(node->client_sd) < 0)
        {
            #ifdef DEBUG
                printf("%d - FILE DESCRIPTOR not closed\n", node->node_no);
            #endif
            if (errno == EBADF)
            {
                printf("invalid client connection file descriptor - %d\n", node->client_sd);
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
    }
}



void* monitor_idle_connections(void*)
{
    worker_thread *tempNode;
    int bytes = 0;
    char buf[BACKEND_BUFFER] = {0};
    int n;
    for(;;)
    {
        n = epoll_wait(epollfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
        for(int i = 0 ; i < n; i++)
        {
            if((events[i].events & EPOLLRDHUP) && events[i].data.fd != BACKEND_SD)
            {
                #ifdef DEBUG
                    printf("event detected %d\n", events[i].data.fd);
                #endif
                tempNode = hashmap[events[i].data.fd];
                if(tempNode == NULL)
                {
                     printf("tempNode == NULL\n");
                     break;
                }
                if(pthread_mutex_trylock(&(tempNode->wakeup_mutex)) == 0)
                {
                    if(events[i].events & EPOLLIN)
                    {
                        read_backend_write_client(tempNode->ssl, tempNode->client_sd);
                        bytes = SSL_read(tempNode->ssl, buf, sizeof(buf));
                        printf("read %d bytes right before closing\n", bytes);
                    }
                    tempNode->status = STATUS_CLOSING_CONNECTION;
                    pthread_cond_signal(&(tempNode->condition_cond));
                }
            }
        }
    }
}
