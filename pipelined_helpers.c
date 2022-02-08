#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include "pipelined_helpers.h"
#include "server_helpers.h"
#define __USE_XOPEN
#include <time.h>
#include <pthread.h>
#include <poll.h>

#define MAX_FNAME 100
#define MAX_RESPONSE 1000

struct arg_struct
{
    int connfd;
    int priority;
    char buff[MAX];
    char fname[MAX_FNAME];
};

pthread_mutex_t response_mutex;
pthread_cond_t sending_cv;  
int turn = 1; 


void *pipelined_communication_with_client(void *arg)
{
    log_to_file("Starting pipelined communication\n");
    int connfd = *(int *)arg;

    char fname[MAX_FNAME];
    char buff[MAX];
    char buff2[MAX];
    int n;
    int priority = 1; 

    while (1)
    {
        // Wait for input on the connfd socket.
        // If there is input after 10 seconds, close the connectrion.
        pthread_t thread;
        struct pollfd fds;
        fds.fd = connfd;
        fds.events = POLLIN;
        int ret = poll(&fds, 1, 20000);
        if (ret == 0)
        {
            goto close_connection;
        }

        // read the message from client and copy it in buffer
        int check = 0;
        while ((n = read(connfd, buff2, MAX - 1)) > 0)
        {
            if (is_http_method_get(buff2))
            {
                strcpy(fname, extract_fname(buff2));
                check = 1;
                memcpy(buff, buff2, MAX);
            }

            // hacky way to detect the end of the message.
            printf("%s\n", buff2);
            if (buff2[n - 1] == '\n')
            {
                if (check == 0)
                {
                    goto close_connection;
                }
                break;
            }
        }

        // Create a thread to handle the request
        struct arg_struct args;
        args.connfd = connfd;
        args.priority = priority;
        strncpy(args.fname, fname, MAX_FNAME);
        strncpy(args.buff, buff, MAX);
        pthread_create(&thread, NULL, handle_request, &args);
        priority++;
    }

close_connection:
    close(connfd);
    pthread_exit(NULL);
}

void *handle_request(void *arg)
{
    // Parsing arguments 
    struct arg_struct args = *(struct arg_struct *)arg;
    int connfd = args.connfd;
    int priority = args.priority; 


    char buff[MAX];
    char *fname = (char *)malloc(MAX_FNAME);
    strcpy(buff, args.buff);
    strcpy(fname, args.fname);

    log_to_file("Enter handle_Request, filename is: ");
    log_to_file(fname);

    // Variable Definitions
    char content_length_header[MAX];
    char *fpath;
    char writebuff[MAX];
    char date_header[32];
    long file_size;
    char response[MAX_RESPONSE];
    char *connection_timeout_header = "Keep-Alive: timeout=60, max=1000";
    char *connection_keep_alive_header = "Connection: keep-alive";
    char *connection_close_header = "Connection: close";
    time_t now = time(NULL);
    char *website_dir = "website/";
    int website_dir_length = strlen(website_dir);

    FILE *fp;

    if (fname == NULL)
    {
        goto end_request;
    }

    if (strlen(fname) == 0)
    {
        log_to_file("Length of filename detected to be 0\n");
        strcpy(fname, "index.html");
        fname[10] = '\0';
    }

    //appending path with fname
    fpath = malloc(website_dir_length + strlen(fname) + 1);
    memcpy(fpath, website_dir, website_dir_length);
    memcpy(fpath + website_dir_length, fname, strlen(fname));
    fpath[website_dir_length + strlen(fname)] = '\0';
    printf("fpath: %s\n\n", fpath);

    // opening file for reading
    if (!(fp = fopen(fpath, "rb")))
    {
        printf("error in reading file");

        // generating response headers
        char body[] = "<h1 style='text-align: center;'>File not found</h1>";

        char *http_status_code = "HTTP/1.0 404 Not found";

        strftime(date_header, 32, "Date: %a, %d %b %Y %H:%M:%S", gmtime(&now));
        date_header[31] = '\0';

        int num_printed = sprintf(response, "%s\r\n%s\r\n%s\r\n\r\n%s", http_status_code, connection_close_header, date_header, body);
        response[num_printed] = '\0';
        send_response(connfd, writebuff, response);
        goto end_request;
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
        send_response(connfd, writebuff, response);
        goto end_request;
    }
    else
    {
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
        
        pthread_mutex_lock(&response_mutex);
        while (priority != turn) {
            pthread_cond_broadcast(&sending_cv);
            pthread_cond_wait(&sending_cv, &response_mutex);
        }
        send_response(connfd, writebuff, response);
        send_file(fp, fpath, connfd);
        turn++; 
        pthread_cond_broadcast(&sending_cv);
        pthread_mutex_unlock(&response_mutex);
    }

end_request:
    pthread_exit(NULL);
}

