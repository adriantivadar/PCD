#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include<pthread.h>

#define PORT "4567"  // port
#define BACKLOG 10   //  connections                   

struct clients {
    int fd;
    struct clients *next;    
};

struct clients * clients = NULL;

void add_to_clients(int fd) {
    printf("adding %d\n", fd);
    // create a new client node 
    struct clients *new_cli = malloc(sizeof(struct clients));
    if (new_cli == NULL) {
        fprintf(stderr, "add_to_clients: No more memory!");
        return;    
    }
    new_cli->fd = fd;
    // add the node to the start of the list
    new_cli->next = clients;
    clients = new_cli;
}

void broadcast(char *message, int from_fd) {
    struct clients *temp;
    for (temp = clients; temp != NULL; temp = temp->next) {
        int ret = write(temp->fd, message, strlen(message) + 1);
        printf("Broadcasting to %d, ret=%d\n", temp->fd, ret);

    }
}

void *connection_handler(void *socket) {
    int new_fd = *(int *)socket;
    char chat[1024];
    char message[1200];
    int i;

    printf("Got connection\n");

    add_to_clients(new_fd);

    while (1) {
        ssize_t message_len = read(new_fd, chat, 1024);
        if (message_len == -1) {
            break;
        }
        else {
            sprintf(message, "<%d>: %s", new_fd, chat);
            printf("%s\n", message);
        }
        broadcast(message, new_fd);
    }
}

void remove_from_clients(int fd) {
    // search for the node in the list
    struct clients *temp;
    struct clients *prev = NULL;
    for (temp = clients; temp != NULL; prev = temp, temp = temp->next) {
        if (temp->fd == fd) {
            // found
            break;
        }
    }
    if (prev == NULL) { 
        clients = temp->next;    
    }
    else {
        prev->next = temp->next;
    }
    free(temp);
}

// get sockaddr
void *get_in_addr(struct sockaddr *sa)
{
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int sockfd, new_fd;    
    struct addrinfo hints, 
                    *servinfo, 
                    *p;
    char server_ip[INET6_ADDRSTRLEN];
    char client_ip[INET6_ADDRSTRLEN];
    struct sockaddr_in6 connectors_address; 
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        void *addr;
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) p->ai_addr;
        addr = &(ipv6->sin6_addr);
        inet_ntop(p->ai_family, addr, server_ip, sizeof server_ip);
        printf("Server IP: %s, Port: %s\n", server_ip, PORT);
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
                perror("server: socket\n");
                continue;        
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                perror("server: bind");
                continue;        
        }
        break;
    }

    freeaddrinfo(servinfo); 

    if (p == NULL) { 
        fprintf(stderr, "failed to bind\n");
        return 1;    
    }

    if (listen(sockfd, BACKLOG) == -1) { 
        perror("listen");
        return 1;    
    }
    
    pthread_t thread_id;
    // connections
    while (1) {
        sin_size = sizeof connectors_address;
        new_fd = accept(sockfd, (struct sockaddr *)&connectors_address, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        if( pthread_create( &thread_id , NULL ,  connection_handler, (void*) &new_fd) < 0)
        {
            perror("could not create thread");
            return 1;
        }
    }
    close(sockfd);
    return 0;
}