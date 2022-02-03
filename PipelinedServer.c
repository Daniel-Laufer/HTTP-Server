
#include <stdio.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h> 
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "pipelined_helpers.h"

#define NUM_THREADS 20


// Driver function
int main()
{
    int last_accepted = 0; 

    int listenfd, connfd;
    pthread_t threads[NUM_THREADS];
    

    // specifies a transport address and port for the AF_INET address family
    struct sockaddr_in servaddr;

    // creating a socket over domain: AF_INET (IPv4 protocol), SOCK_STREAM: TCP, protocol: 0 = IP
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    printf("Socket successfully created..\n");

    // fixing the socket bind failed issue
    int yes = 1;
    if((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) 
        perror("setsockopt");
    

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PIPELINED_SERVER_PORT
    servaddr.sin_family = AF_INET;

    // binding the socket to any possible IP address
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // setting up the port the server socket will be listening to
    servaddr.sin_port = htons(PIPELINED_SERVER_PORT);

    // binding newly created socket to the given IP address and port
    if ((bind(listenfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
    {
        printf("socket bind failed...\n");
        exit(0);
    }

    // now server is ready to listen for new connections
    if ((listen(listenfd, MAX_CONNECTIONS)) != 0)
    {
        printf("Listen failed...\n");
        exit(0);
    }
    printf("Server listening..\n");

    // flush whatever is in the stdout buffer
    fflush(stdout);

    // Accept the data packet from client and verification
    int i = 0; 
    while ((connfd = accept(listenfd, (SA *)NULL, NULL)) >= 0)
    {
        printf("server accept the client... %d\n", connfd);
        if (last_accepted != connfd) {
            int *connfd2 = (int *) malloc(sizeof(int));
            *connfd2 = connfd;
            pthread_create(&threads[i], NULL, pipelined_communication_with_client, connfd2);
        }
        i++;

        if (i == NUM_THREADS) {
            i = 0;
        }
    
        last_accepted = connfd; 
    }

    if (connfd < 0)
    {
        printf("server accept failed...\n");
        exit(0);
    }

    // After chatting close the socket
    close(listenfd);
}


