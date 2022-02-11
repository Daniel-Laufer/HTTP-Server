
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
#include <errno.h>
#include <time.h>

#define NUM_THREADS 20

struct thread_args
{
    char *root_path;
    int connfd;
};

/*
Responsible for communicating with client through a persistent HTTP connection
*/
void *persistent_communication_with_client(void *args);

// Driver function
int main(int argc, char *argv[])
{
    int last_accepted = 0;

    int listenfd, connfd;
    char *ptr;
    int16_t server_port;
    char *root_path;
    errno = 0;
    pthread_t threads[NUM_THREADS];

    // specifies a transport address and port for the AF_INET address family
    struct sockaddr_in servaddr;

    if (argc != 3)
    {
        printf("Usage: ./persistent_server <port> <root_path>\n");
        exit(0);
    }
    else
    {
        server_port = strtol(argv[1], &ptr, 10);
        root_path = argv[2];

        // if an invalid port or root_path was passed
        if (errno != 0)
        {
            perror("strtol");
            exit(0);
        }
    }

    // building thread aregument struct

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
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1)
        perror("setsockopt");

    bzero(&servaddr, sizeof(servaddr));

    // setting up the connection
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

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

    struct thread_args *persis_thread_args = malloc(sizeof(struct thread_args));
    persis_thread_args->root_path = root_path;

    // Accept the data packet from client and verification
    int i = 0;
    while ((connfd = accept(listenfd, (SA *)NULL, NULL)) >= 0)
    {
        printf("server accept the client... %d\n", connfd);
        if (last_accepted != connfd)
        {
            // int *connfd2 = (int *)malloc(sizeof(int));
            // *connfd2 = connfd;
            persis_thread_args->connfd = connfd;
            pthread_create(&threads[i], NULL, persistent_communication_with_client, persis_thread_args);
        }
        i++;

        if (i == NUM_THREADS)
            i = 0;

        last_accepted = connfd;
    }

    if (connfd < 0)
    {
        printf("server accept failed...\n");
        exit(0);
    }

    // After chatting close the socket
    free(persis_thread_args);
    close(listenfd);
}

void *persistent_communication_with_client(void *args)
{
    // extracting args passed
    struct thread_args *persis_thread_args = args;
    int connfd = persis_thread_args->connfd;
    char *root_path = persis_thread_args->root_path;

    // defining required variables to serve a client
    char content_length_header[MAX], date_header[32], response[1000];
    long file_size;
    char *connection_timeout_header = "Keep-Alive: timeout=10, max=1000";
    char *connection_keep_alive_header = "Connection: keep-alive";
    char *connection_close_header = "Connection: close";
    time_t now = time(NULL);
    int root_path_length = strlen(root_path);

    FILE *fp;
    char *fname, *fpath;
    char buff[MAX], buff2[MAX], writebuff[MAX];
    int n;

    // looping until the client is done requesting
    while (1)
    {
        // Wait for input on the connfd socket
        // If there is input after 10 seconds, close the connection
        struct pollfd fds;
        fds.fd = connfd;
        fds.events = POLLIN;
        int ret = poll(&fds, 1, 10000);
        if (ret == 0)
            goto close_connection;

        // read the message from client and copy it in buffer
        int check = 0;
        int total_read = 0;
        while ((n = read(connfd, buff2, MAX - 1)) > 0)
        {
            total_read += n;
            // unsupported HTTP method provided
            if (is_http_method_get(buff2))
            {
                fname = extract_fname(buff2);
                check = 1;
                memcpy(buff, buff2, MAX);
            }

            // Detect the end of the message.
            if (buff2[n - 1] == '\n')
            {
                if (check == 0)
                    goto close_connection;
                break;
            }
        }

        // if nothing was read or fname failed to be malloc'ed
        if (total_read == 0 || fname == NULL)
            goto close_connection;

        // if requesting root
        if (strlen(fname) == 0)
        {
            free(fname);
            fname = malloc(sizeof(char) * 11);
            memcpy(fname, "index.html", 11);
            fname[10] = '\0';
        }

        // appending path with fname
        if (root_path[strlen(root_path) - 1] != '/') // adding a slash at the end
        {
            fpath = malloc(sizeof(char) * (root_path_length + 1 + strlen(fname) + 1));
            memcpy(fpath, root_path, root_path_length);
            memcpy(fpath + root_path_length, "/", 1);

            memcpy(fpath + root_path_length + 1, fname, strlen(fname));
            fpath[root_path_length + 1 + strlen(fname)] = '\0';
        }
        else // not need to append slash at the end
        {
            fpath = malloc(sizeof(char) * (root_path_length + strlen(fname) + 1));
            memcpy(fpath, root_path, root_path_length);
            memcpy(fpath + root_path_length, fname, strlen(fname));
            fpath[root_path_length + strlen(fname)] = '\0';
        }

        // opening file for reading
        if (!(fp = fopen(fpath, "rb")) || fpath[strlen(fpath) - 1] == '/')
        {
            free(fpath);
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
            free(fpath);
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
                goto close_connection;
        }

        free(fpath);
    }

close_connection:
    free(fname);
    close(connfd);
    pthread_exit(NULL);
}