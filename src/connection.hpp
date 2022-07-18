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
class CONNECTION_POOL;
std::map<int, struct connection_node*> hashmap;

struct connection_node
{
    int client_sd;
    int backend_sd;
    SSL *ssl;
    struct connection_node *next;
    struct connection_node *left;
};

class CONNECTION_POOL
{
    /**
    new thread will be inserted to the end of the linked list
    and the thread for the task will be taken out from the front of the list
    */
public:
    struct connection_node *head = (struct connection_node *) malloc(
        2*sizeof(int) + 2*sizeof(connection_node*) + sizeof(SSL*)
    );
    struct connection_node *temp = NULL;
    struct connection_node *tail = NULL;
    struct connection_node *get_connection_node();
    int check_availability();
    void clean_expired(int fd);
    void queue(connection_node *node);
};
struct connection_node *CONNECTION_POOL::get_connection_node()
{
    printf("getting connection node\n");
    if(head->next == NULL)
    {
        return NULL;
    }
    temp = head -> next;
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, temp->backend_sd, &ev) == -1) {
        perror("epoll_ctl: EPOLL_CTL_DEL has failed\n");
    }
    head -> next = temp -> next;
    printf("returning connection node\n");
    return temp;
}

int CONNECTION_POOL::check_availability()
{
    if(head->next == NULL)
    {
        return NO_CONNECTION_AVAILABLE;
    }
    return CONNECTION_AVAILABLE;
}
        
void CONNECTION_POOL::clean_expired(int fd)
{
    // printf("fd %d\n", fd);
    // if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev) == -1) {
    //     perror("epoll_ctl: EPOLL_CTL_DEL during cleaning has failed\n");
    // }
    // printf("hashmap[fd]->left->next = hashmap[fd]->next;\n");
    // if(hashmap[fd] == NULL)
    // {
    //     printf("hashmap[fd]->left == NULL\n");
    // }
    // else if (hashmap[fd]->left->next == NULL)
    // {
    //      printf("hashmap[fd]->left->next == NULL\n");
    // }
    // else
    // {
    //     hashmap[fd]->left->next = hashmap[fd]->next;
    // }
    // printf("SSL_free(hashmap[fd]->ssl);\n");
    // SSL_free(hashmap[fd]->ssl);
    // printf("free(hashmap[fd]);\n");
    // free(hashmap[fd]);
    // printf("close(fd);\n");
    // close(fd);
    if(head->next == NULL)
    {
        //do nothing
        return;
    }
    temp = head -> next;
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, temp->backend_sd, &ev) == -1) {
        perror("epoll_ctl: EPOLL_CTL_DEL has failed\n");
    }
    head -> next = temp -> next;
    SSL_free(temp->ssl);
    free(temp);
    printf("returning connection node\n");
}
void CONNECTION_POOL::queue(connection_node *node)
{
    if(head->next == NULL)
    {
        head->next = node;
        node->left = head;
    }
    else{
        node->left = tail;
    }
    if(tail != NULL){
        tail->next = node;
    }
    node->next = NULL;
    tail = node;
}

CONNECTION_POOL *connection_pool = new CONNECTION_POOL;