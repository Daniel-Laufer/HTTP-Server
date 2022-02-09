
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
#include "server_helpers.h"
#include <time.h>


#define NUM_THREADS 20

/*
Responsible for communicating with client through a persistent HTTP connection
*/
void *persistent_communication_with_client(void *connfd);


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

    // assign IP, PERSIS_SERVER_PORT
    servaddr.sin_family = AF_INET;

    // binding the socket to any possible IP address
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // setting up the port the server socket will be listening to
    servaddr.sin_port = htons(PERSIS_SERVER_PORT);

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
            pthread_create(&threads[i], NULL, persistent_communication_with_client, connfd2);
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


void *persistent_communication_with_client(void *arg)
{
    int connfd = *(int *)arg;

    char printbuf[200];
    sprintf(printbuf, "[persistent_communication_with_client] %d enter function\n", connfd);
    log_to_file(printbuf);

    char content_length_header[MAX];
    char date_header[32];
    long file_size;
    char response[1000];
    char *connection_timeout_header = "Keep-Alive: timeout=10, max=1000";
    char *connection_keep_alive_header = "Connection: keep-alive";
    char *connection_close_header = "Connection: close";
    time_t now = time(NULL);
    char *website_dir = "website/";
    int website_dir_length = strlen(website_dir);

    FILE *fp;
    char *fname;
    char *fpath;
    char buff[MAX];
    char buff2[MAX];
    char writebuff[MAX];
    int n;

    while (1)
    {
        sprintf(printbuf, "[persistent_communication_with_client] %d Entering while(1) loop\n", connfd);
        log_to_file(printbuf);

        // Wait for input on the connfd socket.
        // If there is input after 10 seconds, close the connectrion.

        struct pollfd fds;
        fds.fd = connfd;
        fds.events = POLLIN;
        int ret = poll(&fds, 1, 20000);
        if (ret == 0)
        {
            sprintf(printbuf, "[persistent_communication_with_client] %d closing connection, no values from poll\n", connfd);
            log_to_file(printbuf);
            goto close_connection;
        }

        sprintf(printbuf, "[persistent_communication_with_client] got through poll %d \n", connfd);
        log_to_file(printbuf);

        // read the message from client and copy it in buffer
        int check = 0;
        while ((n = read(connfd, buff2, MAX - 1)) > 0)
        {
            // unsupported HTTP method provided
            if (is_http_method_get(buff2))
            {

                fname = extract_fname(buff2);
                sprintf(printbuf, "[persistent_communication_with_client] Get request is received %d File: %s\n", connfd, fname);
                log_to_file(printbuf);
                check = 1;
                memcpy(buff, buff2, MAX);
            }

            else
            {
                sprintf(printbuf, "[persistent_communication_with_client] Remaining buff characters received %d\n", connfd);
                log_to_file(printbuf);
            }

            // hacky way to detect the end of the message.
            printf("%s\n", buff2);
            if (buff2[n - 1] == '\n')
            {
                if (check == 0)
                {
                    sprintf(printbuf, "I Reached the end but didn't find a file %d\n", connfd);
                    log_to_file(printbuf);
                    log_to_file(buff2);
                    goto close_connection;
                }
                break;
            }
        }

        sprintf(printbuf, "[persistent_communication_with_client] finished first while loop %d\n", connfd);
        log_to_file(printbuf);

        if (fname == NULL)
        {
            log_to_file("fname was null... exiting");
            goto close_connection;
        }

        if (strlen(fname) == 0)
        {
            free(fname);
            fname = malloc(sizeof(char) * 11);
            memcpy(fname, "index.html", 11);
            fname[10] = '\0';
        }

        //appending path with fname
        fpath = malloc(sizeof(char) * (website_dir_length + strlen(fname) + 1));
        memcpy(fpath, website_dir, website_dir_length);
        memcpy(fpath + website_dir_length, fname, strlen(fname));
        fpath[website_dir_length + strlen(fname)] = '\0';
        printf("%s\n", fpath);

        if (fpath[strlen(fpath) - 1] == '/')
        {
            /* TODO: restrict users from entering paths such as localhost:8090/assets/ */
        }


        // opening file for reading
        if (!(fp = fopen(fpath, "rb")))
        {
            // free(fpath);
            perror("error in reading file");
            // generating response headers
            char body[] = "<h1 style='text-align: center;'>File not found</h1>";

            char *http_status_code = "HTTP/1.0 404 Not found";

            strftime(date_header, 32, "Date: %a, %d %b %Y %H:%M:%S", gmtime(&now));
            date_header[31] = '\0';

            int num_printed = sprintf(response, "%s\r\n%s\r\n%s\r\n\r\n%s", http_status_code, connection_close_header, date_header, body);
            response[num_printed] = '\0';
            send_response(connfd, writebuff, response);
            goto close_connection;
        }

        // if an If-Modified-Since header is provided then conditionally return a 304 Not Modified status with no body.
        if (!has_requested_file_been_modified_since(fpath, buff))
        {
            // generating response headers
            char *http_status_code = "HTTP/1.0 304 Not Modified";

            strftime(date_header, 32, "Date: %a, %d %b %Y %H:%M:%S", gmtime(&now));
            date_header[31] = '\0';

            sprintf(content_length_header, "Content-length: 0");

            int num_printed = sprintf(response, "%s\r\n%s\r\n%s\r\n%s\r\n\r\n", http_status_code, connection_close_header, content_length_header, date_header); // no body sent for 304 response
            response[num_printed] = '\0';
            // end generating response headers

            send_response(connfd, writebuff, response);
            goto close_connection;
        }
        else
        {
            // generating response headers
            char *http_status_code = "HTTP/1.0 200 Success";

            strftime(date_header, 32, "Date: %a, %d %b %Y %H:%M:%S", gmtime(&now));
            date_header[31] = '\0';

            char content_type_header[30];
            sprintf(content_type_header, "Content-type: %s", extract_ftype(fpath));

            char content_length_header[MAX];

            file_size = get_file_size(fp);
            sprintf(content_length_header, "Content-length: %ld", file_size);

            int num_printed = sprintf(response, "%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n\r\n", http_status_code, connection_keep_alive_header, connection_timeout_header, date_header, content_length_header, content_type_header);
            response[num_printed] = '\0';
            // end generating response headers

            send_response(connfd, writebuff, response);

            // Sending the body of the request

            if (send_file(fp, fpath, connfd) < 0)
            {
                goto close_connection; 
            }
    
            // free(fname);
 
        }
    }

close_connection:
    // free(fpath);
    // free(fname);
    close(connfd);
    pthread_exit(NULL);
}