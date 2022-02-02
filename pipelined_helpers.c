

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
#define __USE_XOPEN
#include <time.h>
#include <pthread.h>
#include <poll.h>

#define MAX_FNAME 100
#define MAX_RESPONSE 1000


struct arg_struct {
    int connfd;
    char buff[MAX];
    char fname[MAX_FNAME];
};


/* implementation of the helper functions */
void log_to_file(char *message);

void log_to_file(char *message) {
   FILE *logger = fopen("logfile.txt", "a");
   fputs(message, logger);
   fclose(logger);
}


void *pipelined_communication_with_client(void *arg)
{
    int connfd = *(int *)arg;
    
    char fname[MAX_FNAME];
    char buff[MAX];
    char buff2[MAX];
    int n;

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
            if (buff2[n - 1] == '\n') {
                if (check == 0) {
                    goto close_connection;
                }
                break;
            }
        }

        // Create a thread to handle the request
        struct arg_struct args;
        args.connfd = connfd;
        strncpy(args.fname, fname, MAX_FNAME);
        strncpy(args.buff, buff, MAX);
        pthread_create(&thread, NULL, handle_request, &args);   
    }

close_connection:
    close(connfd);
    pthread_exit(NULL);
}

void *handle_request(void *arg) {
    struct arg_struct args = *(struct arg_struct *) arg;
    
    int connfd = args.connfd;
    char buff[MAX];
    char *fname = (char *) malloc(MAX_FNAME);
    strcpy(buff, args.buff);
    strcpy(fname, args.fname);

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

    if (fname == NULL) {
        goto end_request;
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

        send_response(connfd, writebuff, response);
        send_file(fp, fpath, connfd);
    }

end_request:
    pthread_exit(NULL);
}


/**
 * Return 1 if the client sent a GET HTTP request, 0 otherwise.
 */
int is_http_method_get(char *request)
{
    char request_type[4];
    memcpy(request_type, &request[0], 3);
    request_type[3] = '\0';

    if ((strncmp(request_type, "GET", 3)) == 0)
        return 1;

    return 0; // unsupported HTTP method;
}

/**
 * Returns the value of the If-Modified-Since Header (a date represented by a string)
 */
struct tm *_extract_modified_since_value(char *request_info)
{
    char date_time_str[25];
    int modified_since_key_length = 17;
    int modified_since_value_length = 25;

    for (int i = 0; i < MAX - modified_since_key_length; i++)
    {
        if (strncmp(request_info + i, "If-Modified-Since", modified_since_key_length) == 0)
        {
            strncpy(date_time_str, request_info + i + modified_since_key_length + 2, modified_since_value_length);
            struct tm *tm = malloc(sizeof(struct tm));
            char *ret;
            if ((ret = strptime(date_time_str, "%a, %d %b %Y %H:%M:%S", tm)) == NULL)
            {
                free(tm);
                return NULL; // a return of NULL means a conversion failed. Fail silently.
            }
            return tm;
        }
    }
    return NULL;
}

/**
 * Returns the the file name given the <request_info>
 * sent from the client. Remember to free return value once
 * there is no use to it any longer.
 */
char *extract_fname(char *request_info)
{
    // start from index 4 since the beginning will be "GET "
    int i = 4, size = 0;

    // loop until we reach " HTTP"
    while (1)
    {
        // as soon as we get to HTTP/ we know we finished parsing the filename
        if (request_info[i + 1] == '/')
            break;

        i++;
        size++;
    }

    // subtracting " HTTP" from file name
    size -= 5;

    char *fname = malloc(sizeof(char) * (size + 1));
    memcpy(fname, &request_info[5], size);
    fname[size] = '\0';
    return fname;
}

/**
 * Returns the content-type value based on the given
 * file name <fname>.
 */
const char *extract_ftype(char *fname)
{
    char *rest = strrchr(fname, '.');

    if (strcmp(rest, ".html") == 0)
    {
        return "text/html";
    }

    else if (strcmp(rest, ".css") == 0)
    {
        return "text/css";
    }

    else if (strcmp(rest, ".js") == 0)
    {
        return "text/javascript";
    }

    else if (strcmp(rest, ".jpg") == 0)
    {
        return "image/jpeg";
    }

    return "text/plain";
}

/*
Return the size of this file (number of bytes).
*/
long get_file_size(FILE *fp)
{
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return size;
}

/**
 * Given two integers <a> and <b>, returns
 * the minimum of the two.
 */
int min(int a, int b)
{
    if (a > b)
        return b;
    return a;
}

/** 
 * Sends a file given a pointer to that file, with file name <fname> to target socket <tarsocket>.
 * Upon an error, -1 is returned. Otherwise, if the operation was successful, 
 * 0 is returned.
 */
int send_file(FILE *fp, char *fname, int tarsocket)
{
    // determining the file's size first
    long bytes_left = get_file_size(fp);
    int read_bytes;
    char buff[512];
    int actual_sent = 0;

    // while there are no bytes left to be sent
    while (bytes_left > 0)
    {
        // reading and sending one byte a time to ensure
        // no data loss occurs
        int min_to_send = min(sizeof(buff), bytes_left);
        read_bytes = fread(buff, sizeof(char), min_to_send, fp);
        if (read_bytes <= 0 || read_bytes < min_to_send)
        {
            perror("send_file");
            return -1;
        }

        actual_sent = send(tarsocket, buff, read_bytes, 0);
        bytes_left -= actual_sent;

        // not everything was sent
        if (actual_sent < min_to_send)
        {
            // go back to retry send
            fseek(fp, -(min_to_send - actual_sent), SEEK_CUR);
        }
    }

    fclose(fp);
    return 0;
}

/*
Returns 1 if the file with the path <fname> has been modified since
the date-time specified in the If-Modified-Since Header *or* if the
If-Modified-Since Header isn't present *or* its value is incorrectly formatted
(failing silently). 
*/
int has_requested_file_been_modified_since(char *fname, char *buff)
{
    struct tm *if_modified_since_date_time = _extract_modified_since_value(buff);
    if (if_modified_since_date_time == NULL)
        return 1;

    struct stat attr;
    stat(fname, &attr);

    // creating a tm struct for the file's modification time and then convert it to unix time
    struct tm *file_last_modified_date = localtime(&(attr.st_ctime));
    time_t raw_file_last_modified_date = mktime(file_last_modified_date);

    // converting if_modified_since_date_time into unix time
    time_t raw_if_modified_since_date_time = mktime(if_modified_since_date_time);
    free(if_modified_since_date_time); // don't need this if_modified_since_date_time tm struct any longer

    // file HAS been modified since the value of  the If-Modified-Since header
    if (raw_file_last_modified_date >= raw_if_modified_since_date_time)
        return 1;

    return 0;
}

/*
Sends a response back to the client and conditionally drops the connection. If successful, return a 1, otherwise 0. 
*/
int send_response(int connfd, char *writebuff, char *message_to_write)
{
    printf("===================");
    printf("\n\nSending following response to client:\n%s\n", message_to_write);
    write(connfd, message_to_write, strlen((char *)message_to_write));
    printf("===================\n");
    return 1;
}
